#ifndef COCONEXT_DYN_UNSIGNED_HPP
#define COCONEXT_DYN_UNSIGNED_HPP

#include <coconext/types/bits.hpp>
#include <coconext/types/concepts.hpp>
#include <coconext/types/dyn_bits.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/range.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>

namespace coconext::types::detail {

class DynSigned;

class DynUnsigned {
    template <typename T>
    T to_native_int() const {
        auto val = value_;
        if (val.ugt(DynBits(val.get_width(), std::numeric_limits<T>::max()))) {
            throw std::out_of_range("Value too large for destination native type");
        }

        return static_cast<T>(val.raw().word(0));
    }

  public:
    DynUnsigned(DynBits const& val) : value_(val) {}

    size_t get_width() const { return value_.get_width(); }

    // Construct from a native integer.
    template <NativeInteger T>
    DynUnsigned(size_t width, T v) : value_(width, v) {
        if constexpr (std::is_signed_v<T>) {
            if (v < 0) {
                throw std::overflow_error("negative value in Unsigned construction");
            }
        }

        if (std::numeric_limits<T>::digits > width) {
            using unsigned_T = std::make_unsigned_t<T>;
            unsigned_T max_unsigned = (width >= sizeof(unsigned_T) * 8)
                                        ? static_cast<unsigned_T>(-1)
                                        : (static_cast<unsigned_T>(1) << width) - 1;

            if (static_cast<unsigned_T>(v) > max_unsigned) {
                throw std::overflow_error("value does not fit in Unsigned width");
            }
        }
    }

    bool operator==(DynUnsigned const& rhs) const noexcept { return value_ == rhs.value_; }

    bool operator<(DynUnsigned const& rhs) const noexcept { return value_.ult(rhs.value_); }

    bool operator<=(DynUnsigned const& rhs) const noexcept {
        return value_.ule(rhs.value_);
    }

    bool operator>(DynUnsigned const& rhs) const noexcept { return value_.ugt(rhs.value_); }

    bool operator>=(DynUnsigned const& rhs) const noexcept {
        return value_.uge(rhs.value_);
    }

    explicit operator bool() const noexcept {
        return value_ != DynBits{value_.get_width(), 0};
    }

    explicit operator long long() const { return to_native_int<long long>(); }
    explicit operator unsigned long long() const { return to_native_int<long long>(); }

    template <typename ShiftType>
    DynUnsigned operator<<(ShiftType const& shift_amount) const {
        using CleanType = std::remove_cvref_t<ShiftType>;

        static_assert(
            std::is_integral_v<CleanType> || std::is_same_v<CleanType, DynUnsigned>
                || std::is_same_v<CleanType, DynSigned>,
            "Shift amount can only be a native integer, DynSigned, or Unsigned"
        );

        size_t safe_shift = 0;

        if constexpr (std::is_integral_v<CleanType>) {
            if constexpr (std::is_signed_v<CleanType>) {
                if (shift_amount < 0) {
                    throw std::invalid_argument("Negative shift amount");
                }
            }
            safe_shift = static_cast<size_t>(shift_amount);
        } else if constexpr (std::is_same_v<CleanType, DynUnsigned>) {
            if (shift_amount.get_width() > 64) {
                throw std::out_of_range("Bit Width cap 2**64");
            }
            safe_shift = static_cast<size_t>(static_cast<unsigned long long>(shift_amount));
        } else if constexpr (std::is_same_v<CleanType, DynSigned>) {
            if (shift_amount.get_width() > 64) {
                throw std::out_of_range("Bit Width cap 2**64");
            }
            long long signed_val = static_cast<long long>(shift_amount);
            if (signed_val < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
            safe_shift = static_cast<size_t>(signed_val);
        }

        if (safe_shift >= get_width()) {
            return DynUnsigned(get_width(), 0);
        }

        return DynUnsigned(value_ << safe_shift);
    }

    template <typename ShiftType>
    DynUnsigned operator>>(ShiftType const& shift_amount) const {
        using CleanType = std::remove_cvref_t<ShiftType>;
        static_assert(
            std::is_integral_v<CleanType> || std::is_same_v<CleanType, DynUnsigned>
                || std::is_same_v<CleanType, DynSigned>,
            "Shift amount can only be a native integer, DynSigned, or Unsigned"
        );

        size_t safe_shift = 0;

        if constexpr (std::is_integral_v<CleanType>) {
            if constexpr (std::is_signed_v<CleanType>) {
                if (shift_amount < 0) {
                    throw std::invalid_argument("Negative shift amount");
                }
            }
            safe_shift = static_cast<size_t>(shift_amount);
        } else if constexpr (std::is_same_v<CleanType, DynUnsigned>) {
            if (shift_amount.get_width() > 64) {
                throw std::out_of_range("Bit Width cap 2**64");
            }
            safe_shift = static_cast<size_t>(static_cast<unsigned long long>(shift_amount));
        } else if constexpr (std::is_same_v<CleanType, DynSigned>) {
            if (shift_amount.get_width() > 64) {
                throw std::out_of_range("Bit Width cap 2**64");
            }
            long long signed_val = static_cast<long long>(shift_amount);
            if (signed_val < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
            safe_shift = static_cast<size_t>(signed_val);
        }

        if (safe_shift >= get_width()) {
            return DynUnsigned(get_width(), 0);
        }

        return DynUnsigned(value_.srl(safe_shift));
    }

    template <typename ShiftType>
    constexpr DynUnsigned& operator<<=(ShiftType const& shift_amount) {
        *this = *this << shift_amount;
        return *this;
    }

    template <typename ShiftType>
    constexpr DynUnsigned& operator>>=(ShiftType const& shift_amount) {
        *this = *this >> shift_amount;
        return *this;
    }

    auto operator+(DynUnsigned const& rhs) const {
        return DynUnsigned(add_unsigned(value_, rhs.value_));
    }

    auto operator*(DynUnsigned const& rhs) const {
        return DynUnsigned(mul_unsigned(value_, rhs.value_));
    }

    auto operator/(DynUnsigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynUnsigned(div_unsigned(value_, rhs.value_));
    }

    auto operator%(DynUnsigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynUnsigned(rem_unsigned(value_, rhs.value_));
    }

    auto operator+=(DynUnsigned const& rhs) {
        value_ = add_unsigned(value_, rhs.value_).truncate(get_width());
        return *this;
    }

    auto operator-=(DynUnsigned const& rhs) {
        value_ = sub_unsigned(value_, rhs.value_).truncate(get_width());
        return *this;
    }

    auto operator*=(DynUnsigned const& rhs) {
        value_ = mul_unsigned(value_, rhs.value_).truncate(get_width());
        return *this;
    }

    auto operator/=(DynUnsigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        value_ = div_unsigned(value_, rhs.value_).truncate(get_width());
        return *this;
    }

    auto operator%=(DynUnsigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        value_ = rem_unsigned(value_, rhs.value_).zero_extend(get_width());
        return *this;
    }

    template <NativeInteger T>
    auto operator+=(T const& rhs) {
        *this += DynUnsigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    template <NativeInteger T>
    auto operator-=(T const& rhs) {
        *this -= DynUnsigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    template <NativeInteger T>
    auto operator*=(T const& rhs) {
        *this *= DynUnsigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    template <NativeInteger T>
    auto operator/=(T const& rhs) {
        *this /= DynUnsigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    template <NativeInteger T>
    auto operator%=(T const& rhs) {
        *this %= DynUnsigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    friend DynUnsigned& operator+=(DynUnsigned& lhs, DynSigned const& rhs);
    friend DynUnsigned& operator-=(DynUnsigned& lhs, DynSigned const& rhs);
    friend DynUnsigned& operator*=(DynUnsigned& lhs, DynSigned const& rhs);
    friend DynUnsigned& operator/=(DynUnsigned& lhs, DynSigned const& rhs);
    friend DynUnsigned& operator%=(DynUnsigned& lhs, DynSigned const& rhs);

    auto index(Range::value_type index) const {
        if (index >= static_cast<Range::value_type>(get_width()) || index < 0) {
            throw std::out_of_range("Out of bounds access in DynSigned.index()");
        }
        return value_.get_bit(index);
    }

  private:
    friend struct bits_fn;
    DynBits value_;
};

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_UNSIGNED_HPP

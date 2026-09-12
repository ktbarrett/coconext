#ifndef COCONEXT_DYN_UNSIGNED_HPP
#define COCONEXT_DYN_UNSIGNED_HPP

#include <algorithm>
#include <coconext/types/concepts.hpp>
#include <coconext/types/dyn_int_base.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/range.hpp>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace coconext::types::detail {

class DynSigned;

class DynUnsigned {
    template <typename T>
    T to_native_int() const {
        return value_.template to_native_integer<T>();
    }

  public:
    explicit DynUnsigned(DynUInt val) : value_(std::move(val)) {}
    explicit DynUnsigned(DynSInt val) : value_(std::move(val)) {}
    explicit DynUnsigned(std::string_view str, size_t width) : value_(str, width) {}

    size_t size() const { return value_.size(); }

    // Construct from a native integer.
    template <NativeInteger T>
    DynUnsigned(T v, size_t width) : value_(width) {
        if (width == 0) {
            throw std::invalid_argument("DynUnsigned(0) has no integer representation");
        }
        if (!native_value_fits<false>(width, v)) {
            throw std::overflow_error("value does not fit in Unsigned width");
        }
        value_ = DynUInt(v, width);
    }

    template <HasDynamicStorage Target>
    [[nodiscard]] Target as() && {
        auto const range = int_downto_range(value_.size());
        return adopt_storage<Target>(range, std::move(value_));
    }

    [[nodiscard]] auto as() && noexcept {
        return reinterpreted<DynUnsigned>(std::move(*this));
    }

    bool operator==(DynUnsigned const& rhs) const noexcept { return value_ == rhs.value_; }

    bool operator<(DynUnsigned const& rhs) const { return compare_value(rhs) < 0; }

    bool operator<=(DynUnsigned const& rhs) const { return compare_value(rhs) <= 0; }

    bool operator>(DynUnsigned const& rhs) const { return compare_value(rhs) > 0; }

    bool operator>=(DynUnsigned const& rhs) const { return compare_value(rhs) >= 0; }

    explicit operator bool() const noexcept { return value_.popcount() != 0; }

    explicit operator long long() const { return to_native_int<long long>(); }
    explicit operator unsigned long long() const {
        return to_native_int<unsigned long long>();
    }

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
            if (static_cast<unsigned long long>(shift_amount)
                > std::numeric_limits<unsigned long long>::max())
            {
                throw std::out_of_range("Bit Width cap 2**64");
            }
            safe_shift = static_cast<size_t>(static_cast<unsigned long long>(shift_amount));
        } else if constexpr (std::is_same_v<CleanType, DynSigned>) {
            if (static_cast<long long>(shift_amount)
                > std::numeric_limits<long long>::max())
            {
                throw std::out_of_range("Bit Width cap 2**64");
            }
            long long signed_val = static_cast<long long>(shift_amount);
            if (signed_val < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
            safe_shift = static_cast<size_t>(signed_val);
        }

        if (safe_shift >= size()) {
            return DynUnsigned(0, size());
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
            if (static_cast<unsigned long long>(shift_amount)
                > std::numeric_limits<unsigned long long>::max())
            {
                throw std::out_of_range("Bit Width cap 2**64");
            }
            safe_shift = static_cast<size_t>(static_cast<unsigned long long>(shift_amount));
        } else if constexpr (std::is_same_v<CleanType, DynSigned>) {
            if (static_cast<long long>(shift_amount)
                > std::numeric_limits<long long>::max())
            {
                throw std::out_of_range("Bit Width cap 2**64");
            }
            long long signed_val = static_cast<long long>(shift_amount);
            if (signed_val < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
            safe_shift = static_cast<size_t>(signed_val);
        }

        if (safe_shift >= size()) {
            return DynUnsigned(0, size());
        }

        return DynUnsigned(value_ >> safe_shift);
    }

    auto operator|(DynUnsigned const& other) const {
        return DynUnsigned(value_ | storage(other));
    }

    auto operator&(DynUnsigned const& other) const {
        return DynUnsigned(value_ & storage(other));
    }

    auto operator^(DynUnsigned const& other) const {
        return DynUnsigned(value_ ^ storage(other));
    }

    auto operator~() const { return DynUnsigned(~value_); }

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
        return DynUnsigned(value_ + rhs.value_);
    }

    auto operator*(DynUnsigned const& rhs) const {
        return DynUnsigned(value_ * rhs.value_);
    }

    auto operator/(DynUnsigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynUnsigned(value_ / rhs.value_);
    }

    auto operator%(DynUnsigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynUnsigned(value_ % rhs.value_);
    }

    auto operator+=(DynUnsigned const& rhs) {
        value_ = DynUInt(value_ + rhs.value_, size());
        return *this;
    }

    auto operator-=(DynUnsigned const& rhs) {
        value_ = DynUInt(value_ - rhs.value_, size());
        return *this;
    }

    auto operator*=(DynUnsigned const& rhs) {
        value_ = DynUInt(value_ * rhs.value_, size());
        return *this;
    }

    auto operator/=(DynUnsigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        value_ = DynUInt(value_ / rhs.value_, size());
        return *this;
    }

    auto operator%=(DynUnsigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        value_ = DynUInt(value_ % rhs.value_, size());
        return *this;
    }

    template <NativeInteger T>
    auto operator+=(T const& rhs) {
        *this += DynUnsigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    template <NativeInteger T>
    auto operator-=(T const& rhs) {
        *this -= DynUnsigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    template <NativeInteger T>
    auto operator*=(T const& rhs) {
        *this *= DynUnsigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    template <NativeInteger T>
    auto operator/=(T const& rhs) {
        *this /= DynUnsigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    template <NativeInteger T>
    auto operator%=(T const& rhs) {
        *this %= DynUnsigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    friend DynUnsigned& operator+=(DynUnsigned& lhs, DynSigned const& rhs);
    friend DynUnsigned& operator-=(DynUnsigned& lhs, DynSigned const& rhs);
    friend DynUnsigned& operator*=(DynUnsigned& lhs, DynSigned const& rhs);
    friend DynUnsigned& operator/=(DynUnsigned& lhs, DynSigned const& rhs);
    friend DynUnsigned& operator%=(DynUnsigned& lhs, DynSigned const& rhs);

    auto begin() noexcept { return value_.begin(); }
    auto begin() const noexcept { return value_.begin(); }
    auto end() noexcept { return value_.end(); }
    auto end() const noexcept { return value_.end(); }
    auto rbegin() noexcept { return value_.rbegin(); }
    auto rbegin() const noexcept { return value_.rbegin(); }
    auto rend() noexcept { return value_.rend(); }
    auto rend() const noexcept { return value_.rend(); }

    auto index(Range::value_type index) const {
        if (index >= static_cast<Range::value_type>(size()) || index < 0) {
            throw std::out_of_range("Out of bounds access in DynSigned.index()");
        }
        return value_.get_bit(index);
    }

  private:
    int compare_value(DynUnsigned const& rhs) const {
        size_t const compare_width = std::max(size(), rhs.size());
        auto lhs_value = DynUInt(value_, compare_width);
        auto rhs_value = DynUInt(rhs.value_, compare_width);
        return lhs_value < rhs_value ? -1 : rhs_value < lhs_value ? 1 : 0;
    }

    friend struct storage_fn;
    DynUInt value_;
};

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_UNSIGNED_HPP

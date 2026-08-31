#ifndef COCONEXT_DYN_SIGNED_HPP
#define COCONEXT_DYN_SIGNED_HPP

#include <algorithm>
#include <coconext/types/dyn_int_base.hpp>
#include <coconext/types/dyn_unsigned.hpp>
#include <utility>

namespace coconext::types::detail {

class DynUnsigned;

class DynSigned {
    template <typename T>
    T to_native_int() const {
        return value_.template to_native_integer<T>();
    }

  public:
    explicit DynSigned(DynSInt val) : value_(std::move(val)) {}
    explicit DynSigned(size_t width, std::string_view str) : value_(width, str) {}

    size_t width() const { return value_.width(); }

    DynSigned(Vector<Bit> const& v) : value_(v.size()) {
        size_t bit_idx = 0;
        for (auto it = v.rbegin(); it != v.rend(); ++it) {
            value_.set_bit(bit_idx++, char(*it) == '1');
        }
    }

    // Construct from a native integer.
    template <NativeInteger T>
    DynSigned(size_t width, T v) : value_(width) {
        if (width == 0) {
            throw std::invalid_argument("DynSigned(0) has no integer representation");
        }
        if (!native_value_fits<true>(width, v)) {
            throw std::overflow_error("value does not fit in Signed width");
        }
        value_ = DynSInt(width, v);
    }

    bool operator==(DynSigned const& rhs) const noexcept {
        return value_ == rhs.value_ && width() == rhs.width();
    }

    bool operator<(DynSigned const& rhs) const { return compare_value(rhs) < 0; }

    bool operator<=(DynSigned const& rhs) const { return compare_value(rhs) <= 0; }

    bool operator>(DynSigned const& rhs) const { return compare_value(rhs) > 0; }

    bool operator>=(DynSigned const& rhs) const { return compare_value(rhs) >= 0; }

    explicit operator bool() const noexcept { return value_.popcount() != 0; }

    explicit operator long long() const { return to_native_int<long long>(); }

    template <typename ShiftType>
    DynSigned operator<<(ShiftType const& shift_amount) const {
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

        if (safe_shift >= width()) {
            return DynSigned(width(), 0);
        }

        return DynSigned(value_ << safe_shift);
    }

    template <typename ShiftType>
    DynSigned operator>>(ShiftType const& shift_amount) const {
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

        if (safe_shift >= width()) {
            if (safe_shift > 0) {
                return DynSigned(width(), -1);
            } else {
                return DynSigned(width(), 0);
            }
        }

        return DynSigned(value_ >> safe_shift);
    }

    template <typename ShiftType>
    constexpr DynSigned& operator<<=(ShiftType const& shift_amount) {
        *this = *this << shift_amount;
        return *this;
    }

    template <typename ShiftType>
    constexpr DynSigned& operator>>=(ShiftType const& shift_amount) {
        *this = *this >> shift_amount;
        return *this;
    }

    auto operator|(DynSigned const& other) const { return DynSigned(value_ | bits(other)); }

    auto operator&(DynSigned const& other) const { return DynSigned(value_ & bits(other)); }

    auto operator^(DynSigned const& other) const { return DynSigned(value_ ^ bits(other)); }

    auto operator~() const { return DynSigned(~value_); }

    auto operator+() const { return *this; }

    auto operator-() const { return DynSigned(-value_); }

    auto operator+(DynSigned const& rhs) const { return DynSigned(value_ + rhs.value_); }

    auto operator-(DynSigned const& rhs) const { return DynSigned(value_ - rhs.value_); }

    auto operator*(DynSigned const& rhs) const { return DynSigned(value_ * rhs.value_); }

    auto operator/(DynSigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynSigned(value_ / rhs.value_);
    }

    auto operator%(DynSigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynSigned(value_ % rhs.value_);
    }

    auto operator+=(DynSigned const& rhs) {
        value_ = DynSInt(width(), value_ + rhs.value_);
        return *this;
    }

    auto operator-=(DynSigned const& rhs) {
        value_ = DynSInt(width(), value_ - rhs.value_);
        return *this;
    }

    auto operator*=(DynSigned const& rhs) {
        value_ = DynSInt(width(), value_ * rhs.value_);
        return *this;
    }

    auto operator/=(DynSigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }

        value_ = DynSInt(width(), value_ / rhs.value_);
        return *this;
    }

    auto operator%=(DynSigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }

        value_ = DynSInt(width(), value_ % rhs.value_);
        return *this;
    }

    template <NativeInteger T>
    auto operator+=(T const& rhs) {
        *this += DynSigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    template <NativeInteger T>
    auto operator-=(T const& rhs) {
        *this -= DynSigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    template <NativeInteger T>
    auto operator*=(T const& rhs) {
        *this *= DynSigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    template <NativeInteger T>
    auto operator/=(T const& rhs) {
        *this /= DynSigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    template <NativeInteger T>
    auto operator%=(T const& rhs) {
        *this %= DynSigned(std::numeric_limits<T>::digits, rhs);
        return *this;
    }

    auto index(Range::value_type index) const {
        if (index >= static_cast<Range::value_type>(width()) || index < 0) {
            throw std::out_of_range("Out of bounds access in DynSigned.index()");
        }
        return value_.get_bit(index);
    }

    friend DynSigned& operator+=(DynSigned& lhs, DynUnsigned const& rhs);
    friend DynSigned& operator-=(DynSigned& lhs, DynUnsigned const& rhs);
    friend DynSigned& operator*=(DynSigned& lhs, DynUnsigned const& rhs);
    friend DynSigned& operator/=(DynSigned& lhs, DynUnsigned const& rhs);
    friend DynSigned& operator%=(DynSigned& lhs, DynUnsigned const& rhs);

  private:
    int compare_value(DynSigned const& rhs) const {
        size_t const compare_width = std::max(width(), rhs.width());
        auto lhs_value = DynSInt(compare_width, value_);
        auto rhs_value = DynSInt(compare_width, rhs.value_);
        return lhs_value < rhs_value ? -1 : rhs_value < lhs_value ? 1 : 0;
    }

    friend struct bits_fn;
    DynSInt value_;
};

inline DynSigned rem(DynSigned const& lhs, DynSigned const& rhs) { return lhs % rhs; }

inline DynSigned mod(DynSigned const& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }
    return DynSigned(mod(bits(lhs), bits(rhs)));
}

// DynUnsigned Unary operators
inline DynSigned operator+(DynUnsigned const& lhs) {
    return DynSigned(DynSInt(lhs.width() + 1, bits(lhs)));
}

inline DynSigned operator-(DynUnsigned const& lhs) { return DynSigned(-bits(lhs)); }

// DynUnsigned operator-
inline DynSigned operator-(DynUnsigned const& lhs, DynUnsigned const& rhs) {
    return DynSigned(bits(lhs) - bits(rhs));
}

// DynUnsigned X DynSigned compound operators
inline DynUnsigned& operator+=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto result = DynSInt(lhs.width(), bits(lhs)) + bits(rhs);
    lhs.value_ = DynUInt(lhs.width(), result);
    return lhs;
}

inline DynUnsigned& operator-=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto result = DynSInt(lhs.width(), bits(lhs)) - bits(rhs);
    lhs.value_ = DynUInt(lhs.width(), result);
    return lhs;
}

inline DynUnsigned& operator*=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto result = DynSInt(lhs.width(), bits(lhs)) * bits(rhs);
    lhs.value_ = DynUInt(lhs.width(), result);
    return lhs;
}

inline DynUnsigned& operator/=(DynUnsigned& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.width() + 1, rhs.width());
    auto lhs_positive = DynSInt(safe_width, bits(lhs));

    auto quotient = lhs_positive / bits(rhs);
    lhs.value_ = DynUInt(lhs.width(), quotient);

    return lhs;
}

inline DynUnsigned& operator%=(DynUnsigned& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.width() + 1, rhs.width());
    auto lhs_positive = DynSInt(safe_width, bits(lhs));

    auto remainder = lhs_positive % bits(rhs);
    lhs.value_ = DynUInt(lhs.width(), remainder);

    return lhs;
}

// DynSigned X DynUnsigned compound operators
inline DynSigned& operator+=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = DynSInt(rhs.width() + 1, bits(rhs));
    auto result = bits(lhs) + rhs_positive;
    lhs.value_ = DynSInt(lhs.width(), result);
    return lhs;
}

inline DynSigned& operator-=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = DynSInt(rhs.width() + 1, bits(rhs));
    auto result = bits(lhs) - rhs_positive;
    lhs.value_ = DynSInt(lhs.width(), result);
    return lhs;
}

inline DynSigned& operator*=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = DynSInt(rhs.width() + 1, bits(rhs));
    auto result = bits(lhs) * rhs_positive;
    lhs.value_ = DynSInt(lhs.width(), result);
    return lhs;
}

inline DynSigned& operator/=(DynSigned& lhs, DynUnsigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.width() + 1, rhs.width());
    auto lhs_ext = DynSInt(safe_width, bits(lhs));
    auto rhs_positive = DynSInt(rhs.width() + 1, bits(rhs));

    auto quotient = lhs_ext / rhs_positive;
    lhs.value_ = DynSInt(lhs.width(), quotient);

    return lhs;
}

inline DynSigned& operator%=(DynSigned& lhs, DynUnsigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.width(), rhs.width()) + 1;

    auto lhs_ext = DynSInt(safe_width, bits(lhs));
    auto rhs_positive = DynSInt(safe_width, bits(rhs));

    auto remainder = lhs_ext % rhs_positive;
    lhs.value_ = DynSInt(lhs.width(), remainder);

    return lhs;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_SIGNED_HPP

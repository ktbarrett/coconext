#ifndef COCONEXT_DYN_SIGNED_HPP
#define COCONEXT_DYN_SIGNED_HPP

#include <coconext/types/dyn_bits.hpp>
#include <coconext/types/dyn_unsigned.hpp>

namespace coconext::types::detail {

class DynUnsigned;

class DynSigned {
    template <typename T>
    T to_native_int() const {
        constexpr size_t target_width = sizeof(T) * 8;

        if (!fits_signed(value_.cref(), target_width)) {
            throw std::out_of_range("Value does not fit in destination native signed type");
        }

        if (value_.width() < target_width) {
            auto extended = value_.sign_extend(target_width);
            return static_cast<T>(extended.raw().word(0));
        } else {
            auto truncated = value_.truncate(target_width);
            return static_cast<T>(truncated.raw().word(0));
        }
    }

  public:
    explicit DynSigned(DynBits const& val) : value_(val) {}

    size_t width() const { return value_.width(); }

    DynSigned(Vector<Bit> const& v) : value_(v.size()) {
        size_t bit_idx = 0;
        for (auto it = v.rbegin(); it != v.rend(); ++it) {
            value_.set_bit(bit_idx++, char(*it) == '1');
        }
    }

    // Construct from a native integer.
    template <NativeInteger T>
    DynSigned(size_t width, T v) : value_(width, v) {
        if (width >= std::numeric_limits<T>::digits) {
            return;
        }

        if constexpr (std::is_unsigned_v<T>) {
            T max_val = (static_cast<T>(1) << (width - 1)) - 1;
            if (v > max_val) {
                throw std::overflow_error("Unsigned value too large for Signed width");
            }
        } else {
            T max_val = (static_cast<T>(1) << (width - 1)) - 1;
            T min_val = -(static_cast<T>(1) << (width - 1));

            if (v > max_val || v < min_val) {
                throw std::overflow_error("Signed value does not fit in provided width");
            }
        }
    }

    bool operator==(DynSigned const& rhs) const noexcept {
        return value_ == rhs.value_ && width() == rhs.width();
    }

    auto operator<(DynSigned const& rhs) const noexcept { return value_.slt(rhs.value_); }

    auto operator<=(DynSigned const& rhs) const noexcept { return value_.sle(rhs.value_); }

    auto operator>(DynSigned const& rhs) const noexcept { return value_.sgt(rhs.value_); }

    auto operator>=(DynSigned const& rhs) const noexcept { return value_.sge(rhs.value_); }

    explicit operator bool() const noexcept { return value_ != DynBits{value_.width(), 0}; }

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

        return DynSigned(value_.sra(safe_shift));
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

    auto operator+() const { return *this; }

    auto operator-() const { return DynSigned(sub_signed(DynBits(width(), 0), value_)); }

    auto operator+(DynSigned const& rhs) const {
        return DynSigned(add_signed(value_, rhs.value_));
    }

    auto operator-(DynSigned const& rhs) const {
        return DynSigned(sub_signed(value_, rhs.value_));
    }

    auto operator*(DynSigned const& rhs) const {
        return DynSigned(mul_signed(value_, rhs.value_));
    }

    auto operator/(DynSigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynSigned(div_signed(value_, rhs.value_));
    }

    auto operator%(DynSigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynSigned(mod_signed(value_, rhs.value_));
    }

    auto operator+=(DynSigned const& rhs) {
        value_ = add_signed(value_, rhs.value_).truncate(width());
        return *this;
    }

    auto operator-=(DynSigned const& rhs) {
        value_ = sub_signed(value_, rhs.value_).truncate(width());
        return *this;
    }

    auto operator*=(DynSigned const& rhs) {
        value_ = mul_signed(value_, rhs.value_).truncate(width());
        return *this;
    }

    auto operator/=(DynSigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }

        value_ = div_signed(value_, rhs.value_).truncate(width());
        return *this;
    }

    auto operator%=(DynSigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }

        value_ = mod_signed(value_, rhs.value_).sign_extend(width());
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
    friend struct bits_fn;
    DynBits value_;
};

// DynUnsigned Unary operators
inline DynSigned operator+(DynUnsigned const& lhs) {
    return DynSigned(bits(lhs).zero_extend(lhs.width() + 1));
}

inline DynSigned operator-(DynUnsigned const& lhs) {
    return DynSigned(sub_unsigned(DynBits(lhs.width(), 0), bits(lhs)));
}

// DynUnsigned operator-
inline DynSigned operator-(DynUnsigned const& lhs, DynUnsigned const& rhs) {
    return DynSigned(sub_unsigned(bits(lhs), bits(rhs)));
}

// DynUnsigned X DynSigned compound operators
inline DynUnsigned& operator+=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto temp = DynSigned(bits(lhs));
    lhs.value_ = std::move(bits(temp += rhs));
    return lhs;
}

inline DynUnsigned& operator-=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto temp = DynSigned(bits(lhs));
    lhs.value_ = std::move(bits(temp -= rhs));
    return lhs;
}

inline DynUnsigned& operator*=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto temp = DynSigned(bits(lhs));
    lhs.value_ = std::move(bits(temp *= rhs));
    return lhs;
}

inline DynUnsigned& operator/=(DynUnsigned& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.width() + 1, rhs.width());
    auto lhs_positive = bits(lhs).zero_extend(safe_width);

    auto quotient = div_signed(lhs_positive, bits(rhs));
    lhs.value_ = quotient.truncate(lhs.width());

    return lhs;
}

inline DynUnsigned& operator%=(DynUnsigned& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.width() + 1, rhs.width());
    auto lhs_positive = bits(lhs).zero_extend(safe_width);

    auto remainder = rem_signed(lhs_positive, bits(rhs));
    lhs.value_ = remainder.truncate(lhs.width());

    return lhs;
}

// DynSigned X DynUnsigned compound operators
inline DynSigned& operator+=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = detail::bits(rhs).zero_extend(rhs.width() + 1);
    auto result = detail::add_signed(detail::bits(lhs), rhs_positive);
    lhs.value_ = result.truncate(lhs.width());
    return lhs;
}

inline DynSigned& operator-=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = detail::bits(rhs).zero_extend(rhs.width() + 1);
    auto result = detail::sub_signed(detail::bits(lhs), rhs_positive);
    lhs.value_ = result.truncate(lhs.width());
    return lhs;
}

inline DynSigned& operator*=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = detail::bits(rhs).zero_extend(rhs.width() + 1);
    auto result = detail::mul_signed(detail::bits(lhs), rhs_positive);
    lhs.value_ = result.truncate(lhs.width());
    return lhs;
}

inline DynSigned& operator/=(DynSigned& lhs, DynUnsigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.width() + 1, rhs.width());
    auto lhs_ext = bits(lhs).sign_extend(safe_width);

    auto quotient = div_signed(lhs_ext, bits(rhs));
    lhs.value_ = quotient.truncate(lhs.width());

    return lhs;
}

inline DynSigned& operator%=(DynSigned& lhs, DynUnsigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.width(), rhs.width()) + 1;

    auto lhs_ext = bits(lhs).sign_extend(safe_width);
    auto rhs_positive = bits(rhs).zero_extend(safe_width);

    auto remainder = mod_signed(lhs_ext, rhs_positive);
    lhs.value_ = remainder.truncate(lhs.width());

    return lhs;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_SIGNED_HPP

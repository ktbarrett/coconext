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

        if (value_.get_width() < target_width) {
            auto extended = value_.sign_extend(target_width);
            return static_cast<T>(extended.raw().word(0));
        } else {
            auto truncated = value_.truncate(target_width);
            return static_cast<T>(truncated.raw().word(0));
        }
    }

  public:
    DynSigned(DynBits const& val) : value_(val) {}

    size_t get_width() const { return value_.get_width(); }

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

    //     // Cross-width conversion. Throws if the source value doesn't fit in N bits.
    //     template <Range R2>
    //     explicit(R.length() < R2.length()) constexpr Unsigned(Unsigned<R2> const& other)
    //     {
    //         if constexpr (R.length() >= R2.length()) {
    //             value_ = other.value_;
    //         } else if (
    //             bits(other).ule((~Bits<R.length()>{}).template
    //             zero_extend<R2.length()>())
    //         )
    //         {
    //             value_ = coconext::types::resize<R.length()>(other).value_;
    //         } else {
    //             throw std::out_of_range("value does not fit in Unsigned width");
    //         }
    //     }

    //     // Cross-width conversion from DynSigned. Throws if the source value doesn't fit
    //     in N bits
    //     // or is negative.
    //     template <Range R2>
    //     explicit constexpr Unsigned(DynSigned<R2> const& other) {
    //         auto const& src_bits = bits(other);
    //         if constexpr (R2.length() > 0) {
    //             if (src_bits.get_bit(R2.length() - 1)) {
    //                 throw std::out_of_range(
    //                     "Cannot construct Unsigned from negative DynSigned value"
    //                 );
    //             }
    //         }
    //         *this = Unsigned<R>(Unsigned<R2>(src_bits));
    //     }

    // Construct from a BitArray. Throws if the source value is not exactly N bits.
    // explicit constexpr Unsigned(Array<Bit, R2> const& other) {
    //     static_assert(
    //         R.length() == R2.length(), "BitArray reinterpret requires identical width"
    //     );

    //     value_ = bits(other);
    // }

    //     // Implicit conversion to supertype BitArray
    //     template <Range R2>
    //     constexpr operator Array<Bit, R2>() const noexcept {
    //         static_assert(
    //             R.length() == R2.length(), "BitArray reinterpret requires identical
    //             width"
    //         );
    //         return Array<Bit, R2>(value_);
    //     }

    //     // Consume deduced-target reinterpret wrapper
    //     template <typename SourceT>
    //     constexpr Unsigned(auto_reinterpreted<SourceT>&& wrapper) {
    //         *this = as<Unsigned<R>>(std::move(wrapper).consume());
    //     }

    //     template <typename SourceT>
    //     constexpr Unsigned& operator=(auto_reinterpreted<SourceT>&& wrapper) {
    //         *this = as<Unsigned<R>>(std::move(wrapper).consume());
    //         return *this;
    //     }

    //     template <typename SourceWrapper>
    //     constexpr Unsigned(auto_resized<SourceWrapper>&& wrapper) {
    //         auto [src, ovf, rnd] = std::move(wrapper).consume();
    //         using ActualSource = std::remove_cvref_t<SourceWrapper>;

    //         static_assert(
    //             is_coconext_unsigned_v<ActualSource>,
    //             "reR.length() target and source must both be Unsigned. Use as() for
    //             cross-type " "conversions."
    //         );

    //         constexpr size_t TargetW = R.length();
    //         constexpr size_t SourceW = ActualSource::size();

    //         if constexpr (TargetW >= SourceW) {
    //             // Widening (and the null cases) never lose information.
    //             value_ = bits(src).template zero_extend<TargetW>();
    //         } else if (ovf == overflow_mode::wrap) {
    //             value_ = bits(src).template truncate<TargetW>();
    //         } else {
    //             value_ = bits(src).template saturate_unsigned<TargetW>();
    //         }
    //     }

    //     template <typename SourceWrapper>
    //     constexpr Unsigned& operator=(auto_resized<SourceWrapper>&& wrapper) {
    //         *this = Unsigned(std::move(wrapper));
    //         return *this;
    //     }

    bool operator==(DynSigned const& rhs) const noexcept {
        return value_ == rhs.value_ && get_width() == rhs.get_width();
    }

    auto operator<(DynSigned const& rhs) const noexcept { return value_.slt(rhs.value_); }

    auto operator<=(DynSigned const& rhs) const noexcept { return value_.sle(rhs.value_); }

    auto operator>(DynSigned const& rhs) const noexcept { return value_.sgt(rhs.value_); }

    auto operator>=(DynSigned const& rhs) const noexcept { return value_.sge(rhs.value_); }

    explicit operator bool() const noexcept {
        return value_ != DynBits{value_.get_width(), 0};
    }

    explicit operator long long() const { return to_native_int<long long>(); }

    //     template <typename ShiftType>
    //     constexpr Unsigned<R> operator<<(ShiftType const& shift_amount) const {
    //         using CleanType = std::remove_cvref_t<ShiftType>;

    //         // Shift amounts must fit in a native integer.
    //         static_assert(
    //             std::is_integral_v<CleanType> || is_coconext_unsigned_v<CleanType>
    //                 || is_coconext_signed_v<CleanType>,
    //             "Shift amount can only be a native integer, DynSigned, or Unsigned"
    //         );

    //         size_t safe_shift = 0;

    //         if constexpr (std::is_integral_v<CleanType>) {
    //             if constexpr (std::is_signed_v<CleanType>) {
    //                 if (shift_amount < 0) {
    //                     throw std::invalid_argument("Negative shift amount");
    //                 }
    //             }
    //             safe_shift = static_cast<size_t>(shift_amount);
    //         } else if constexpr (is_coconext_unsigned_v<CleanType>) {
    //             static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
    //             safe_shift = static_cast<size_t>(static_cast<unsigned long
    //             long>(shift_amount));
    //         } else if constexpr (is_coconext_signed_v<CleanType>) {
    //             static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
    //             long long signed_val = static_cast<long long>(shift_amount);
    //             if (signed_val < 0) {
    //                 throw std::invalid_argument("Negative shift amount");
    //             }
    //             safe_shift = static_cast<size_t>(signed_val);
    //         }

    //         if (safe_shift >= R.length()) {
    //             return Unsigned<R>(0);
    //         }

    //         return Unsigned<R>(value_ << safe_shift);
    //     }

    //     template <typename ShiftType>
    //     constexpr Unsigned<R> operator>>(ShiftType const& shift_amount) const {
    //         using CleanType = std::remove_cvref_t<ShiftType>;

    //         // Shift amounts must fit in a native integer.
    //         // thing
    //         static_assert(
    //             std::is_integral_v<CleanType> || is_coconext_unsigned_v<CleanType>
    //                 || is_coconext_signed_v<CleanType>,
    //             "Shift amount can only be a native integer, DynSigned, or Unsigned"
    //         );

    //         size_t safe_shift = 0;

    //         if constexpr (std::is_integral_v<CleanType>) {
    //             if constexpr (std::is_signed_v<CleanType>) {
    //                 if (shift_amount < 0) {
    //                     throw std::invalid_argument("Negative shift amount");
    //                 }
    //             }
    //             safe_shift = static_cast<size_t>(shift_amount);
    //         } else if constexpr (is_coconext_unsigned_v<CleanType>) {
    //             static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
    //             safe_shift = static_cast<size_t>(static_cast<unsigned long
    //             long>(shift_amount));
    //         } else if constexpr (is_coconext_signed_v<CleanType>) {
    //             static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
    //             long long signed_val = static_cast<long long>(shift_amount);
    //             if (signed_val < 0) {
    //                 throw std::invalid_argument("Negative shift amount");
    //             }
    //             safe_shift = static_cast<size_t>(signed_val);
    //         }

    //         if (safe_shift >= R.length()) {
    //             return Unsigned<R>(0);
    //         }

    //         return Unsigned<R>(value_.srl(safe_shift));
    //     }

    //     template <typename ShiftType>
    //     constexpr Unsigned<R>& operator<<=(ShiftType const& shift_amount) {
    //         *this = *this << shift_amount;
    //         return *this;
    //     }

    //     template <typename ShiftType>
    //     constexpr Unsigned<R>& operator>>=(ShiftType const& shift_amount) {
    //         *this = *this >> shift_amount;
    //         return *this;
    //     }

    //     constexpr auto operator+() const {
    //         constexpr Range R_res = int_downto_range(R.length() + 1);
    //         return coconext::types::as<DynSigned<R_res>>(
    //             coconext::types::resize<R_res.length()>(*this)
    //         );
    //     }

    //     // The operand is unsigned, so it zero-extends into the wider result before
    //     // being negated: -Unsigned<8>(150) is -150, not -(-106).
    //     constexpr auto operator-() const {
    //         constexpr size_t Wr = R.length() + 1;
    //         return DynSigned<int_downto_range(Wr)>(
    //             Bits<Wr>{} - value_.template zero_extend<Wr>()
    //         );
    //     }

    auto operator+() const { return *this; }

    auto operator-() const {
        return DynSigned(sub_signed(DynBits(get_width(), 0), value_));
    }

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
        return DynSigned(rem_signed(value_, rhs.value_));
    }

    auto operator+=(DynSigned const& rhs) {
        if (get_width() >= rhs.get_width()) {
            value_ = add_signed(value_, rhs.value_).truncate(get_width());
            return *this;
        } else {
            value_ = DynBits::exact_add(value_, rhs.value_.truncate(get_width()));
            return *this;
        }
    }

    auto operator-=(DynSigned const& rhs) {
        if (get_width() >= rhs.get_width()) {
            value_ = sub_signed(value_, rhs.value_).truncate(get_width());
            return *this;
        } else {
            value_ = DynBits::exact_sub(value_, rhs.value_.truncate(get_width()));
            return *this;
        }
    }

    auto operator*=(DynSigned const& rhs) {
        if (get_width() >= rhs.get_width()) {
            value_ = mul_signed(value_, rhs.value_).truncate(get_width());
            return *this;
        } else {
            value_ = DynBits::exact_mul(value_, rhs.value_.truncate(get_width()));
            return *this;
        }
    }

    auto operator/=(DynSigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }

        value_ = detail::div_signed(value_, rhs.value_).truncate(get_width());
        return *this;
    }

    auto operator%=(DynSigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }

        value_ = detail::rem_signed(value_, rhs.value_).truncate(get_width());
        return *this;
    }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator+=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             *this = coconext::types::resize<R.length()>(
    //                 *this + Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //         }
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator-=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             auto res = coconext::types::resize<R.length()>(
    //                 *this - Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //             *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         }
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator*=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             *this = coconext::types::resize<R.length()>(
    //                 *this * Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //         }
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator/=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             *this = coconext::types::resize<R.length()>(
    //                 *this / Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //         }
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator%=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             *this = coconext::types::resize<R.length()>(
    //                 *this % Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //         }
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator+=(DynSigned<R2> const& rhs) {
    //         auto res = coconext::types::resize<R.length()>(+(*this) + rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator-=(DynSigned<R2> const& rhs) {
    //         auto res = coconext::types::resize<R.length()>(+(*this) - rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator*=(DynSigned<R2> const& rhs) {
    //         auto res = coconext::types::resize<R.length()>(+(*this) * rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator/=(DynSigned<R2> const& rhs) {
    //         if (!static_cast<bool>(rhs)) {
    //             throw std::domain_error("Division by zero");
    //         }
    //         auto res = coconext::types::resize<R.length()>(+(*this) / rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator%=(DynSigned<R2> const& rhs) {
    //         if (!static_cast<bool>(rhs)) {
    //             throw std::domain_error("Division by zero");
    //         }
    //         auto res = coconext::types::resize<R.length()>(+(*this) % rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

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

DynSigned operator+(DynUnsigned const& lhs) {
    return DynSigned(bits(lhs).zero_extend(lhs.get_width() + 1));
}

DynSigned operator-(DynUnsigned const& lhs) {
    return DynSigned(sub_unsigned(DynBits(lhs.get_width(), 0), bits(lhs)));
}

DynSigned operator-(DynUnsigned const& lhs, DynUnsigned const& rhs) {
    return DynSigned(sub_unsigned(bits(lhs), bits(rhs)));
}

DynUnsigned operator+=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto temp = DynSigned(bits(lhs));
    lhs.value_ = std::move(bits(temp += rhs));
    return lhs;
}

DynUnsigned operator-=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto temp = DynSigned(bits(lhs));
    lhs.value_ = std::move(bits(temp -= rhs));
    return lhs;
}

DynUnsigned operator*=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto temp = DynSigned(bits(lhs));
    lhs.value_ = std::move(bits(temp *= rhs));
    return lhs;
}

DynUnsigned operator/=(DynUnsigned& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.get_width() + 1, rhs.get_width());
    auto lhs_positive = bits(lhs).zero_extend(safe_width);

    auto quotient = div_signed(lhs_positive, bits(rhs));
    lhs.value_ = quotient.truncate(lhs.get_width());

    return lhs;
}

DynUnsigned operator%=(DynUnsigned& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.get_width() + 1, rhs.get_width());
    auto lhs_positive = detail::bits(lhs).zero_extend(safe_width);

    auto remainder = detail::rem_signed(lhs_positive, detail::bits(rhs));
    lhs.value_ = remainder.truncate(lhs.get_width());

    return lhs;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_SIGNED_HPP

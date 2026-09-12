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
    explicit DynSigned(Range range) : value_(range.length()), range_(range) {}

    template <bool IsSigned>
    explicit DynSigned(DynInt<IsSigned> val)
        : value_(std::move(val)), range_(int_downto_range(value_.size())) {}

    template <bool IsSigned>
    DynSigned(DynInt<IsSigned> val, Range range) : value_(std::move(val)), range_(range) {
        if (value_.size() != range_.length()) {
            throw std::invalid_argument("Integer storage width does not match its range");
        }
    }

    DynSigned(std::string_view str, Range range)
        : value_(str, range.length()), range_(range) {}

    explicit DynSigned(std::string_view str, size_t width)
        : DynSigned(str, int_downto_range(width)) {}

    Range range() const noexcept { return range_; }
    size_t size() const noexcept { return range_.length(); }

    // Construct from a native integer. Range coordinates label bits, not powers of two.
    template <NativeInteger T>
    DynSigned(T v, Range range) : DynSigned(range) {
        if (size() == 0) {
            throw std::invalid_argument("DynSigned(0) has no integer representation");
        }
        if (!native_value_fits<true>(size(), v)) {
            throw std::overflow_error("value does not fit in Signed width");
        }
        value_ = DynSInt(v, size());
    }

    template <NativeInteger T>
    DynSigned(T v, size_t width) : DynSigned(v, int_downto_range(width)) {}

    template <HasDynamicStorage Target>
    [[nodiscard]] Target as() && {
        return adopt_storage<Target>(range_, std::move(value_));
    }

    [[nodiscard]] auto as() && noexcept {
        return reinterpreted<DynSigned>(std::move(*this));
    }

    bool operator==(DynSigned const& rhs) const noexcept {
        return value_ == rhs.value_ && size() == rhs.size();
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

        if (safe_shift >= size()) {
            return DynSigned(0, range_);
        }

        return DynSigned(value_ << safe_shift, range_);
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

        if (safe_shift >= size()) {
            if (safe_shift > 0) {
                return DynSigned(-1, range_);
            } else {
                return DynSigned(0, range_);
            }
        }

        return DynSigned(value_ >> safe_shift, range_);
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

    auto operator|(DynSigned const& other) const {
        return DynSigned(value_ | storage(other), range_);
    }

    auto operator&(DynSigned const& other) const {
        return DynSigned(value_ & storage(other), range_);
    }

    auto operator^(DynSigned const& other) const {
        return DynSigned(value_ ^ storage(other), range_);
    }

    auto operator~() const { return DynSigned(~value_, range_); }

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
        return DynSigned(value_ % rhs.value_, rhs.range_);
    }

    auto operator+=(DynSigned const& rhs) {
        value_ = DynSInt(value_ + rhs.value_, size());
        return *this;
    }

    auto operator-=(DynSigned const& rhs) {
        value_ = DynSInt(value_ - rhs.value_, size());
        return *this;
    }

    auto operator*=(DynSigned const& rhs) {
        value_ = DynSInt(value_ * rhs.value_, size());
        return *this;
    }

    auto operator/=(DynSigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }

        value_ = DynSInt(value_ / rhs.value_, size());
        return *this;
    }

    auto operator%=(DynSigned const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }

        value_ = DynSInt(value_ % rhs.value_, size());
        return *this;
    }

    template <NativeInteger T>
    auto operator+=(T const& rhs) {
        *this += DynSigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    template <NativeInteger T>
    auto operator-=(T const& rhs) {
        *this -= DynSigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    template <NativeInteger T>
    auto operator*=(T const& rhs) {
        *this *= DynSigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    template <NativeInteger T>
    auto operator/=(T const& rhs) {
        *this /= DynSigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    template <NativeInteger T>
    auto operator%=(T const& rhs) {
        *this %= DynSigned(rhs, std::numeric_limits<T>::digits);
        return *this;
    }

    auto begin() noexcept { return value_.begin(); }
    auto begin() const noexcept { return value_.begin(); }
    auto end() noexcept { return value_.end(); }
    auto end() const noexcept { return value_.end(); }
    auto rbegin() noexcept { return value_.rbegin(); }
    auto rbegin() const noexcept { return value_.rbegin(); }
    auto rend() noexcept { return value_.rend(); }
    auto rend() const noexcept { return value_.rend(); }

    auto operator[](Range::value_type index) {
        auto const offset = offset_of(range_, index);
        if (!offset) {
            throw std::out_of_range("DynSigned index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }

    auto operator[](Range::value_type index) const {
        auto const offset = offset_of(range_, index);
        if (!offset) {
            throw std::out_of_range("DynSigned index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }

    bool index(Range::value_type index) const { return static_cast<bool>((*this)[index]); }

    friend DynSigned& operator+=(DynSigned& lhs, DynUnsigned const& rhs);
    friend DynSigned& operator-=(DynSigned& lhs, DynUnsigned const& rhs);
    friend DynSigned& operator*=(DynSigned& lhs, DynUnsigned const& rhs);
    friend DynSigned& operator/=(DynSigned& lhs, DynUnsigned const& rhs);
    friend DynSigned& operator%=(DynSigned& lhs, DynUnsigned const& rhs);

  private:
    int compare_value(DynSigned const& rhs) const {
        size_t const compare_width = std::max(size(), rhs.size());
        auto lhs_value = DynSInt(value_, compare_width);
        auto rhs_value = DynSInt(rhs.value_, compare_width);
        return lhs_value < rhs_value ? -1 : rhs_value < lhs_value ? 1 : 0;
    }

    friend struct storage_fn;
    DynSInt value_;
    Range range_;
};

inline DynSigned rem(DynSigned const& lhs, DynSigned const& rhs) { return lhs % rhs; }

inline DynSigned mod(DynSigned const& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }
    return DynSigned(mod(storage(lhs), storage(rhs)), rhs.range());
}

// DynUnsigned Unary operators
inline DynSigned operator+(DynUnsigned const& lhs) {
    return DynSigned(DynSInt(storage(lhs), lhs.size() + 1));
}

inline DynSigned operator-(DynUnsigned const& lhs) { return DynSigned(-storage(lhs)); }

// DynUnsigned operator-
inline DynSigned operator-(DynUnsigned const& lhs, DynUnsigned const& rhs) {
    return DynSigned(storage(lhs) - storage(rhs));
}

// DynUnsigned X DynSigned compound operators
inline DynUnsigned& operator+=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto result = DynSInt(storage(lhs), lhs.size()) + storage(rhs);
    lhs.value_ = DynUInt(result, lhs.size());
    return lhs;
}

inline DynUnsigned& operator-=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto result = DynSInt(storage(lhs), lhs.size()) - storage(rhs);
    lhs.value_ = DynUInt(result, lhs.size());
    return lhs;
}

inline DynUnsigned& operator*=(DynUnsigned& lhs, DynSigned const& rhs) {
    auto result = DynSInt(storage(lhs), lhs.size()) * storage(rhs);
    lhs.value_ = DynUInt(result, lhs.size());
    return lhs;
}

inline DynUnsigned& operator/=(DynUnsigned& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.size() + 1, rhs.size());
    auto lhs_positive = DynSInt(storage(lhs), safe_width);

    auto quotient = lhs_positive / storage(rhs);
    lhs.value_ = DynUInt(quotient, lhs.size());

    return lhs;
}

inline DynUnsigned& operator%=(DynUnsigned& lhs, DynSigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.size() + 1, rhs.size());
    auto lhs_positive = DynSInt(storage(lhs), safe_width);

    auto remainder = lhs_positive % storage(rhs);
    lhs.value_ = DynUInt(remainder, lhs.size());

    return lhs;
}

// DynSigned X DynUnsigned compound operators
inline DynSigned& operator+=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = DynSInt(storage(rhs), rhs.size() + 1);
    auto result = storage(lhs) + rhs_positive;
    lhs.value_ = DynSInt(result, lhs.size());
    return lhs;
}

inline DynSigned& operator-=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = DynSInt(storage(rhs), rhs.size() + 1);
    auto result = storage(lhs) - rhs_positive;
    lhs.value_ = DynSInt(result, lhs.size());
    return lhs;
}

inline DynSigned& operator*=(DynSigned& lhs, DynUnsigned const& rhs) {
    auto rhs_positive = DynSInt(storage(rhs), rhs.size() + 1);
    auto result = storage(lhs) * rhs_positive;
    lhs.value_ = DynSInt(result, lhs.size());
    return lhs;
}

inline DynSigned& operator/=(DynSigned& lhs, DynUnsigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.size() + 1, rhs.size());
    auto lhs_ext = DynSInt(storage(lhs), safe_width);
    auto rhs_positive = DynSInt(storage(rhs), rhs.size() + 1);

    auto quotient = lhs_ext / rhs_positive;
    lhs.value_ = DynSInt(quotient, lhs.size());

    return lhs;
}

inline DynSigned& operator%=(DynSigned& lhs, DynUnsigned const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }

    size_t safe_width = std::max(lhs.size(), rhs.size()) + 1;

    auto lhs_ext = DynSInt(storage(lhs), safe_width);
    auto rhs_positive = DynSInt(storage(rhs), safe_width);

    auto remainder = lhs_ext % rhs_positive;
    lhs.value_ = DynSInt(remainder, lhs.size());

    return lhs;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_SIGNED_HPP

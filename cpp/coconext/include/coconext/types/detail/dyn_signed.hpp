#ifndef COCONEXT_DETAIL_DYN_SIGNED_HPP
#define COCONEXT_DETAIL_DYN_SIGNED_HPP

#include <algorithm>
#include <optional>
#include <utility>

#include <coconext/types/detail/common_math.hpp>
#include <coconext/types/detail/dyn_bigint.hpp>
#include <coconext/types/detail/dyn_unsigned.hpp>

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

    Bit and_reduce() const { return value_.popcount() == size() ? Bit::_1 : Bit::_0; }

    Bit or_reduce() const { return value_.popcount() != 0 ? Bit::_1 : Bit::_0; }

    Bit xor_reduce() const { return value_.popcount() % 2 == 1 ? Bit::_1 : Bit::_0; }

    std::optional<Vector<Bit>> resolve(ResolveMethod = ResolveMethod::WEAK) const {
        return std::optional<Vector<Bit>>{std::in_place, value_, range_};
    }

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

    template <NativeInteger ShiftType>
    DynSigned operator<<(ShiftType const& shift_amount) const {
        size_t const safe_shift = normalize_shift_amount(shift_amount, size());

        if (safe_shift >= size()) {
            return DynSigned(0, range_);
        }

        return DynSigned(value_ << safe_shift, range_);
    }

    template <NativeInteger ShiftType>
    DynSigned operator>>(ShiftType const& shift_amount) const {
        size_t const safe_shift = normalize_shift_amount(shift_amount, size());

        if (safe_shift >= size()) {
            if (safe_shift > 0) {
                return DynSigned(-1, range_);
            } else {
                return DynSigned(0, range_);
            }
        }

        return DynSigned(value_ >> safe_shift, range_);
    }

    template <NativeInteger ShiftType>
    constexpr DynSigned& operator<<=(ShiftType const& shift_amount) {
        *this = *this << shift_amount;
        return *this;
    }

    template <NativeInteger ShiftType>
    constexpr DynSigned& operator>>=(ShiftType const& shift_amount) {
        *this = *this >> shift_amount;
        return *this;
    }

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

    DynSigned rem(DynSigned const& rhs) const { return *this % rhs; }

    DynSigned mod(DynSigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynSigned(value_.mod(storage(rhs)), rhs.range());
    }

    auto begin() noexcept { return value_.begin(); }
    auto begin() const noexcept { return value_.begin(); }
    auto end() noexcept { return value_.end(); }
    auto end() const noexcept { return value_.end(); }
    auto rbegin() noexcept { return value_.rbegin(); }
    auto rbegin() const noexcept { return value_.rbegin(); }
    auto rend() noexcept { return value_.rend(); }
    auto rend() const noexcept { return value_.rend(); }

    DynSigned& inplace_not() & {
        for (auto&& v : *this) {
            v = ~v;
        }
        return *this;
    }

    DynSigned&& inplace_not() && {
        inplace_not();
        return std::move(*this);
    }

    std::optional<Range::value_type> index_of(Bit const& v) const {
        auto const it = std::ranges::find(*this, v);
        if (it == end()) {
            return std::nullopt;
        }
        return range()[static_cast<size_t>(std::ranges::distance(begin(), it))];
    }

    std::optional<Range::value_type> rindex_of(Bit const& v) const {
        auto const it = std::find(rbegin(), rend(), v);
        if (it == rend()) {
            return std::nullopt;
        }
        return range()
            [size() - 1 - static_cast<size_t>(std::ranges::distance(rbegin(), it))];
    }

    auto operator[](Range::value_type index) {
        auto const offset = range_.offset_of(index);
        if (!offset) {
            throw std::out_of_range("DynSigned index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }

    auto operator[](Range::value_type index) const {
        auto const offset = range_.offset_of(index);
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

namespace coconext::types {

using DynSigned = detail::DynSigned;

}  // namespace coconext::types

#endif  // COCONEXT_DETAIL_DYN_SIGNED_HPP

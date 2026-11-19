#ifndef COCONEXT_DETAIL_DYN_UNSIGNED_HPP
#define COCONEXT_DETAIL_DYN_UNSIGNED_HPP

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <coconext/types/detail/common_math.hpp>
#include <coconext/types/detail/concepts.hpp>
#include <coconext/types/detail/dyn_bigint.hpp>
#include <coconext/types/detail/hash.hpp>
#include <coconext/types/detail/logic_array_ops.hpp>
#include <coconext/types/range.hpp>

namespace coconext::types::detail {

class DynSigned;

class DynUnsigned {
    template <typename T>
    T to_native_int() const {
        return value_.template to_native_integer<T>();
    }

  public:
    explicit DynUnsigned(Range range) : value_(range.length()), range_(range) {}

    template <bool IsSigned>
    explicit DynUnsigned(DynInt<IsSigned> val)
        : value_(std::move(val)), range_(int_downto_range(value_.size())) {}

    template <bool IsSigned>
    DynUnsigned(DynInt<IsSigned> val, Range range) : value_(std::move(val)), range_(range) {
        if (value_.size() != range_.length()) {
            throw std::invalid_argument("Integer storage width does not match its range");
        }
    }

    DynUnsigned(std::string_view str, Range range)
        : value_(str, range.length()), range_(range) {}

    explicit DynUnsigned(std::string_view str, size_t width)
        : DynUnsigned(str, int_downto_range(width)) {}

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
    DynUnsigned(T v, Range range) : DynUnsigned(range) {
        if (size() == 0) {
            throw std::invalid_argument("DynUnsigned(0) has no integer representation");
        }
        if (!native_value_fits<false>(size(), v)) {
            throw std::overflow_error("value does not fit in Unsigned width");
        }
        value_ = DynUInt(v, size());
    }

    template <NativeInteger T>
    DynUnsigned(T v, size_t width) : DynUnsigned(v, int_downto_range(width)) {}

    template <HasDynamicStorage Target>
    [[nodiscard]] Target as() && {
        return adopt_storage<Target>(range_, std::move(value_));
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

    template <NativeInteger ShiftType>
    DynUnsigned operator<<(ShiftType const& shift_amount) const {
        size_t const safe_shift = normalize_shift_amount(shift_amount, size());

        if (safe_shift >= size()) {
            return DynUnsigned(0, range_);
        }

        return DynUnsigned(value_ << safe_shift, range_);
    }

    template <NativeInteger ShiftType>
    DynUnsigned operator>>(ShiftType const& shift_amount) const {
        size_t const safe_shift = normalize_shift_amount(shift_amount, size());

        if (safe_shift >= size()) {
            return DynUnsigned(0, range_);
        }

        return DynUnsigned(value_ >> safe_shift, range_);
    }

    template <NativeInteger ShiftType>
    constexpr DynUnsigned& operator<<=(ShiftType const& shift_amount) {
        *this = *this << shift_amount;
        return *this;
    }

    template <NativeInteger ShiftType>
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
        return DynUnsigned(value_ % rhs.value_, rhs.range_);
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

    DynUnsigned& inplace_not() & {
        for (auto&& v : *this) {
            v = ~v;
        }
        return *this;
    }

    DynUnsigned&& inplace_not() && {
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
            throw std::out_of_range("DynUnsigned index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }

    auto operator[](Range::value_type index) const {
        auto const offset = range_.offset_of(index);
        if (!offset) {
            throw std::out_of_range("DynUnsigned index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }

    bool index(Range::value_type index) const { return static_cast<bool>((*this)[index]); }

  private:
    int compare_value(DynUnsigned const& rhs) const {
        size_t const compare_width = std::max(size(), rhs.size());
        auto lhs_value = DynUInt(value_, compare_width);
        auto rhs_value = DynUInt(rhs.value_, compare_width);
        return lhs_value < rhs_value ? -1 : rhs_value < lhs_value ? 1 : 0;
    }

    friend struct storage_fn;
    DynUInt value_;
    Range range_;
};

}  // namespace coconext::types::detail

namespace coconext::types {

using DynUnsigned = detail::DynUnsigned;

}  // namespace coconext::types

#endif  // COCONEXT_DETAIL_DYN_UNSIGNED_HPP

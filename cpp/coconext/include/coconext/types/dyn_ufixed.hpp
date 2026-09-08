#ifndef COCONEXT_DYN_UFIXED_HPP
#define COCONEXT_DYN_UFIXED_HPP

#include <algorithm>
#include <climits>
#include <cmath>
#include <coconext/types/dyn_fixed_base.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/ufixed.hpp>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace coconext::types::detail {

class DynUfixed {
    template <NativeInteger T>
    static DynUfixed integer_operand(T value) {
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(value, negative);
        Range range = int_downto_range(magnitude.width());
        return DynUfixed(range, std::move(magnitude));
    }

    template <NativeInteger T>
    T to_native_int() const {
        dyn_fixed_detail::require_numeric_range(range_);
        constexpr size_t target_width = sizeof(T) * CHAR_BIT;
        auto aligned =
            dyn_fixed_detail::align_magnitude(value_, range_.right, 0, target_width + 1);
        bool const target_signed = std::numeric_limits<T>::is_signed;
        bool out_of_range = aligned.overflow;
        if (target_signed) {
            out_of_range = out_of_range || aligned.bits.get_bit(target_width - 1)
                        || aligned.bits.get_bit(target_width);
        } else {
            out_of_range = out_of_range || aligned.bits.get_bit(target_width);
        }
        if (out_of_range) {
            throw std::out_of_range("DynUfixed value does not fit destination integer");
        }
        return DynUInt(target_width, aligned.bits).template to_native_integer<T>();
    }

    template <std::floating_point T>
    T to_native_float() const {
        dyn_fixed_detail::require_numeric_range(range_);
        if (value_.width() == 0 || value_.popcount() == 0) {
            return T{0};
        }
        size_t const msb = value_.width() - value_.count_leading_zeros() - 1;
        size_t constexpr precision = std::numeric_limits<T>::digits;
        size_t const shift = msb >= precision ? msb - precision + 1 : 0;
        std::uint64_t mantissa = 0;
        size_t const retained = std::min(precision, value_.width() - shift);
        for (size_t i = 0; i < retained; ++i) {
            if (value_.get_bit(shift + i)) {
                mantissa |= std::uint64_t{1} << i;
            }
        }
        if (shift > 0) {
            bool const round_bit = value_.get_bit(shift - 1);
            bool sticky = false;
            for (size_t i = 0; i + 1 < shift; ++i) {
                if (value_.get_bit(i)) {
                    sticky = true;
                    break;
                }
            }
            if (round_bit && (sticky || (mantissa & 1))) {
                ++mantissa;
            }
        }
        auto const exponent =
            static_cast<long double>(range_.right) + static_cast<long double>(shift);
        if (exponent > std::numeric_limits<int>::max()) {
            return std::numeric_limits<T>::infinity();
        }
        if (exponent < std::numeric_limits<int>::min()) {
            return T{0};
        }
        return std::ldexp(static_cast<T>(mantissa), static_cast<int>(exponent));
    }

  public:
    explicit DynUfixed(Range range) : range_(range), value_(range.length()) {}

    DynUfixed(Range range, DynUInt raw) : range_(range), value_(std::move(raw)) {
        dyn_fixed_detail::validate_storage(range_, value_.width());
    }

    template <NativeInteger T>
    DynUfixed(Range range, T value) : range_(range), value_(range.length()) {
        dyn_fixed_detail::require_numeric_range(range_);
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(value, negative);
        if (negative) {
            throw std::out_of_range("Cannot construct DynUfixed from a negative integer");
        }
        value_ = dyn_fixed_detail::convert_unsigned_magnitude(magnitude, 0, range_);
    }

    template <std::floating_point T>
    explicit DynUfixed(
        Range range,
        T value,
        overflow_mode overflow = overflow_mode::saturate,
        round_mode rounding = round_mode::round_to_even
    )
        : range_(range), value_(range.length()) {
        dyn_fixed_detail::require_numeric_range(range_);
        if (std::isnan(value)) {
            throw std::domain_error("Cannot convert NaN to fixed-point");
        }
        if (std::isinf(value)) {
            if (overflow == overflow_mode::wrap) {
                throw std::domain_error("Cannot wrap Infinity");
            }
            value_ = value > T{0} ? ~DynUInt(size()) : DynUInt(size());
            return;
        }
        if (value < T{0}) {
            throw std::out_of_range("Cannot construct DynUfixed from a negative float");
        }
        auto aligned = dyn_fixed_detail::align_floating_magnitude(
            value, range_.right, dyn_fixed_detail::checked_size_add(size(), 1)
        );
        dyn_fixed_detail::round_magnitude(aligned, rounding, false);
        bool const out_of_range = aligned.overflow || aligned.bits.get_bit(size());
        if (out_of_range && overflow == overflow_mode::saturate) {
            value_ = ~DynUInt(size());
        } else {
            value_ = DynUInt(size(), aligned.bits);
        }
    }

    DynUfixed(Range range, DynUnsigned const& source)
        : DynUfixed(
              range, dyn_fixed_detail::convert_unsigned_magnitude(storage(source), 0, range)
          ) {}

    template <Range R>
    DynUfixed(Range range, Unsigned<R> const& source)
        : DynUfixed(
              range,
              dyn_fixed_detail::convert_unsigned_magnitude(
                  DynUInt(storage(source)), 0, range
              )
          ) {
        dyn_fixed_detail::require_downto(R);
    }

    DynUfixed(Range range, DynSigned const& source);

    template <Range R>
    DynUfixed(Range range, Signed<R> const& source)
        : range_(range), value_(range.length()) {
        dyn_fixed_detail::require_downto(R);
        auto raw = DynSInt(storage(source));
        if (raw.width() != 0 && raw.is_negative()) {
            throw std::out_of_range("Cannot construct DynUfixed from a negative Signed");
        }
        value_ =
            dyn_fixed_detail::convert_unsigned_magnitude(raw.logical_bits(), 0, range_);
    }

    DynUfixed(Range range, DynUfixed const& source)
        : DynUfixed(
              range,
              dyn_fixed_detail::convert_unsigned_magnitude(
                  source.value_, source.range_.right, range
              )
          ) {
        dyn_fixed_detail::require_downto(source.range_);
    }

    DynUfixed(Range range, DynSfixed const& source);

    template <Range R>
    DynUfixed(Range range, Ufixed<R> const& source)
        : DynUfixed(
              range,
              dyn_fixed_detail::convert_unsigned_magnitude(
                  DynUInt(storage(source)), R.right, range
              )
          ) {
        dyn_fixed_detail::require_downto(R);
    }

    template <Range R>
    DynUfixed(Range range, Sfixed<R> const& source);

    template <HasDynamicStorage Target>
    [[nodiscard]] Target as() && {
        return adopt_storage<Target>(range_, std::move(value_));
    }

    [[nodiscard]] auto as() && noexcept {
        return reinterpreted<DynUfixed>(std::move(*this));
    }

    static DynUfixed resized(
        Range target,
        DynUfixed const& source,
        overflow_mode overflow = overflow_mode::saturate,
        round_mode rounding = round_mode::round_to_even
    ) {
        dyn_fixed_detail::require_downto(source.range_);
        return DynUfixed(
            target,
            dyn_fixed_detail::resize_unsigned_magnitude(
                source.value_, source.range_.right, target, overflow, rounding
            )
        );
    }

    Range range() const noexcept { return range_; }
    size_t size() const noexcept { return range_.length(); }
    size_t width() const noexcept { return size(); }

    Range::value_type frac_bits() const noexcept {
        return -(range_.direction == Direction::DOWNTO ? range_.right : range_.left);
    }
    Range::value_type int_bits() const noexcept {
        return (range_.direction == Direction::DOWNTO ? range_.left : range_.right) + 1;
    }
    double resolution() const noexcept {
        auto const exponent =
            range_.direction == Direction::DOWNTO ? range_.right : range_.left;
        if (exponent > std::numeric_limits<int>::max()) {
            return std::numeric_limits<double>::infinity();
        }
        if (exponent < std::numeric_limits<int>::min()) {
            return 0.0;
        }
        return std::ldexp(1.0, static_cast<int>(exponent));
    }

    std::string raw_decimal() const { return value_.to_decimal_string(); }
    std::string raw_binary() const { return value_.to_binary_string(); }

    explicit operator bool() const {
        dyn_fixed_detail::require_downto(range_);
        return value_.popcount() != 0;
    }
    explicit operator signed char() const { return to_native_int<signed char>(); }
    explicit operator unsigned char() const { return to_native_int<unsigned char>(); }
    explicit operator short() const { return to_native_int<short>(); }
    explicit operator unsigned short() const { return to_native_int<unsigned short>(); }
    explicit operator int() const { return to_native_int<int>(); }
    explicit operator unsigned int() const { return to_native_int<unsigned int>(); }
    explicit operator long() const { return to_native_int<long>(); }
    explicit operator unsigned long() const { return to_native_int<unsigned long>(); }
    explicit operator long long() const { return to_native_int<long long>(); }
    explicit operator unsigned long long() const {
        return to_native_int<unsigned long long>();
    }
#if defined(__SIZEOF_INT128__)
    explicit operator __int128_t() const { return to_native_int<__int128_t>(); }
    explicit operator __uint128_t() const { return to_native_int<__uint128_t>(); }
#endif
    explicit operator float() const { return to_native_float<float>(); }
    explicit operator double() const { return to_native_float<double>(); }
    explicit operator long double() const { return to_native_float<long double>(); }

    auto begin() noexcept { return value_.begin(); }
    auto begin() const noexcept { return value_.begin(); }
    auto end() noexcept { return value_.end(); }
    auto end() const noexcept { return value_.end(); }
    auto rbegin() noexcept { return value_.rbegin(); }
    auto rbegin() const noexcept { return value_.rbegin(); }
    auto rend() noexcept { return value_.rend(); }
    auto rend() const noexcept { return value_.rend(); }

    auto operator[](Range::value_type index) {
        auto offset = offset_of(range_, index);
        if (!offset) {
            throw std::out_of_range("DynUfixed index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }
    auto operator[](Range::value_type index) const {
        auto offset = offset_of(range_, index);
        if (!offset) {
            throw std::out_of_range("DynUfixed index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }
    bool operator==(DynUfixed const& rhs) const noexcept {
        return range_ == rhs.range_ && value_ == rhs.value_;
    }
    std::strong_ordering operator<=>(DynUfixed const& rhs) const {
        require_same_range(rhs);
        dyn_fixed_detail::require_numeric_range(range_);
        return value_ <=> rhs.value_;
    }

    template <typename ShiftType>
    DynUfixed operator<<(ShiftType const& amount) const {
        dyn_fixed_detail::require_numeric_range(range_);
        size_t const shift = dyn_fixed_detail::normalize_dynamic_shift(amount, size());
        return DynUfixed(range_, value_ << shift);
    }
    template <typename ShiftType>
    DynUfixed operator>>(ShiftType const& amount) const {
        dyn_fixed_detail::require_numeric_range(range_);
        size_t const shift = dyn_fixed_detail::normalize_dynamic_shift(amount, size());
        return DynUfixed(range_, value_ >> shift);
    }
    template <typename ShiftType>
    DynUfixed& operator<<=(ShiftType const& amount) {
        return *this = *this << amount;
    }
    template <typename ShiftType>
    DynUfixed& operator>>=(ShiftType const& amount) {
        return *this = *this >> amount;
    }

    DynSfixed operator+() const;
    DynSfixed operator-() const;

    DynUfixed operator+(DynUfixed const& rhs) const {
        Range const result_range = dyn_fixed_detail::add_range(range_, rhs.range_);
        size_t const lhs_shift =
            dyn_fixed_detail::index_distance(range_.right, result_range.right);
        size_t const rhs_shift =
            dyn_fixed_detail::index_distance(rhs.range_.right, result_range.right);
        auto lhs = dyn_fixed_detail::shift_left_widened(value_, lhs_shift);
        auto rhs_value = dyn_fixed_detail::shift_left_widened(rhs.value_, rhs_shift);
        return DynUfixed(result_range, DynUInt(result_range.length(), lhs + rhs_value));
    }

    DynSfixed operator-(DynUfixed const& rhs) const;

    DynUfixed operator*(DynUfixed const& rhs) const {
        Range const result_range = dyn_fixed_detail::multiply_range(range_, rhs.range_);
        return DynUfixed(result_range, value_ * rhs.value_);
    }

    std::pair<DynUfixed, DynUfixed> divrem(
        DynUfixed const& rhs,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        Range const quotient_range =
            dyn_fixed_detail::unsigned_quotient_range(range_, rhs.range_);
        Range::value_type const result_right = std::min(range_.right, rhs.range_.right);
        Range const remainder_range{
            std::min(range_.left, rhs.range_.left), Direction::DOWNTO, result_right
        };
        size_t const lhs_shift =
            dyn_fixed_detail::index_distance(range_.right, result_right);
        size_t const rhs_shift =
            dyn_fixed_detail::index_distance(rhs.range_.right, result_right);
        auto lhs = dyn_fixed_detail::shift_left_widened(value_, lhs_shift);
        auto divisor = dyn_fixed_detail::shift_left_widened(rhs.value_, rhs_shift);
        auto [quotient, remainder] = dyn_fixed_detail::divide_fixed_magnitudes(
            lhs,
            divisor,
            quotient_range.length(),
            quotient_range.right,
            false,
            rounding,
            guard_bits
        );
        return {
            DynUfixed(quotient_range, std::move(quotient)),
            DynUfixed(remainder_range, DynUInt(remainder_range.length(), remainder))
        };
    }

    DynUfixed divide(
        DynUfixed const& rhs,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        return divrem(rhs, rounding, guard_bits).first;
    }

    DynUfixed operator/(DynUfixed const& rhs) const { return divide(rhs); }
    DynUfixed operator%(DynUfixed const& rhs) const { return divrem(rhs).second; }

    DynUfixed& operator+=(DynUfixed const& rhs) {
        dyn_fixed_detail::require_downto(range_);
        dyn_fixed_detail::require_downto(rhs.range_);
        if (size() == 0) {
            return *this;
        }
        return *this = resized(
                   range_, *this + rhs, overflow_mode::wrap, round_mode::round_to_zero
               );
    }
    DynUfixed& operator-=(DynUfixed const& rhs);
    DynUfixed& operator*=(DynUfixed const& rhs) {
        dyn_fixed_detail::require_downto(range_);
        dyn_fixed_detail::require_downto(rhs.range_);
        if (size() == 0) {
            return *this;
        }
        return *this = resized(
                   range_, *this * rhs, overflow_mode::wrap, round_mode::round_to_zero
               );
    }
    DynUfixed& operator/=(DynUfixed const& rhs) {
        dyn_fixed_detail::require_downto(range_);
        dyn_fixed_detail::require_downto(rhs.range_);
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        if (size() == 0) {
            return *this;
        }
        return *this = resized(
                   range_, *this / rhs, overflow_mode::wrap, round_mode::round_to_zero
               );
    }
    DynUfixed& operator%=(DynUfixed const& rhs) {
        dyn_fixed_detail::require_downto(range_);
        dyn_fixed_detail::require_downto(rhs.range_);
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        if (size() == 0) {
            return *this;
        }
        return *this = resized(
                   range_, *this % rhs, overflow_mode::wrap, round_mode::round_to_zero
               );
    }

    template <NativeInteger T>
    DynUfixed& operator+=(T rhs) {
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(rhs, negative);
        DynUfixed operand(int_downto_range(magnitude.width()), std::move(magnitude));
        return negative ? *this -= operand : *this += operand;
    }
    template <NativeInteger T>
    DynUfixed& operator-=(T rhs) {
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(rhs, negative);
        DynUfixed operand(int_downto_range(magnitude.width()), std::move(magnitude));
        return negative ? *this += operand : *this -= operand;
    }
    template <NativeInteger T>
    DynUfixed& operator*=(T rhs) {
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(rhs, negative);
        if (negative && static_cast<bool>(*this)) {
            throw std::out_of_range(
                "Compound arithmetic does not allow a negative DynUfixed result"
            );
        }
        return *this *=
               DynUfixed(int_downto_range(magnitude.width()), std::move(magnitude));
    }
    template <NativeInteger T>
    DynUfixed& operator/=(T rhs) {
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(rhs, negative);
        if (negative && static_cast<bool>(*this)) {
            throw std::out_of_range(
                "Compound arithmetic does not allow a negative DynUfixed result"
            );
        }
        return *this /=
               DynUfixed(int_downto_range(magnitude.width()), std::move(magnitude));
    }
    template <NativeInteger T>
    DynUfixed& operator%=(T rhs) {
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(rhs, negative);
        return *this %=
               DynUfixed(int_downto_range(magnitude.width()), std::move(magnitude));
    }

    DynUfixed& operator++() {
        *this += 1;
        return *this;
    }
    DynUfixed operator++(int) {
        auto copy = *this;
        ++*this;
        return copy;
    }
    DynUfixed& operator--() {
        *this -= 1;
        return *this;
    }
    DynUfixed operator--(int) {
        auto copy = *this;
        --*this;
        return copy;
    }

  private:
    void require_same_range(DynUfixed const& rhs) const {
        if (range_ != rhs.range_) {
            throw std::invalid_argument("DynUfixed comparison requires equal ranges");
        }
    }

    friend class DynSfixed;
    friend struct storage_fn;

    Range range_;
    DynUInt value_;
};

}  // namespace coconext::types::detail

namespace coconext::types {

using DynUfixed = detail::DynUfixed;

template <>
inline constexpr bool is_fixed<detail::DynUfixed> = true;

inline detail::DynUfixed resize(
    detail::DynUfixed const& source,
    Range target,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even
) {
    return detail::DynUfixed::resized(target, source, overflow, rounding);
}

inline detail::DynUfixed divide(
    detail::DynUfixed const& lhs,
    detail::DynUfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divide(rhs, rounding, guard_bits);
}

inline detail::DynUfixed remainder(
    detail::DynUfixed const& lhs,
    detail::DynUfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits).second;
}

inline detail::DynUfixed rem(
    detail::DynUfixed const& lhs,
    detail::DynUfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return remainder(lhs, rhs, rounding, guard_bits);
}

inline detail::DynUfixed modulo(
    detail::DynUfixed const& lhs,
    detail::DynUfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return remainder(lhs, rhs, rounding, guard_bits);
}

inline detail::DynUfixed mod(
    detail::DynUfixed const& lhs,
    detail::DynUfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return remainder(lhs, rhs, rounding, guard_bits);
}

inline std::pair<detail::DynUfixed, detail::DynUfixed> divrem(
    detail::DynUfixed const& lhs,
    detail::DynUfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits);
}

inline std::pair<detail::DynUfixed, detail::DynUfixed> divmod(
    detail::DynUfixed const& lhs,
    detail::DynUfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits);
}

inline detail::DynUfixed reciprocal(
    detail::DynUfixed const& value,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    Range const one_range{0, Direction::DOWNTO, 0};
    return divide(detail::DynUfixed(one_range, 1), value, rounding, guard_bits);
}

inline detail::DynUfixed reverse(detail::DynUfixed const& value) {
    auto raw = value.range().direction == Direction::TO
                 ? detail::dyn_fixed_detail::reverse_bits(detail::storage(value))
                 : detail::DynUInt(value.size(), detail::storage(value));
    return detail::DynUfixed(coconext::types::reverse(value.range()), std::move(raw));
}

}  // namespace coconext::types

template <>
struct std::formatter<coconext::types::detail::DynUfixed> {
    char presentation = 'd';

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            presentation = *it++;
            if (presentation != 'd' && presentation != 'b') {
                throw std::format_error("Invalid format specifier for DynUfixed");
            }
        }
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Invalid format string");
        }
        return it;
    }

    auto format(
        coconext::types::detail::DynUfixed const& value, std::format_context& ctx
    ) const {
        using namespace coconext::types;
        if (presentation == 'd' && value.range().direction != Direction::DOWNTO) {
            throw std::format_error("Decimal format requires DOWNTO direction");
        }
        std::string body = presentation == 'b'
                             ? detail::dyn_fixed_detail::fixed_binary_string(
                                   detail::storage(value), value.range()
                               )
                             : detail::dyn_fixed_detail::fixed_decimal_string(
                                   detail::storage(value), false, value.range().right
                               );
        return std::format_to(ctx.out(), "DynUfixed{}{{{}}}", value.range(), body);
    }
};

template <>
struct std::hash<coconext::types::detail::DynUfixed> {
    size_t operator()(coconext::types::detail::DynUfixed const& value) const {
        using namespace coconext::types;
        return detail::hash_combine(
            std::string_view(typeid(value).name()),
            value.range(),
            detail::storage(value).to_binary_string()
        );
    }
};

#endif  // COCONEXT_DYN_UFIXED_HPP

#ifndef COCONEXT_DYN_SFIXED_HPP
#define COCONEXT_DYN_SFIXED_HPP

#include <algorithm>
#include <climits>
#include <cmath>
#include <coconext/types/dyn_ufixed.hpp>
#include <coconext/types/sfixed.hpp>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>

namespace coconext::types::detail {

class DynSfixed {
    template <NativeInteger T>
    static DynSfixed integer_operand(T value) {
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(value, negative);
        Range range = int_downto_range(magnitude.width());
        return DynSfixed(
            range,
            DynSInt(
                magnitude.width(),
                negative ? dyn_fixed_detail::wrapped_negate(magnitude) : magnitude
            )
        );
    }

    template <NativeInteger T>
    T to_native_int() const {
        dyn_fixed_detail::require_numeric_range(range_);
        constexpr size_t target_width = sizeof(T) * CHAR_BIT;
        bool const negative = value_.is_negative();
        auto magnitude = dyn_fixed_detail::unsigned_magnitude(value_);
        auto aligned =
            dyn_fixed_detail::align_magnitude(magnitude, range_.right, 0, target_width + 1);

        if constexpr (std::numeric_limits<T>::is_signed) {
            DynUInt limit(target_width + 1);
            limit.set_bit(target_width - 1, true);
            bool const out_of_range =
                aligned.overflow
                || (negative ? limit < aligned.bits : !(aligned.bits < limit));
            if (out_of_range) {
                throw std::out_of_range("DynSfixed value does not fit destination integer");
            }
            auto raw = DynUInt(target_width, aligned.bits);
            return DynSInt(
                       target_width, negative ? dyn_fixed_detail::wrapped_negate(raw) : raw
            )
                .template to_native_integer<T>();
        } else {
            if (negative || aligned.overflow || aligned.bits.get_bit(target_width)) {
                throw std::out_of_range("DynSfixed value does not fit destination integer");
            }
            return DynUInt(target_width, aligned.bits).template to_native_integer<T>();
        }
    }

    template <std::floating_point T>
    T to_native_float() const {
        dyn_fixed_detail::require_numeric_range(range_);
        if (value_.width() == 0 || value_.popcount() == 0) {
            return T{0};
        }
        bool const negative = value_.is_negative();
        auto magnitude = dyn_fixed_detail::unsigned_magnitude(value_);
        size_t const msb = magnitude.width() - magnitude.count_leading_zeros() - 1;
        size_t constexpr precision = std::numeric_limits<T>::digits;
        size_t const shift = msb >= precision ? msb - precision + 1 : 0;
        std::uint64_t mantissa = 0;
        size_t const retained = std::min(precision, magnitude.width() - shift);
        for (size_t i = 0; i < retained; ++i) {
            if (magnitude.get_bit(shift + i)) {
                mantissa |= std::uint64_t{1} << i;
            }
        }
        if (shift > 0) {
            bool const round_bit = magnitude.get_bit(shift - 1);
            bool sticky = false;
            for (size_t i = 0; i + 1 < shift; ++i) {
                if (magnitude.get_bit(i)) {
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
            return negative ? -std::numeric_limits<T>::infinity()
                            : std::numeric_limits<T>::infinity();
        }
        if (exponent < std::numeric_limits<int>::min()) {
            return negative ? -T{0} : T{0};
        }
        T result = std::ldexp(static_cast<T>(mantissa), static_cast<int>(exponent));
        return negative ? -result : result;
    }

  public:
    explicit DynSfixed(Range range) : range_(range), value_(range.length()) {}

    DynSfixed(Range range, DynSInt raw) : range_(range), value_(std::move(raw)) {
        dyn_fixed_detail::validate_storage(range_, value_.width());
    }

  private:
    explicit DynSfixed(BitVector const& source)
        : DynSfixed(source.range(), DynSInt(source.size(), bits(source))) {}

    explicit DynSfixed(BitVector&& source)
        : DynSfixed(source.range(), DynSInt(bits(std::move(source)))) {}

  public:
    template <NativeInteger T>
    DynSfixed(Range range, T value) : range_(range), value_(range.length()) {
        dyn_fixed_detail::require_numeric_range(range_);
        bool negative = false;
        auto magnitude = dyn_fixed_detail::native_magnitude(value, negative);
        value_ = dyn_fixed_detail::convert_signed_magnitude(magnitude, negative, 0, range_);
    }

    template <std::floating_point T>
    explicit DynSfixed(
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
            DynUInt sign_bit(size());
            sign_bit.set_bit(size() - 1, true);
            value_ = DynSInt(size(), value < T{0} ? sign_bit : ~sign_bit);
            return;
        }

        bool const negative = value < T{0};
        T const magnitude = negative ? -value : value;
        auto aligned = dyn_fixed_detail::align_floating_magnitude(
            magnitude, range_.right, dyn_fixed_detail::checked_size_add(size(), 1)
        );
        dyn_fixed_detail::round_magnitude(aligned, rounding, negative);
        DynUInt negative_limit(size() + 1);
        negative_limit.set_bit(size() - 1, true);
        bool const out_of_range = aligned.overflow
                               || (negative ? negative_limit < aligned.bits
                                            : !(aligned.bits < negative_limit));
        if (out_of_range && overflow == overflow_mode::saturate) {
            DynUInt sign_bit(size());
            sign_bit.set_bit(size() - 1, true);
            value_ = DynSInt(size(), negative ? sign_bit : ~sign_bit);
        } else {
            auto raw = DynUInt(size(), aligned.bits);
            value_ =
                DynSInt(size(), negative ? dyn_fixed_detail::wrapped_negate(raw) : raw);
        }
    }

    DynSfixed(Range range, DynSigned const& source)
        : range_(range), value_(range.length()) {
        dyn_fixed_detail::require_downto(range_);
        auto const& raw = bits(source);
        bool const negative = raw.width() != 0 && raw.is_negative();
        value_ = dyn_fixed_detail::convert_signed_magnitude(
            dyn_fixed_detail::unsigned_magnitude(raw), negative, 0, range_
        );
    }

    DynSfixed(Range range, DynUnsigned const& source)
        : DynSfixed(
              range,
              dyn_fixed_detail::convert_signed_magnitude(bits(source), false, 0, range)
          ) {}

    template <Range R>
    DynSfixed(Range range, Unsigned<R> const& source)
        : DynSfixed(
              range,
              dyn_fixed_detail::convert_signed_magnitude(
                  DynUInt(bits(source)), false, 0, range
              )
          ) {
        dyn_fixed_detail::require_downto(R);
    }

    template <Range R>
    DynSfixed(Range range, Signed<R> const& source)
        : range_(range), value_(range.length()) {
        dyn_fixed_detail::require_downto(R);
        auto raw = DynSInt(bits(source));
        bool const negative = raw.width() != 0 && raw.is_negative();
        value_ = dyn_fixed_detail::convert_signed_magnitude(
            dyn_fixed_detail::unsigned_magnitude(raw), negative, 0, range_
        );
    }

    DynSfixed(Range range, DynSfixed const& source)
        : range_(range), value_(range.length()) {
        dyn_fixed_detail::require_downto(source.range_);
        bool const negative = source.value_.width() != 0 && source.value_.is_negative();
        value_ = dyn_fixed_detail::convert_signed_magnitude(
            dyn_fixed_detail::unsigned_magnitude(source.value_),
            negative,
            source.range_.right,
            range_
        );
    }

    DynSfixed(Range range, DynUfixed const& source)
        : DynSfixed(
              range,
              dyn_fixed_detail::convert_signed_magnitude(
                  bits(source), false, source.range().right, range
              )
          ) {
        dyn_fixed_detail::require_downto(source.range());
    }

    template <Range R>
    DynSfixed(Range range, Sfixed<R> const& source)
        : range_(range), value_(range.length()) {
        dyn_fixed_detail::require_downto(R);
        auto raw = DynSInt(bits(source));
        bool const negative = raw.width() != 0 && raw.is_negative();
        value_ = dyn_fixed_detail::convert_signed_magnitude(
            dyn_fixed_detail::unsigned_magnitude(raw), negative, R.right, range_
        );
    }

    template <Range R>
    DynSfixed(Range range, Ufixed<R> const& source)
        : DynSfixed(
              range,
              dyn_fixed_detail::convert_signed_magnitude(
                  DynUInt(bits(source)), false, R.right, range
              )
          ) {
        dyn_fixed_detail::require_downto(R);
    }

    static DynSfixed resized(
        Range target,
        DynSfixed const& source,
        overflow_mode overflow = overflow_mode::saturate,
        round_mode rounding = round_mode::round_to_even
    ) {
        dyn_fixed_detail::require_downto(source.range_);
        bool const negative = source.value_.width() != 0 && source.value_.is_negative();
        return DynSfixed(
            target,
            dyn_fixed_detail::resize_signed_magnitude(
                dyn_fixed_detail::unsigned_magnitude(source.value_),
                negative,
                source.range_.right,
                target,
                overflow,
                rounding
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

    std::string raw_decimal() const { return value_.to_decimal_string(true); }
    std::string raw_binary() const { return value_.to_binary_string(); }

    explicit operator bool() const { return value_.popcount() != 0; }
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
            throw std::out_of_range("DynSfixed index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }
    auto operator[](Range::value_type index) const {
        auto offset = offset_of(range_, index);
        if (!offset) {
            throw std::out_of_range("DynSfixed index out of bounds");
        }
        return value_[size() - 1 - *offset];
    }
    bool operator==(DynSfixed const& rhs) const noexcept {
        return range_ == rhs.range_ && value_ == rhs.value_;
    }
    std::strong_ordering operator<=>(DynSfixed const& rhs) const {
        require_same_range(rhs);
        dyn_fixed_detail::require_numeric_range(range_);
        return value_ <=> rhs.value_;
    }

    template <typename ShiftType>
    DynSfixed operator<<(ShiftType const& amount) const {
        dyn_fixed_detail::require_numeric_range(range_);
        size_t const shift = dyn_fixed_detail::normalize_dynamic_shift(amount, size());
        return DynSfixed(range_, value_ << shift);
    }
    template <typename ShiftType>
    DynSfixed operator>>(ShiftType const& amount) const {
        dyn_fixed_detail::require_numeric_range(range_);
        size_t const shift = dyn_fixed_detail::normalize_dynamic_shift(amount, size());
        return DynSfixed(range_, value_ >> shift);
    }
    template <typename ShiftType>
    DynSfixed& operator<<=(ShiftType const& amount) {
        return *this = *this << amount;
    }
    template <typename ShiftType>
    DynSfixed& operator>>=(ShiftType const& amount) {
        return *this = *this >> amount;
    }

    DynSfixed operator+() const {
        dyn_fixed_detail::require_downto(range_);
        return *this;
    }
    DynSfixed operator-() const {
        dyn_fixed_detail::require_downto(range_);
        Range result_range{
            dyn_fixed_detail::checked_add(range_.left, 1), Direction::DOWNTO, range_.right
        };
        return DynSfixed(result_range, -value_);
    }
    DynSfixed abs() const {
        dyn_fixed_detail::require_numeric_range(range_);
        Range result_range{
            dyn_fixed_detail::checked_add(range_.left, 1), Direction::DOWNTO, range_.right
        };
        auto magnitude = dyn_fixed_detail::unsigned_magnitude(value_);
        return DynSfixed(result_range, DynSInt(result_range.length(), magnitude));
    }

    DynSfixed operator+(DynSfixed const& rhs) const {
        Range const result_range = dyn_fixed_detail::add_range(range_, rhs.range_);
        size_t const lhs_shift =
            dyn_fixed_detail::index_distance(range_.right, result_range.right);
        size_t const rhs_shift =
            dyn_fixed_detail::index_distance(rhs.range_.right, result_range.right);
        auto lhs = dyn_fixed_detail::shift_left_widened(value_, lhs_shift);
        auto rhs_value = dyn_fixed_detail::shift_left_widened(rhs.value_, rhs_shift);
        return DynSfixed(result_range, DynSInt(result_range.length(), lhs + rhs_value));
    }
    DynSfixed operator-(DynSfixed const& rhs) const {
        Range const result_range = dyn_fixed_detail::add_range(range_, rhs.range_);
        size_t const lhs_shift =
            dyn_fixed_detail::index_distance(range_.right, result_range.right);
        size_t const rhs_shift =
            dyn_fixed_detail::index_distance(rhs.range_.right, result_range.right);
        auto lhs = dyn_fixed_detail::shift_left_widened(value_, lhs_shift);
        auto rhs_value = dyn_fixed_detail::shift_left_widened(rhs.value_, rhs_shift);
        return DynSfixed(result_range, DynSInt(result_range.length(), lhs - rhs_value));
    }
    DynSfixed operator*(DynSfixed const& rhs) const {
        Range const result_range = dyn_fixed_detail::multiply_range(range_, rhs.range_);
        return DynSfixed(result_range, value_ * rhs.value_);
    }

    std::pair<DynSfixed, DynSfixed> divrem(
        DynSfixed const& rhs,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        return divide_impl(rhs, false, rounding, guard_bits);
    }
    std::pair<DynSfixed, DynSfixed> divmod(
        DynSfixed const& rhs,
        [[maybe_unused]] overflow_mode overflow = overflow_mode::saturate,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        return divide_impl(rhs, true, rounding, guard_bits);
    }
    DynSfixed divide(
        DynSfixed const& rhs,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        return divrem(rhs, rounding, guard_bits).first;
    }
    DynSfixed operator/(DynSfixed const& rhs) const { return divide(rhs); }
    DynSfixed operator%(DynSfixed const& rhs) const { return divrem(rhs).second; }

    DynSfixed& operator+=(DynSfixed const& rhs) {
        dyn_fixed_detail::require_downto(range_);
        dyn_fixed_detail::require_downto(rhs.range_);
        if (size() == 0) {
            return *this;
        }
        return *this = resized(
                   range_, *this + rhs, overflow_mode::wrap, round_mode::round_to_zero
               );
    }
    DynSfixed& operator-=(DynSfixed const& rhs) {
        dyn_fixed_detail::require_downto(range_);
        dyn_fixed_detail::require_downto(rhs.range_);
        if (size() == 0) {
            return *this;
        }
        return *this = resized(
                   range_, *this - rhs, overflow_mode::wrap, round_mode::round_to_zero
               );
    }
    DynSfixed& operator*=(DynSfixed const& rhs) {
        dyn_fixed_detail::require_downto(range_);
        dyn_fixed_detail::require_downto(rhs.range_);
        if (size() == 0) {
            return *this;
        }
        return *this = resized(
                   range_, *this * rhs, overflow_mode::wrap, round_mode::round_to_zero
               );
    }
    DynSfixed& operator/=(DynSfixed const& rhs) {
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
    DynSfixed& operator%=(DynSfixed const& rhs) {
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
    DynSfixed& operator+=(T rhs) {
        return *this += integer_operand(rhs);
    }
    template <NativeInteger T>
    DynSfixed& operator-=(T rhs) {
        return *this -= integer_operand(rhs);
    }
    template <NativeInteger T>
    DynSfixed& operator*=(T rhs) {
        return *this *= integer_operand(rhs);
    }
    template <NativeInteger T>
    DynSfixed& operator/=(T rhs) {
        return *this /= integer_operand(rhs);
    }
    template <NativeInteger T>
    DynSfixed& operator%=(T rhs) {
        return *this %= integer_operand(rhs);
    }

    DynSfixed& operator++() {
        *this += 1;
        return *this;
    }
    DynSfixed operator++(int) {
        auto copy = *this;
        ++*this;
        return copy;
    }
    DynSfixed& operator--() {
        *this -= 1;
        return *this;
    }
    DynSfixed operator--(int) {
        auto copy = *this;
        --*this;
        return copy;
    }

  private:
    std::pair<DynSfixed, DynSfixed> divide_impl(
        DynSfixed const& rhs, bool modulo, round_mode rounding, size_t guard_bits
    ) const {
        Range const quotient_range =
            dyn_fixed_detail::signed_quotient_range(range_, rhs.range_);
        Range::value_type const result_right = std::min(range_.right, rhs.range_.right);
        Range const remainder_range{
            modulo ? rhs.range_.left : std::min(range_.left, rhs.range_.left),
            Direction::DOWNTO,
            result_right
        };
        size_t const lhs_shift =
            dyn_fixed_detail::index_distance(range_.right, result_right);
        size_t const rhs_shift =
            dyn_fixed_detail::index_distance(rhs.range_.right, result_right);
        auto lhs = dyn_fixed_detail::shift_left_widened(value_, lhs_shift);
        auto divisor = dyn_fixed_detail::shift_left_widened(rhs.value_, rhs_shift);
        auto [quotient, remainder] = dyn_fixed_detail::divrem_signed_fixed(
            lhs,
            divisor,
            quotient_range.length(),
            quotient_range.right,
            modulo,
            rounding,
            guard_bits
        );
        return {
            DynSfixed(quotient_range, std::move(quotient)),
            DynSfixed(remainder_range, DynSInt(remainder_range.length(), remainder))
        };
    }

    void require_same_range(DynSfixed const& rhs) const {
        if (range_ != rhs.range_) {
            throw std::invalid_argument("DynSfixed comparison requires equal ranges");
        }
    }

    friend class DynUfixed;
    friend struct bits_fn;
    template <typename>
    friend class auto_reinterpreted;

    Range range_;
    DynSInt value_;
};

template <Range R>
Ufixed<R>::Ufixed(DynUfixed const& other) {
    static_assert(
        R.direction == Direction::DOWNTO,
        "Ufixed construction from DynUfixed requires DOWNTO direction"
    );
    dyn_fixed_detail::require_downto(other.range());
    auto converted =
        dyn_fixed_detail::convert_unsigned_magnitude(bits(other), other.range().right, R);
    value_ = dyn_fixed_detail::copy_to_static_int<R.length(), false>(converted);
}

template <Range R>
Ufixed<R>::Ufixed(DynSfixed const& other) {
    static_assert(
        R.direction == Direction::DOWNTO,
        "Ufixed construction from DynSfixed requires DOWNTO direction"
    );
    dyn_fixed_detail::require_downto(other.range());
    auto const& raw = bits(other);
    if (raw.width() != 0 && raw.is_negative()) {
        throw std::out_of_range("negative value in Ufixed construction");
    }
    auto converted = dyn_fixed_detail::convert_unsigned_magnitude(
        raw.logical_bits(), other.range().right, R
    );
    value_ = dyn_fixed_detail::copy_to_static_int<R.length(), false>(converted);
}

template <Range R>
Sfixed<R>::Sfixed(DynUfixed const& other) {
    static_assert(
        R.direction == Direction::DOWNTO,
        "Sfixed construction from DynUfixed requires DOWNTO direction"
    );
    dyn_fixed_detail::require_downto(other.range());
    auto converted = dyn_fixed_detail::convert_signed_magnitude(
        bits(other), false, other.range().right, R
    );
    value_ =
        dyn_fixed_detail::copy_to_static_int<R.length(), true>(converted.logical_bits());
}

template <Range R>
Sfixed<R>::Sfixed(DynSfixed const& other) {
    static_assert(
        R.direction == Direction::DOWNTO,
        "Sfixed construction from DynSfixed requires DOWNTO direction"
    );
    dyn_fixed_detail::require_downto(other.range());
    auto const& raw = bits(other);
    bool const negative = raw.width() != 0 && raw.is_negative();
    auto converted = dyn_fixed_detail::convert_signed_magnitude(
        dyn_fixed_detail::unsigned_magnitude(raw), negative, other.range().right, R
    );
    value_ =
        dyn_fixed_detail::copy_to_static_int<R.length(), true>(converted.logical_bits());
}

inline DynUfixed::DynUfixed(Range range, DynSigned const& source)
    : range_(range), value_(range.length()) {
    dyn_fixed_detail::require_downto(range_);
    auto const& raw = bits(source);
    if (raw.width() != 0 && raw.is_negative()) {
        throw std::out_of_range("Cannot construct DynUfixed from a negative DynSigned");
    }
    value_ = dyn_fixed_detail::convert_unsigned_magnitude(raw.logical_bits(), 0, range_);
}

template <Range R>
DynUfixed::DynUfixed(Range range, Sfixed<R> const& source)
    : range_(range), value_(range.length()) {
    dyn_fixed_detail::require_downto(R);
    auto raw = DynSInt(bits(source));
    if (raw.width() != 0 && raw.is_negative()) {
        throw std::out_of_range("Cannot construct DynUfixed from a negative Sfixed");
    }
    value_ =
        dyn_fixed_detail::convert_unsigned_magnitude(raw.logical_bits(), R.right, range_);
}

inline DynUfixed::DynUfixed(Range range, DynSfixed const& source)
    : range_(range), value_(range.length()) {
    dyn_fixed_detail::require_downto(range_);
    dyn_fixed_detail::require_downto(source.range_);
    if (source.value_.width() != 0 && source.value_.is_negative()) {
        throw std::out_of_range("Cannot construct DynUfixed from a negative DynSfixed");
    }
    value_ = dyn_fixed_detail::convert_unsigned_magnitude(
        source.value_.logical_bits(), source.range_.right, range_
    );
}

inline DynSfixed DynUfixed::operator+() const {
    dyn_fixed_detail::require_downto(range_);
    Range result_range{
        dyn_fixed_detail::checked_add(range_.left, 1), Direction::DOWNTO, range_.right
    };
    return DynSfixed(result_range, DynSInt(result_range.length(), value_));
}

inline DynSfixed DynUfixed::operator-() const {
    dyn_fixed_detail::require_downto(range_);
    Range result_range{
        dyn_fixed_detail::checked_add(range_.left, 1), Direction::DOWNTO, range_.right
    };
    return DynSfixed(result_range, -value_);
}

inline DynSfixed DynUfixed::operator-(DynUfixed const& rhs) const {
    Range const result_range = dyn_fixed_detail::add_range(range_, rhs.range_);
    size_t const lhs_shift =
        dyn_fixed_detail::index_distance(range_.right, result_range.right);
    size_t const rhs_shift =
        dyn_fixed_detail::index_distance(rhs.range_.right, result_range.right);
    auto lhs_raw = dyn_fixed_detail::shift_left_widened(value_, lhs_shift);
    auto rhs_raw = dyn_fixed_detail::shift_left_widened(rhs.value_, rhs_shift);
    DynSInt lhs(result_range.length(), lhs_raw);
    DynSInt rhs_value(result_range.length(), rhs_raw);
    return DynSfixed(result_range, DynSInt(result_range.length(), lhs - rhs_value));
}

inline DynUfixed& DynUfixed::operator-=(DynUfixed const& rhs) {
    dyn_fixed_detail::require_downto(range_);
    dyn_fixed_detail::require_downto(rhs.range_);
    if (size() == 0) {
        return *this;
    }
    auto difference = *this - rhs;
    if (bits(difference).is_negative()) {
        throw std::out_of_range(
            "Compound subtraction does not allow a negative DynUfixed result"
        );
    }
    value_ = dyn_fixed_detail::resize_unsigned_magnitude(
        bits(difference).logical_bits(),
        difference.range().right,
        range_,
        overflow_mode::wrap,
        round_mode::round_to_zero
    );
    return *this;
}

inline DynSfixed operator+(DynUfixed const& lhs, DynSfixed const& rhs) {
    return +lhs + rhs;
}
inline DynSfixed operator-(DynUfixed const& lhs, DynSfixed const& rhs) {
    return +lhs - rhs;
}
inline DynSfixed operator*(DynUfixed const& lhs, DynSfixed const& rhs) {
    return +lhs * rhs;
}
inline DynSfixed operator/(DynUfixed const& lhs, DynSfixed const& rhs) {
    return +lhs / rhs;
}
inline DynSfixed operator%(DynUfixed const& lhs, DynSfixed const& rhs) {
    return +lhs % rhs;
}
inline DynSfixed operator+(DynSfixed const& lhs, DynUfixed const& rhs) {
    return lhs + +rhs;
}
inline DynSfixed operator-(DynSfixed const& lhs, DynUfixed const& rhs) {
    return lhs - +rhs;
}
inline DynSfixed operator*(DynSfixed const& lhs, DynUfixed const& rhs) {
    return lhs * +rhs;
}
inline DynSfixed operator/(DynSfixed const& lhs, DynUfixed const& rhs) {
    return lhs / +rhs;
}
inline DynSfixed operator%(DynSfixed const& lhs, DynUfixed const& rhs) {
    return lhs % +rhs;
}

inline DynUfixed& operator+=(DynUfixed& lhs, DynSfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    if (lhs.size() == 0) {
        return lhs;
    }
    auto result = +lhs + rhs;
    if (bits(result).is_negative()) {
        throw std::out_of_range(
            "Compound arithmetic does not allow a negative DynUfixed result"
        );
    }
    lhs = DynUfixed::resized(
        lhs.range(),
        DynUfixed(result.range(), result),
        overflow_mode::wrap,
        round_mode::round_to_zero
    );
    return lhs;
}
inline DynUfixed& operator-=(DynUfixed& lhs, DynSfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    if (lhs.size() == 0) {
        return lhs;
    }
    return lhs += -rhs;
}
inline DynUfixed& operator*=(DynUfixed& lhs, DynSfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    if (lhs.size() == 0) {
        return lhs;
    }
    auto result = +lhs * rhs;
    if (bits(result).is_negative()) {
        throw std::out_of_range(
            "Compound arithmetic does not allow a negative DynUfixed result"
        );
    }
    lhs = DynUfixed::resized(
        lhs.range(),
        DynUfixed(result.range(), result),
        overflow_mode::wrap,
        round_mode::round_to_zero
    );
    return lhs;
}
inline DynUfixed& operator/=(DynUfixed& lhs, DynSfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }
    if (lhs.size() == 0) {
        return lhs;
    }
    auto result = +lhs / rhs;
    if (bits(result).is_negative()) {
        throw std::out_of_range(
            "Compound arithmetic does not allow a negative DynUfixed result"
        );
    }
    lhs = DynUfixed::resized(
        lhs.range(),
        DynUfixed(result.range(), result),
        overflow_mode::wrap,
        round_mode::round_to_zero
    );
    return lhs;
}
inline DynUfixed& operator%=(DynUfixed& lhs, DynSfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }
    if (lhs.size() == 0) {
        return lhs;
    }
    auto result = +lhs % rhs;
    if (bits(result).is_negative()) {
        throw std::out_of_range(
            "Compound arithmetic does not allow a negative DynUfixed result"
        );
    }
    lhs = DynUfixed::resized(
        lhs.range(),
        DynUfixed(result.range(), result),
        overflow_mode::wrap,
        round_mode::round_to_zero
    );
    return lhs;
}

inline DynSfixed& operator+=(DynSfixed& lhs, DynUfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    return lhs += +rhs;
}
inline DynSfixed& operator-=(DynSfixed& lhs, DynUfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    return lhs -= +rhs;
}
inline DynSfixed& operator*=(DynSfixed& lhs, DynUfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    return lhs *= +rhs;
}
inline DynSfixed& operator/=(DynSfixed& lhs, DynUfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    return lhs /= +rhs;
}
inline DynSfixed& operator%=(DynSfixed& lhs, DynUfixed const& rhs) {
    dyn_fixed_detail::require_downto(lhs.range());
    dyn_fixed_detail::require_downto(rhs.range());
    return lhs %= +rhs;
}

template <typename T>
concept DynFixedOperand = std::same_as<std::remove_cvref_t<T>, DynUfixed>
                       || std::same_as<std::remove_cvref_t<T>, DynSfixed>;

template <typename T>
concept StaticFixedOperand = is_coconext_ufixed_v<std::remove_cvref_t<T>>
                          || is_coconext_sfixed_v<std::remove_cvref_t<T>>;

template <typename LHS, typename RHS>
concept MixedDynamicStaticFixed = (DynFixedOperand<LHS> && StaticFixedOperand<RHS>)
                               || (StaticFixedOperand<LHS> && DynFixedOperand<RHS>);

template <typename T>
concept DynIntegerOperand = std::same_as<std::remove_cvref_t<T>, DynUnsigned>
                         || std::same_as<std::remove_cvref_t<T>, DynSigned>
                         || is_coconext_unsigned_v<std::remove_cvref_t<T>>
                         || is_coconext_signed_v<std::remove_cvref_t<T>>;

inline DynUfixed integer_as_fixed(DynUnsigned const& value) {
    return DynUfixed(int_downto_range(value.width()), DynUInt(value.width(), bits(value)));
}

inline DynSfixed integer_as_fixed(DynSigned const& value) {
    return DynSfixed(int_downto_range(value.width()), DynSInt(value.width(), bits(value)));
}

template <Range R>
DynUfixed integer_as_fixed(Unsigned<R> const& value) {
    return DynUfixed(int_downto_range(R.length()), DynUInt(bits(value)));
}

template <Range R>
DynSfixed integer_as_fixed(Signed<R> const& value) {
    return DynSfixed(int_downto_range(R.length()), DynSInt(bits(value)));
}

inline DynUfixed const& as_dynamic_fixed(DynUfixed const& value) { return value; }

inline DynSfixed const& as_dynamic_fixed(DynSfixed const& value) { return value; }

template <Range R>
DynUfixed as_dynamic_fixed(Ufixed<R> const& value) {
    return DynUfixed(R, DynUInt(bits(value)));
}

template <Range R>
DynSfixed as_dynamic_fixed(Sfixed<R> const& value) {
    return DynSfixed(R, DynSInt(bits(value)));
}

template <typename LHS, typename RHS>
    requires MixedDynamicStaticFixed<LHS, RHS>
auto operator+(LHS const& lhs, RHS const& rhs) {
    return as_dynamic_fixed(lhs) + as_dynamic_fixed(rhs);
}

template <typename LHS, typename RHS>
    requires MixedDynamicStaticFixed<LHS, RHS>
auto operator-(LHS const& lhs, RHS const& rhs) {
    return as_dynamic_fixed(lhs) - as_dynamic_fixed(rhs);
}

template <typename LHS, typename RHS>
    requires MixedDynamicStaticFixed<LHS, RHS>
auto operator*(LHS const& lhs, RHS const& rhs) {
    return as_dynamic_fixed(lhs) * as_dynamic_fixed(rhs);
}

template <typename LHS, typename RHS>
    requires MixedDynamicStaticFixed<LHS, RHS>
auto operator/(LHS const& lhs, RHS const& rhs) {
    return as_dynamic_fixed(lhs) / as_dynamic_fixed(rhs);
}

template <typename LHS, typename RHS>
    requires MixedDynamicStaticFixed<LHS, RHS>
auto operator%(LHS const& lhs, RHS const& rhs) {
    return as_dynamic_fixed(lhs) % as_dynamic_fixed(rhs);
}

template <StaticFixedOperand LHS, DynFixedOperand Result>
LHS& assign_dynamic_fixed_result(LHS& lhs, Result const& result) {
    using LhsType = std::remove_cvref_t<LHS>;
    using ResultType = std::remove_cvref_t<Result>;

    if constexpr (is_coconext_ufixed_v<LhsType> && std::same_as<ResultType, DynSfixed>) {
        if (result.size() != 0 && bits(result).is_negative()) {
            throw std::out_of_range(
                "compound arithmetic does not allow a negative Ufixed result"
            );
        }
    }

    auto resized = ResultType::resized(
        LhsType::static_range, result, overflow_mode::wrap, round_mode::round_to_zero
    );
    std::ranges::copy(resized, lhs.begin());
    return lhs;
}

template <DynFixedOperand LHS, StaticFixedOperand RHS>
LHS& operator+=(LHS& lhs, RHS const& rhs) {
    return lhs += as_dynamic_fixed(rhs);
}

template <DynFixedOperand LHS, StaticFixedOperand RHS>
LHS& operator-=(LHS& lhs, RHS const& rhs) {
    return lhs -= as_dynamic_fixed(rhs);
}

template <DynFixedOperand LHS, StaticFixedOperand RHS>
LHS& operator*=(LHS& lhs, RHS const& rhs) {
    return lhs *= as_dynamic_fixed(rhs);
}

template <DynFixedOperand LHS, StaticFixedOperand RHS>
LHS& operator/=(LHS& lhs, RHS const& rhs) {
    return lhs /= as_dynamic_fixed(rhs);
}

template <DynFixedOperand LHS, StaticFixedOperand RHS>
LHS& operator%=(LHS& lhs, RHS const& rhs) {
    return lhs %= as_dynamic_fixed(rhs);
}

template <StaticFixedOperand LHS, DynFixedOperand RHS>
LHS& operator+=(LHS& lhs, RHS const& rhs) {
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        return lhs;
    } else {
        return assign_dynamic_fixed_result(lhs, lhs + rhs);
    }
}

template <StaticFixedOperand LHS, DynFixedOperand RHS>
LHS& operator-=(LHS& lhs, RHS const& rhs) {
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        return lhs;
    } else {
        return assign_dynamic_fixed_result(lhs, lhs - rhs);
    }
}

template <StaticFixedOperand LHS, DynFixedOperand RHS>
LHS& operator*=(LHS& lhs, RHS const& rhs) {
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        return lhs;
    } else {
        return assign_dynamic_fixed_result(lhs, lhs * rhs);
    }
}

template <StaticFixedOperand LHS, DynFixedOperand RHS>
LHS& operator/=(LHS& lhs, RHS const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        return lhs;
    } else {
        return assign_dynamic_fixed_result(lhs, lhs / rhs);
    }
}

template <StaticFixedOperand LHS, DynFixedOperand RHS>
LHS& operator%=(LHS& lhs, RHS const& rhs) {
    if (!static_cast<bool>(rhs)) {
        throw std::domain_error("Division by zero");
    }
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        return lhs;
    } else {
        return assign_dynamic_fixed_result(lhs, lhs % rhs);
    }
}

template <DynFixedOperand Fixed, DynIntegerOperand Integer>
auto operator+(Fixed const& lhs, Integer const& rhs) {
    return lhs + integer_as_fixed(rhs);
}
template <DynIntegerOperand Integer, DynFixedOperand Fixed>
auto operator+(Integer const& lhs, Fixed const& rhs) {
    return integer_as_fixed(lhs) + rhs;
}
template <DynFixedOperand Fixed, DynIntegerOperand Integer>
auto operator-(Fixed const& lhs, Integer const& rhs) {
    return lhs - integer_as_fixed(rhs);
}
template <DynIntegerOperand Integer, DynFixedOperand Fixed>
auto operator-(Integer const& lhs, Fixed const& rhs) {
    return integer_as_fixed(lhs) - rhs;
}
template <DynFixedOperand Fixed, DynIntegerOperand Integer>
auto operator*(Fixed const& lhs, Integer const& rhs) {
    return lhs * integer_as_fixed(rhs);
}
template <DynIntegerOperand Integer, DynFixedOperand Fixed>
auto operator*(Integer const& lhs, Fixed const& rhs) {
    return integer_as_fixed(lhs) * rhs;
}
template <DynFixedOperand Fixed, DynIntegerOperand Integer>
auto operator/(Fixed const& lhs, Integer const& rhs) {
    return lhs / integer_as_fixed(rhs);
}
template <DynIntegerOperand Integer, DynFixedOperand Fixed>
auto operator/(Integer const& lhs, Fixed const& rhs) {
    return integer_as_fixed(lhs) / rhs;
}
template <DynFixedOperand Fixed, DynIntegerOperand Integer>
auto operator%(Fixed const& lhs, Integer const& rhs) {
    return lhs % integer_as_fixed(rhs);
}
template <DynIntegerOperand Integer, DynFixedOperand Fixed>
auto operator%(Integer const& lhs, Fixed const& rhs) {
    return integer_as_fixed(lhs) % rhs;
}

template <DynFixedOperand Fixed, DynIntegerOperand Integer>
Fixed& operator+=(Fixed& lhs, Integer const& rhs) {
    lhs += integer_as_fixed(rhs);
    return lhs;
}
template <DynFixedOperand Fixed, DynIntegerOperand Integer>
Fixed& operator-=(Fixed& lhs, Integer const& rhs) {
    lhs -= integer_as_fixed(rhs);
    return lhs;
}
template <DynFixedOperand Fixed, DynIntegerOperand Integer>
Fixed& operator*=(Fixed& lhs, Integer const& rhs) {
    lhs *= integer_as_fixed(rhs);
    return lhs;
}
template <DynFixedOperand Fixed, DynIntegerOperand Integer>
Fixed& operator/=(Fixed& lhs, Integer const& rhs) {
    lhs /= integer_as_fixed(rhs);
    return lhs;
}
template <DynFixedOperand Fixed, DynIntegerOperand Integer>
Fixed& operator%=(Fixed& lhs, Integer const& rhs) {
    lhs %= integer_as_fixed(rhs);
    return lhs;
}

template <NativeInteger T>
auto native_as_fixed(T value) {
    bool negative = false;
    auto magnitude = dyn_fixed_detail::native_magnitude(value, negative);
    Range const range = int_downto_range(magnitude.width());
    if constexpr (std::numeric_limits<T>::is_signed) {
        return DynSfixed(
            range,
            DynSInt(
                magnitude.width(),
                negative ? dyn_fixed_detail::wrapped_negate(magnitude) : magnitude
            )
        );
    } else {
        return DynUfixed(range, std::move(magnitude));
    }
}

template <DynFixedOperand Fixed, NativeInteger Integer>
auto operator+(Fixed const& lhs, Integer rhs) {
    return lhs + native_as_fixed(rhs);
}
template <NativeInteger Integer, DynFixedOperand Fixed>
auto operator+(Integer lhs, Fixed const& rhs) {
    return native_as_fixed(lhs) + rhs;
}
template <DynFixedOperand Fixed, NativeInteger Integer>
auto operator-(Fixed const& lhs, Integer rhs) {
    return lhs - native_as_fixed(rhs);
}
template <NativeInteger Integer, DynFixedOperand Fixed>
auto operator-(Integer lhs, Fixed const& rhs) {
    return native_as_fixed(lhs) - rhs;
}
template <DynFixedOperand Fixed, NativeInteger Integer>
auto operator*(Fixed const& lhs, Integer rhs) {
    return lhs * native_as_fixed(rhs);
}
template <NativeInteger Integer, DynFixedOperand Fixed>
auto operator*(Integer lhs, Fixed const& rhs) {
    return native_as_fixed(lhs) * rhs;
}
template <DynFixedOperand Fixed, NativeInteger Integer>
auto operator/(Fixed const& lhs, Integer rhs) {
    return lhs / native_as_fixed(rhs);
}
template <NativeInteger Integer, DynFixedOperand Fixed>
auto operator/(Integer lhs, Fixed const& rhs) {
    return native_as_fixed(lhs) / rhs;
}
template <DynFixedOperand Fixed, NativeInteger Integer>
auto operator%(Fixed const& lhs, Integer rhs) {
    return lhs % native_as_fixed(rhs);
}
template <NativeInteger Integer, DynFixedOperand Fixed>
auto operator%(Integer lhs, Fixed const& rhs) {
    return native_as_fixed(lhs) % rhs;
}

}  // namespace coconext::types::detail

namespace coconext::types {

using DynSfixed = detail::DynSfixed;

template <>
inline constexpr bool is_fixed<detail::DynSfixed> = true;

inline detail::DynSfixed resize(
    detail::DynSfixed const& source,
    Range target,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even
) {
    return detail::DynSfixed::resized(target, source, overflow, rounding);
}

template <auto... Args>
detail::Ufixed<detail::make_fixed_range<Args...>()> resize(
    detail::DynUfixed const& source,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even
) {
    constexpr Range target = detail::make_fixed_range<Args...>();
    static_assert(
        target.direction == Direction::DOWNTO,
        "resize requires DOWNTO direction for both source and destination."
    );
    return detail::Ufixed<target>(
        detail::DynUfixed::resized(target, source, overflow, rounding)
    );
}

template <auto... Args>
detail::Sfixed<detail::make_fixed_range<Args...>()> resize(
    detail::DynSfixed const& source,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even
) {
    constexpr Range target = detail::make_fixed_range<Args...>();
    static_assert(
        target.direction == Direction::DOWNTO,
        "resize requires DOWNTO direction for both source and destination."
    );
    return detail::Sfixed<target>(
        detail::DynSfixed::resized(target, source, overflow, rounding)
    );
}

template <detail::StaticFixedOperand Target, detail::DynFixedOperand Source>
    requires(
        !std::is_lvalue_reference_v<Source>
        && !std::is_const_v<std::remove_reference_t<Source>>
    )
Target as(Source&& source) {
    if (source.size() != Target::size()) {
        throw std::invalid_argument("as() requires equal widths.");
    }
    auto raw = detail::bits(source).logical_bits();
    return Target(detail::dyn_fixed_detail::copy_to_static_int<Target::size(), false>(raw));
}

inline detail::DynSfixed divide(
    detail::DynSfixed const& lhs,
    detail::DynSfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divide(rhs, rounding, guard_bits);
}

inline detail::DynSfixed remainder(
    detail::DynSfixed const& lhs,
    detail::DynSfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits).second;
}

inline detail::DynSfixed rem(
    detail::DynSfixed const& lhs,
    detail::DynSfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return remainder(lhs, rhs, rounding, guard_bits);
}

inline detail::DynSfixed modulo(
    detail::DynSfixed const& lhs,
    detail::DynSfixed const& rhs,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divmod(rhs, overflow, rounding, guard_bits).second;
}

inline detail::DynSfixed mod(
    detail::DynSfixed const& lhs,
    detail::DynSfixed const& rhs,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return modulo(lhs, rhs, overflow, rounding, guard_bits);
}

inline std::pair<detail::DynSfixed, detail::DynSfixed> divrem(
    detail::DynSfixed const& lhs,
    detail::DynSfixed const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits);
}

inline std::pair<detail::DynSfixed, detail::DynSfixed> divmod(
    detail::DynSfixed const& lhs,
    detail::DynSfixed const& rhs,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divmod(rhs, overflow, rounding, guard_bits);
}

inline detail::DynSfixed reciprocal(
    detail::DynSfixed const& value,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    Range const one_range{1, Direction::DOWNTO, 0};
    auto quotient = divide(detail::DynSfixed(one_range, 1), value, rounding, guard_bits);
    Range const result_range{
        detail::dyn_fixed_detail::checked_add(
            detail::dyn_fixed_detail::checked_sub(0, value.range().right), 1
        ),
        Direction::DOWNTO,
        detail::dyn_fixed_detail::checked_sub(0, value.range().left)
    };
    return detail::DynSfixed(result_range, quotient);
}

inline detail::DynSfixed abs(detail::DynSfixed const& value) { return value.abs(); }

inline detail::DynSfixed reverse(detail::DynSfixed const& value) {
    auto logical = detail::bits(value).logical_bits();
    auto raw = value.range().direction == Direction::TO
                 ? detail::dyn_fixed_detail::reverse_bits(logical)
                 : std::move(logical);
    return detail::DynSfixed(coconext::types::reverse(value.range()), detail::DynSInt(raw));
}

}  // namespace coconext::types

template <>
struct std::formatter<coconext::types::detail::DynSfixed> {
    char presentation = 'd';

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            presentation = *it++;
            if (presentation != 'd' && presentation != 'b') {
                throw std::format_error("Invalid format specifier for DynSfixed");
            }
        }
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Invalid format string");
        }
        return it;
    }

    auto format(
        coconext::types::detail::DynSfixed const& value, std::format_context& ctx
    ) const {
        using namespace coconext::types;
        if (presentation == 'd' && value.range().direction != Direction::DOWNTO) {
            throw std::format_error("Decimal format requires DOWNTO direction");
        }
        bool const negative = value.size() != 0 && detail::bits(value).is_negative();
        auto magnitude = detail::dyn_fixed_detail::unsigned_magnitude(detail::bits(value));
        std::string body = presentation == 'b'
                             ? detail::dyn_fixed_detail::fixed_binary_string(
                                   detail::bits(value).logical_bits(), value.range()
                               )
                             : detail::dyn_fixed_detail::fixed_decimal_string(
                                   std::move(magnitude), negative, value.range().right
                               );
        return std::format_to(ctx.out(), "DynSfixed{}{{{}}}", value.range(), body);
    }
};

template <>
struct std::hash<coconext::types::detail::DynSfixed> {
    size_t operator()(coconext::types::detail::DynSfixed const& value) const {
        using namespace coconext::types;
        return detail::hash_combine(
            std::string_view(typeid(value).name()),
            value.range(),
            detail::bits(value).to_binary_string()
        );
    }
};

#endif  // COCONEXT_DYN_SFIXED_HPP

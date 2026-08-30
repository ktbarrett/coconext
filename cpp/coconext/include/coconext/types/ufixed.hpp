#ifndef COCONEXT_UFIXED_HPP
#define COCONEXT_UFIXED_HPP

#include <cmath>
#include <coconext/types/concepts.hpp>
#include <coconext/types/int_base.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/resize_mode.hpp>
#include <coconext/types/signed.hpp>
#include <coconext/types/unsigned.hpp>

namespace coconext::types {

namespace detail {

template <Range R>
class Ufixed;

template <Range R>
class Sfixed;

template <typename T>
inline constexpr bool is_coconext_ufixed_v = false;

template <Range R>
inline constexpr bool is_coconext_ufixed_v<detail::Ufixed<R>> = true;

template <size_t W>
struct aligned_magnitude {
    UInt<W> bits{};
    bool overflow = false;
    bool discarded = false;
    bool half_bit = false;
    bool lower_bits = false;
};

template <size_t W>
constexpr UInt<W> wrapped_negate(UInt<W> const& value) {
    return UInt<W>::exact_sub(UInt<W>{}, value);
}

// Both endpoints are int64_t, while Range requires size_t to be at least as wide.
// Once their ordering is known, unsigned subtraction therefore gives the full
// mathematical distance without overflowing signed arithmetic.
constexpr size_t index_distance(Range::value_type high, Range::value_type low) noexcept {
    return static_cast<size_t>(high) - static_cast<size_t>(low);
}

template <size_t ResultW, size_t SourceW>
constexpr aligned_magnitude<ResultW> align_magnitude(
    UInt<SourceW> const& source,
    Range::value_type source_right,
    Range::value_type target_right
) {
    static_assert(ResultW > 0);

    aligned_magnitude<ResultW> result;

    if constexpr (SourceW == 0) {
        return result;
    } else if (source_right >= target_right) {
        // The destination has at least as many fractional positions, so the
        // source magnitude is shifted left. Copy only bits represented by the
        // finite result and inspect the rest directly for overflow.
        size_t const shift = index_distance(source_right, target_right);
        size_t const copied_source_bits =
            shift < ResultW ? std::min(SourceW, ResultW - shift) : 0;
        for (size_t i = 0; i < copied_source_bits; ++i) {
            result.bits.set_bit(i + shift, source.get_bit(i));
        }
        for (size_t i = copied_source_bits; i < SourceW; ++i) {
            if (source.get_bit(i)) {
                result.overflow = true;
                break;
            }
        }
    } else {
        // The destination has fewer fractional positions. Model a logical
        // right shift without ever indexing past SourceW, even when the two
        // ranges are separated by billions of binary positions.
        size_t const drop = index_distance(target_right, source_right);
        size_t const remaining = drop < SourceW ? SourceW - drop : 0;
        size_t const copied_source_bits = std::min(remaining, ResultW);
        for (size_t i = 0; i < copied_source_bits; ++i) {
            result.bits.set_bit(i, source.get_bit(i + drop));
        }
        if (remaining > ResultW) {
            size_t const first_overflow_bit = drop + ResultW;
            for (size_t i = first_overflow_bit; i < SourceW; ++i) {
                if (source.get_bit(i)) {
                    result.overflow = true;
                    break;
                }
            }
        }

        result.half_bit = drop <= SourceW && source.get_bit(drop - 1);
        size_t const lower_count = std::min(SourceW, drop - 1);
        for (size_t i = 0; i < lower_count; ++i) {
            if (source.get_bit(i)) {
                result.lower_bits = true;
                break;
            }
        }
        result.discarded = result.half_bit || result.lower_bits;
    }

    return result;
}

template <size_t W>
constexpr void round_magnitude(
    aligned_magnitude<W>& value, round_mode mode, bool negative
) {
    bool round_up = false;
    switch (mode) {
    case round_mode::truncate:
        // "truncate" drops low two's-complement bits, i.e. rounds toward
        // negative infinity for signed values.
        round_up = negative && value.discarded;
        break;
    case round_mode::round_to_zero:
        round_up = false;
        break;
    case round_mode::round_to_pos:
        round_up = !negative && value.discarded;
        break;
    case round_mode::round:
        round_up = value.half_bit;
        break;
    case round_mode::round_to_even:
        round_up = value.half_bit && (value.lower_bits || value.bits.get_bit(0));
        break;
    }

    if (round_up) {
        if (value.bits == ~UInt<W>{}) {
            value.overflow = true;
        }
        value.bits = UInt<W>::exact_add(value.bits, UInt<W>{1});
    }
}

// Divide equally-scaled magnitudes once, returning both the rounded fixed-point
// quotient and the exact integer remainder. QuotientRight is the weight of the
// quotient's least-significant result bit. Additional guard bits only influence
// rounding.
template <
    size_t ResultW,
    Range::value_type QuotientRight,
    size_t DividendW,
    size_t DivisorW>
constexpr std::pair<UInt<ResultW>, UInt<DivisorW>> divide_fixed_magnitudes(
    UInt<DividendW> const& dividend,
    UInt<DivisorW> const& divisor,
    bool negative,
    round_mode rounding,
    size_t guard_bits
) {
    if constexpr (DivisorW == 0) {
        throw std::domain_error("Division by zero");
    } else {
        if (divisor == UInt<DivisorW>{}) {
            throw std::domain_error("Division by zero");
        }

        auto [integer_quotient, remainder] = divrem(dividend, divisor);
        UInt<DivisorW> const exact_remainder = remainder;
        aligned_magnitude<ResultW + 1> rounded;

        auto integer_bit = [&](size_t index) {
            return index < DividendW + 1 && integer_quotient.get_bit(index);
        };

        // Copy every retained position whose weight is integral. The remaining
        // retained positions, if any, are generated from the division remainder
        // below.
        if constexpr (QuotientRight >= 0) {
            constexpr size_t FirstIntegerBit = static_cast<size_t>(QuotientRight);
            for (size_t i = 0; i < ResultW + 1; ++i) {
                rounded.bits.set_bit(i, integer_bit(FirstIntegerBit + i));
            }
        } else {
            constexpr size_t FractionBits = index_distance(0, QuotientRight);
            for (size_t i = FractionBits; i < ResultW + 1; ++i) {
                rounded.bits.set_bit(i, integer_bit(i - FractionBits));
            }
        }

        auto const extended_divisor = UInt<DivisorW + 1>(divisor);
        auto next_quotient_bit = [&] {
            auto doubled = UInt<DivisorW + 1>(remainder) << 1;
            bool const quotient_bit = doubled >= extended_divisor;
            if (quotient_bit) {
                doubled = UInt<DivisorW + 1>::exact_sub(doubled, extended_divisor);
            }
            remainder = doubled.template truncate<DivisorW>();
            return quotient_bit;
        };

        if constexpr (QuotientRight < 0) {
            constexpr size_t FractionBits = index_distance(0, QuotientRight);
            // Generate the quotient's retained fractional positions from -1
            // downward. Positions below a null or all-integral result are still
            // consumed so that guard generation starts at the right weight.
            for (size_t i = FractionBits; i > 0; --i) {
                bool const quotient_bit = next_quotient_bit();
                if (i <= ResultW + 1) {
                    rounded.bits.set_bit(i - 1, quotient_bit);
                }
            }
        }

        // Generate only the requested guard positions. As in fixed_generic_pkg,
        // an exact tail beyond the final guard position is not a sticky bit.
        for (size_t i = 0; i < guard_bits; ++i) {
            bool quotient_bit;
            if constexpr (QuotientRight > 0) {
                constexpr size_t IntegerGuardBits = static_cast<size_t>(QuotientRight);
                quotient_bit = i < IntegerGuardBits ? integer_bit(IntegerGuardBits - i - 1)
                                                    : next_quotient_bit();
            } else {
                quotient_bit = next_quotient_bit();
            }
            if (i == 0) {
                rounded.half_bit = quotient_bit;
            } else {
                rounded.lower_bits = rounded.lower_bits || quotient_bit;
            }
        }
        rounded.discarded = rounded.half_bit || rounded.lower_bits;
        round_magnitude(rounded, rounding, negative);
        return {rounded.bits.template truncate<ResultW>(), exact_remainder};
    }
}

template <
    bool Modulo,
    size_t ResultW,
    Range::value_type QuotientRight,
    size_t DividendW,
    size_t DivisorW>
constexpr std::pair<SInt<ResultW>, SInt<DivisorW>> divrem_signed_fixed(
    SInt<DividendW> const& dividend,
    SInt<DivisorW> const& divisor,
    round_mode rounding,
    size_t guard_bits
) {
    if constexpr (DivisorW == 0) {
        throw std::domain_error("Division by zero");
    } else {
        if (divisor == SInt<DivisorW>{}) {
            throw std::domain_error("Division by zero");
        }

        bool lhs_negative = false;
        UInt<DividendW> lhs_magnitude{};
        if constexpr (DividendW > 0) {
            lhs_negative = dividend.get_bit(DividendW - 1);
            auto const dividend_bits = dividend.logical_bits();
            lhs_magnitude = lhs_negative ? wrapped_negate(dividend_bits) : dividend_bits;
        }

        bool const rhs_negative = divisor.get_bit(DivisorW - 1);
        auto const divisor_bits = divisor.logical_bits();
        UInt<DivisorW> const rhs_magnitude =
            rhs_negative ? wrapped_negate(divisor_bits) : divisor_bits;
        bool const quotient_negative = lhs_negative != rhs_negative;

        auto [quotient_magnitude, remainder_magnitude] =
            divide_fixed_magnitudes<ResultW, QuotientRight>(
                lhs_magnitude, rhs_magnitude, quotient_negative, rounding, guard_bits
            );
        SInt<ResultW> const quotient(
            quotient_negative ? wrapped_negate(quotient_magnitude) : quotient_magnitude
        );

        bool remainder_negative = lhs_negative;
        if constexpr (Modulo) {
            if (remainder_magnitude != UInt<DivisorW>{} && lhs_negative != rhs_negative) {
                remainder_magnitude =
                    UInt<DivisorW>::exact_sub(rhs_magnitude, remainder_magnitude);
                remainder_negative = rhs_negative;
            }
        }
        SInt<DivisorW> const remainder(
            remainder_negative ? wrapped_negate(remainder_magnitude) : remainder_magnitude
        );
        return {quotient, remainder};
    }
}

template <size_t TargetW, size_t SourceW>
constexpr UInt<TargetW> convert_unsigned_magnitude(
    UInt<SourceW> const& source,
    Range::value_type source_right,
    Range::value_type target_right
) {
    auto const aligned = align_magnitude<TargetW + 1>(source, source_right, target_right);
    bool const out_of_range = aligned.overflow || aligned.bits.get_bit(TargetW);
    if (aligned.discarded || out_of_range) {
        throw std::out_of_range(
            "value cannot be represented exactly in destination Ufixed"
        );
    }
    return aligned.bits.template truncate<TargetW>();
}

template <size_t TargetW, size_t SourceW>
constexpr SInt<TargetW> convert_signed_magnitude(
    UInt<SourceW> const& source,
    bool negative,
    Range::value_type source_right,
    Range::value_type target_right
) {
    auto const aligned = align_magnitude<TargetW + 1>(source, source_right, target_right);

    if constexpr (TargetW == 0) {
        if (aligned.discarded || aligned.overflow || aligned.bits != UInt<1>{}) {
            throw std::out_of_range(
                "value cannot be represented exactly in destination Sfixed"
            );
        }
        return {};
    } else {
        UInt<TargetW + 1> const negative_limit = UInt<TargetW + 1>{1} << (TargetW - 1);
        bool const out_of_range =
            aligned.overflow
            || (negative ? aligned.bits > negative_limit : aligned.bits >= negative_limit);
        if (aligned.discarded || out_of_range) {
            throw std::out_of_range(
                "value cannot be represented exactly in destination Sfixed"
            );
        }

        UInt<TargetW> const magnitude = aligned.bits.template truncate<TargetW>();
        return SInt<TargetW>(negative ? wrapped_negate(magnitude) : magnitude);
    }
}

template <size_t ResultW, std::floating_point FloatType>
constexpr aligned_magnitude<ResultW> align_floating_magnitude(
    FloatType magnitude, Range::value_type target_right
) {
    constexpr size_t SignificandW = std::numeric_limits<FloatType>::digits;
    int exponent = 0;
    FloatType const fraction = std::frexp(magnitude, &exponent);
    FloatType significand = std::ldexp(fraction, static_cast<int>(SignificandW));
    UInt<SignificandW> significand_bits{};
    for (size_t bit = 0; bit < SignificandW && significand >= FloatType{1}; ++bit) {
        FloatType const half = std::floor(significand / FloatType{2});
        if (significand - half * FloatType{2} >= FloatType{1}) {
            significand_bits.set_bit(bit, true);
        }
        significand = half;
    }
    Range::value_type const source_right = static_cast<Range::value_type>(exponent)
                                         - static_cast<Range::value_type>(SignificandW);
    return align_magnitude<ResultW>(significand_bits, source_right, target_right);
}

template <size_t TargetW, size_t SourceW>
    requires(TargetW >= SourceW)
constexpr UInt<TargetW> shift_left_zero_extended(
    UInt<SourceW> const& source, size_t amount
) {
    if constexpr (TargetW == 0) {
        return {};
    } else {
        return UInt<TargetW>(source) << amount;
    }
}

template <size_t TargetW, size_t SourceW>
    requires(TargetW >= SourceW)
constexpr SInt<TargetW> shift_left_sign_extended(
    SInt<SourceW> const& source, size_t amount
) {
    if constexpr (TargetW == 0) {
        return {};
    } else {
        return SInt<TargetW>(source) << amount;
    }
}

}  // namespace detail

template <auto... Args, typename X>
    requires(
        sizeof...(Args) > 0 && sizeof...(Args) <= 3
        && detail::is_coconext_ufixed_v<std::remove_cvref_t<X>>
    )
constexpr auto resize(
    X&& x,
    overflow_mode ovf = overflow_mode::saturate,
    round_mode rnd = round_mode::round_to_even
);

namespace detail {

template <Range R>
class Ufixed {
    static constexpr size_t integer_source_bits() noexcept {
        return R.left < 0 ? 0 : static_cast<size_t>(R.left + 1);
    }

    template <NativeInteger T>
    static constexpr bool integer_is_negative(T value) {
        if constexpr (std::is_signed_v<T>) {
            return value < 0;
        }
        return false;
    }

    template <NativeInteger T>
    static constexpr auto integer_magnitude_operand(T value) {
        constexpr size_t SourceW =
            std::numeric_limits<T>::digits + (std::is_signed_v<T> ? 1 : 0);
        UInt<SourceW> const source = [&] {
            if constexpr (std::is_signed_v<T>) {
                return SInt<SourceW>(value).logical_bits();
            } else {
                return UInt<SourceW>(value);
            }
        }();
        UInt<SourceW> const magnitude =
            integer_is_negative(value) ? wrapped_negate(source) : source;
        constexpr Range IntegerRange{
            static_cast<Range::value_type>(SourceW) - 1, Direction::DOWNTO, 0
        };
        return Ufixed<IntegerRange>(magnitude);
    }

    template <size_t SourceW>
    constexpr void assign_unsigned_integer(UInt<SourceW> const& source) {
        value_ = detail::convert_unsigned_magnitude<R.length()>(source, 0, R.right);
    }

    template <size_t SourceW>
    constexpr void assign_signed_integer(SInt<SourceW> const& source) {
        if constexpr (SourceW > 0) {
            if (source.get_bit(SourceW - 1)) {
                throw std::out_of_range("negative value in Ufixed construction");
            }
        }
        assign_unsigned_integer(source.logical_bits());
    }

    template <typename T>
    constexpr T to_native_int() const {
        static_assert(
            R.length() > 0, "Ufixed<0> has no integer value, cannot convert to native int"
        );

        if constexpr (R.right > 0) {
            constexpr Range IntegerRange{R.left, Direction::DOWNTO, 0};
            // TODO inefficient
            auto scaled = UInt<IntegerRange.length()>(value_);
            scaled = scaled << R.right;
            return static_cast<T>(Ufixed<IntegerRange>(scaled));
        }

        auto val = value_ >> frac_bits();
        if constexpr (int_bits() > std::numeric_limits<T>::digits) {
            if (val > UInt<R.length()>(std::numeric_limits<T>::max())) {
                throw std::out_of_range("Value too large for destination native type");
            }
        }

        if constexpr (!UInt<R.length()>::is_wide) {
            return static_cast<T>(val.raw());
        } else {
            return static_cast<T>(val.raw().word(0));
        }
    }

    template <typename T>
    constexpr T to_native_float() const noexcept {
        static_assert(
            R.length() > 0, "Ufixed<0> has no value, cannot convert to native float"
        );

        if constexpr (!UInt<R.length()>::is_wide) {
            T base_val = static_cast<T>(value_.raw());
            return std::ldexp(base_val, R.right);  // base_val * 2^(R.right)
        } else {
            if (value_ == UInt<R.length()>{0}) {
                return T{0.0};
            }

            int msb_index = value_.highest_set_index();
            constexpr int mantissa_bits = std::numeric_limits<T>::digits;

            int shift_amount = 0;
            if (msb_index >= mantissa_bits) {
                shift_amount = msb_index - mantissa_bits + 1;
            }

            auto aligned = value_ >> shift_amount;
            uint64_t raw_mantissa = static_cast<uint64_t>(aligned.raw().word(0));

            if (shift_amount > 0) {
                bool round_bit = value_.get_bit(shift_amount - 1);
                bool sticky_bit = false;

                for (int i = 0; i < shift_amount - 1; ++i) {
                    if (value_.get_bit(i)) {
                        sticky_bit = true;
                        break;
                    }
                }

                if (round_bit && (sticky_bit || (raw_mantissa & 1))) {
                    raw_mantissa += 1;
                }
            }

            T base_val = static_cast<T>(raw_mantissa);
            return std::ldexp(base_val, R.right + shift_amount);
        }
    }

  public:
    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return R; }
    static constexpr size_t size() noexcept { return R.length(); }

    constexpr Ufixed() noexcept = default;

    template <size_t W, bool SignedRepresentation>
    constexpr Ufixed(Int<W, SignedRepresentation> const& val) : value_(val) {
        static_assert(W == R.length(), "Construction from Int requires identical width");
    }

    // Construct from a native integer
    template <NativeInteger T>
    explicit(
        std::is_signed_v<T> || (std::numeric_limits<T>::digits > integer_source_bits())
        || R.right > 0
    ) constexpr Ufixed(T v) {
        static_assert(
            R.direction == Direction::DOWNTO,
            "Construction from int not Allowed on a TO Direction Ufixed"
        );
        static_assert(
            R.length() > 0,
            "Ufixed with a null range has no integer representation; use default "
            "construction"
        );
        if constexpr (std::is_signed_v<T>) {
            constexpr size_t SourceW = std::numeric_limits<T>::digits + 1;
            assign_signed_integer(SInt<SourceW>(v));
        } else {
            constexpr size_t SourceW = std::numeric_limits<T>::digits;
            assign_unsigned_integer(UInt<SourceW>(v));
        }
    }

    // Exact numeric conversion from the same kind. Conversions that are guaranteed
    // lossless for every source value are implicit; other conversions are checked.
    template <Range R2>
    explicit(
        !(R2.length() == 0 || (R.length() > 0 && R.left >= R2.left && R.right <= R2.right))
    ) constexpr Ufixed(Ufixed<R2> const& other) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Ufixed same-kind construction requires DOWNTO direction"
        );
        value_ =
            detail::convert_unsigned_magnitude<R.length()>(bits(other), R2.right, R.right);
    }

    // Exact numeric conversion from Sfixed.
    template <Range R2>
    explicit constexpr Ufixed(Sfixed<R2> const& other) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Ufixed cross-kind construction requires DOWNTO direction"
        );

        auto other_bits = bits(other);
        bool is_negative = false;
        if constexpr (R2.length() > 0) {
            is_negative = other_bits.get_bit(R2.length() - 1);
        }

        if (is_negative) {
            throw std::out_of_range("negative value in Ufixed construction");
        }
        value_ = detail::convert_unsigned_magnitude<R.length()>(
            other_bits.logical_bits(), R2.right, R.right
        );
    }

    // Construct from float
    template <std::floating_point FloatType>
    explicit constexpr Ufixed(
        FloatType v,
        overflow_mode om = overflow_mode::saturate,
        round_mode rm = round_mode::round_to_even
    ) {
        static_assert(
            R.direction == Direction::DOWNTO,
            "Construction from a float/double not Allowed on a TO Direction Ufixed"
        );
        static_assert(
            R.length() > 0,
            "Ufixed with a null range has no floating-point representation; use default "
            "construction"
        );
        if (std::isnan(v)) {
            throw std::domain_error("Cannot convert NaN to fixed-point.");
        }
        if (std::isinf(v)) {
            if (om == overflow_mode::wrap) {
                throw std::domain_error("Cannot wrap Infinity.");
            }
            if (v > 0) {
                value_ = ~UInt<R.length()>(0);
            } else {
                value_ = UInt<R.length()>(0);
            }
            return;
        }

        if (v < 0.0) {
            throw std::out_of_range("Cannot construct Ufixed from negative float type");
        }

        auto aligned = detail::align_floating_magnitude<R.length() + 1>(v, R.right);
        detail::round_magnitude(aligned, rm, false);

        bool const out_of_range = aligned.overflow || aligned.bits.get_bit(R.length());
        if (out_of_range && om == overflow_mode::saturate) {
            value_ = ~UInt<R.length()>{};
        } else {
            value_ = aligned.bits.template truncate<R.length()>();
        }
    }

    // Construction from Unsigned
    template <Range R2>
    explicit(R2.length() > integer_source_bits() || R.right > 0) constexpr Ufixed(
        Unsigned<R2> const& v
    )
        requires(R.direction == Direction::DOWNTO)
    {
        assign_unsigned_integer(bits(v));
    }

    // Construction from Signed
    template <Range R2>
    explicit constexpr Ufixed(Signed<R2> const& v)
        requires(R.direction == Direction::DOWNTO)
    {
        assign_signed_integer(bits(v));
    }

    template <typename SourceWrapper>
    constexpr Ufixed(detail::auto_resized<SourceWrapper>&& wrapper) {
        auto [src, ovf, rnd] = std::move(wrapper).consume();
        using ActualSource = std::remove_cvref_t<decltype(src)>;

        static_assert(
            detail::is_coconext_ufixed_v<ActualSource>,
            "resize() target and source must both be Ufixed. Use as() for cross-type "
            "conversions."
        );

        constexpr Range R2 = ActualSource::range();
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "resize requires DOWNTO direction for both source and destination."
        );

        auto aligned =
            detail::align_magnitude<R.length() + 1>(bits(src), R2.right, R.right);
        detail::round_magnitude(aligned, rnd, false);

        bool const out_of_range = aligned.overflow || aligned.bits.get_bit(R.length());
        if (out_of_range && ovf == overflow_mode::saturate) {
            value_ = ~UInt<R.length()>{};
        } else {
            value_ = aligned.bits.template truncate<R.length()>();
        }
    }

    template <typename SourceWrapper>
    constexpr Ufixed& operator=(detail::auto_resized<SourceWrapper>&& wrapper) {
        *this = Ufixed(std::move(wrapper));
        return *this;
    }

    template <Range R2>
    constexpr operator detail::Array<Bit, R2>() const noexcept {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );
        return detail::Array<Bit, R2>(value_);
    }

    explicit constexpr operator bool() const noexcept
        requires(R.direction == Direction::DOWNTO)
    {
        return value_ != UInt<R.length()>{};
    }

    explicit constexpr operator signed char() const noexcept(
        R.left < std::numeric_limits<signed char>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<signed char>();
    }
    explicit constexpr operator unsigned char() const noexcept(
        R.left < std::numeric_limits<unsigned char>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned char>();
    }
    explicit constexpr operator short() const noexcept(
        R.left < std::numeric_limits<short>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<short>();
    }
    explicit constexpr operator unsigned short() const noexcept(
        R.left < std::numeric_limits<unsigned short>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned short>();
    }
    explicit constexpr operator int() const noexcept(
        R.left < std::numeric_limits<int>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<int>();
    }
    explicit constexpr operator unsigned int() const noexcept(
        R.left < std::numeric_limits<unsigned int>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned int>();
    }
    explicit constexpr operator long() const noexcept(
        R.left < std::numeric_limits<long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<long>();
    }
    explicit constexpr operator unsigned long() const noexcept(
        R.left < std::numeric_limits<unsigned long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned long>();
    }
    explicit constexpr operator long long() const noexcept(
        R.left < std::numeric_limits<long long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<long long>();
    }
    explicit constexpr operator unsigned long long() const noexcept(
        R.left < std::numeric_limits<unsigned long long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned long long>();
    }
#if defined(__SIZEOF_INT128__)
    explicit constexpr operator __int128_t() const noexcept(
        R.left < std::numeric_limits<__int128_t>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<__int128_t>();
    }
    explicit constexpr operator __uint128_t() const noexcept(
        R.left < std::numeric_limits<__uint128_t>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<__uint128_t>();
    }
#endif
    explicit constexpr operator float() const noexcept
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_float<float>();
    }
    explicit constexpr operator double() const noexcept
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_float<double>();
    }
    explicit constexpr operator long double() const noexcept
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_float<long double>();
    }

    template <typename T>
    constexpr Ufixed operator<<(T amount) const {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(R.length() > 0, "shift on Ufixed with a null range is undefined");
        static_assert(
            std::is_integral_v<T> || is_coconext_unsigned_v<T> || is_coconext_signed_v<T>,
            "Shift Amount can only be purely Integral"
        );
        if constexpr (std::is_signed_v<T> || is_coconext_signed_v<T>) {
            if (static_cast<int64_t>(amount) < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        uint64_t v = static_cast<unsigned long long>(amount);
        if (v < 0) {
            throw std::invalid_argument("Shift amount cannot be negative");
        }

        return Ufixed<R>(value_ << v);
    }

    template <typename T>
    constexpr Ufixed& operator<<=(T amount) {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(R.length() > 0, "shift on Ufixed with a null range is undefined");
        static_assert(
            std::is_integral_v<T> || is_coconext_unsigned_v<T> || is_coconext_signed_v<T>,
            "Shift Amount can only be purely Integral"
        );
        if constexpr (std::is_signed_v<T> || is_coconext_signed_v<T>) {
            if (static_cast<int64_t>(amount) < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        uint64_t v = static_cast<unsigned long long>(amount);
        if (v < 0) {
            throw std::invalid_argument("Shift amount cannot be negative");
        }

        value_ = value_ << v;
        return *this;
    }

    template <typename T>
    constexpr Ufixed operator>>(T amount) const {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(R.length() > 0, "shift on Ufixed with a null range is undefined");
        static_assert(
            std::is_integral_v<T> || is_coconext_unsigned_v<T> || is_coconext_signed_v<T>,
            "Shift Amount can only be purely Integral"
        );
        if constexpr (std::is_signed_v<T> || is_coconext_signed_v<T>) {
            if (static_cast<int64_t>(amount) < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        uint64_t v = static_cast<unsigned long long>(amount);
        if (v < 0) {
            throw std::invalid_argument("Shift amount cannot be negative");
        }

        return Ufixed<R>(value_ >> v);
    }

    template <typename T>
    constexpr Ufixed& operator>>=(T amount) {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(R.length() > 0, "shift on Ufixed with a null range is undefined");
        static_assert(
            std::is_integral_v<T> || is_coconext_unsigned_v<T> || is_coconext_signed_v<T>,
            "Shift Amount can only be purely Integral"
        );
        if constexpr (std::is_signed_v<T> || is_coconext_signed_v<T>) {
            if (static_cast<int64_t>(amount) < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        uint64_t v = static_cast<unsigned long long>(amount);
        if (v < 0) {
            throw std::invalid_argument("Shift amount cannot be negative");
        }

        value_ = value_ >> v;
        return *this;
    }

    template <Range R2>
    constexpr std::strong_ordering operator<=>(Ufixed<R2> const& other) const noexcept
        requires(R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO)
    {
        static_assert(R == R2, "Comparison requires equal Ranges");
        static_assert(R.length() > 0, "ordering a Ufixed with a null range is undefined");

        if (value_ == other.value_) {
            return std::strong_ordering::equal;
        }
        if (value_ > other.value_) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::less;
    }

    template <Range R2>
    constexpr bool operator==(Ufixed<R2> const& other) const noexcept {
        static_assert(R == R2, "Comparison requires equal Ranges");
        if (value_ == other.value_) {
            return true;
        }

        return false;
    }

    constexpr auto operator+() const {
        static_assert(
            R.direction == Direction::DOWNTO,
            "All arithmetic operations require downto Direction"
        );

        constexpr auto TR = Range{R.left + 1, R.direction, R.right};

        return Sfixed<TR>(*this);
    }

    constexpr auto operator-() const {
        static_assert(
            R.direction == Direction::DOWNTO,
            "All arithmetic operations require downto Direction"
        );

        constexpr auto TR = Range{R.left + 1, R.direction, R.right};
        constexpr size_t TargetW = TR.length();

        auto extended_bits = UInt<TargetW>(value_);
        auto const signed_bits = SInt<TargetW>(extended_bits);
        return Sfixed<TR>(SInt<TargetW>::exact_sub(SInt<TargetW>{}, signed_bits));
    }

    template <Range R2>
    constexpr auto operator+(Ufixed<R2> const& rhs) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range R_res{
            std::max(R.left, R2.left) + 1, Direction::DOWNTO, std::min(R.right, R2.right)
        };
        constexpr size_t ShiftL = R.right - R_res.right;
        constexpr size_t ShiftR = R2.right - R_res.right;

        auto lhs_aligned =
            detail::shift_left_zero_extended<R.length() + ShiftL>(value_, ShiftL);
        auto rhs_aligned =
            detail::shift_left_zero_extended<R2.length() + ShiftR>(bits(rhs), ShiftR);

        return Ufixed<R_res>(lhs_aligned + rhs_aligned);
    }

    template <Range R2>
    constexpr auto operator-(Ufixed<R2> const& rhs) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range R_res{
            std::max(R.left, R2.left) + 1, Direction::DOWNTO, std::min(R.right, R2.right)
        };
        constexpr size_t ShiftL = R.right - R_res.right;
        constexpr size_t ShiftR = R2.right - R_res.right;

        auto lhs_aligned =
            detail::shift_left_zero_extended<R.length() + ShiftL>(value_, ShiftL);
        auto rhs_aligned =
            detail::shift_left_zero_extended<R2.length() + ShiftR>(bits(rhs), ShiftR);

        return Sfixed<R_res>(lhs_aligned - rhs_aligned);
    }

    template <Range R2>
    constexpr auto operator*(Ufixed<R2> const& rhs) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range R_res{R.left + R2.left + 1, Direction::DOWNTO, R.right + R2.right};
        return Ufixed<R_res>(value_ * bits(rhs));
    }

    template <Range R2>
    constexpr auto operator/(Ufixed<R2> const& rhs) const {
        return divide(rhs);
    }

    template <Range R2>
    constexpr auto divide(
        Ufixed<R2> const& rhs,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        return divrem(rhs, rounding, guard_bits).first;
    }

    template <Range R2>
    constexpr auto divrem(
        Ufixed<R2> const& rhs,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range QuotientRange{
            R.left - R2.right, Direction::DOWNTO, R.right - R2.left - 1
        };
        constexpr auto result_right = std::min(R.right, R2.right);
        constexpr Range RemainderRange{
            std::min(R.left, R2.left), Direction::DOWNTO, result_right
        };
        constexpr size_t ShiftL = detail::index_distance(R.right, result_right);
        constexpr size_t ShiftR = detail::index_distance(R2.right, result_right);

        auto lhs_aligned =
            detail::shift_left_zero_extended<R.length() + ShiftL>(value_, ShiftL);
        auto rhs_aligned =
            detail::shift_left_zero_extended<R2.length() + ShiftR>(bits(rhs), ShiftR);
        auto [quotient_bits, remainder_bits] =
            detail::divide_fixed_magnitudes<QuotientRange.length(), QuotientRange.right>(
                lhs_aligned, rhs_aligned, false, rounding, guard_bits
            );

        constexpr size_t RemainderW = RemainderRange.length();
        constexpr size_t AlignedDivisorW = R2.length() + ShiftR;
        if constexpr (RemainderW <= AlignedDivisorW) {
            return std::pair{
                Ufixed<QuotientRange>(quotient_bits),
                Ufixed<RemainderRange>(remainder_bits.template truncate<RemainderW>())
            };
        } else {
            return std::pair{
                Ufixed<QuotientRange>(quotient_bits),
                Ufixed<RemainderRange>(UInt<RemainderW>(remainder_bits))
            };
        }
    }

    template <Range R2>
    constexpr auto operator%(Ufixed<R2> const& rhs) const {
        return divrem(rhs).second;
    }

    template <Range R2>
    constexpr Ufixed& operator+=(Ufixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this + rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }
    template <Range R2>
    constexpr Ufixed& operator-=(Ufixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if constexpr (R.length() == 0) {
            return *this;
        }
        auto diff = *this - rhs;

        if (bits(diff).get_bit(decltype(diff)::size() - 1)) {
            throw std::out_of_range(
                "Compound subtraction does not allow a negative result"
            );
        }

        constexpr auto DiffRange = decltype(diff)::static_range;
        *this = coconext::types::resize<R>(
            Ufixed<DiffRange>(diff), overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }
    template <Range R2>
    constexpr Ufixed& operator*=(Ufixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this * rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }
    template <Range R2>
    constexpr Ufixed& operator/=(Ufixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this / rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }
    template <Range R2>
    constexpr Ufixed& operator%=(Ufixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this % rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }

    template <NativeInteger T>
    constexpr Ufixed& operator+=(T const& rhs) {
        auto const magnitude = integer_magnitude_operand(rhs);
        if (integer_is_negative(rhs)) {
            *this -= magnitude;
        } else {
            *this += magnitude;
        }
        return *this;
    }
    template <NativeInteger T>
    constexpr Ufixed& operator-=(T const& rhs) {
        auto const magnitude = integer_magnitude_operand(rhs);
        if (integer_is_negative(rhs)) {
            *this += magnitude;
        } else {
            *this -= magnitude;
        }
        return *this;
    }
    template <NativeInteger T>
    constexpr Ufixed& operator*=(T const& rhs) {
        auto const magnitude = integer_magnitude_operand(rhs);
        if (integer_is_negative(rhs) && static_cast<bool>(*this)) {
            throw std::out_of_range(
                "compound arithmetic does not allow a negative Ufixed result"
            );
        }
        *this *= magnitude;
        return *this;
    }
    template <NativeInteger T>
    constexpr Ufixed& operator/=(T const& rhs) {
        auto const magnitude = integer_magnitude_operand(rhs);
        if (integer_is_negative(rhs) && static_cast<bool>(*this)) {
            throw std::out_of_range(
                "compound arithmetic does not allow a negative Ufixed result"
            );
        }
        *this /= magnitude;
        return *this;
    }
    template <NativeInteger T>
    constexpr Ufixed& operator%=(T const& rhs) {
        *this %= integer_magnitude_operand(rhs);
        return *this;
    }

    constexpr Ufixed& operator++() {
        *this += 1;
        return *this;
    }
    constexpr Ufixed operator++(int) {
        Ufixed tmp = *this;
        *this += 1;
        return tmp;
    }
    constexpr Ufixed& operator--() {
        *this -= 1;
        return *this;
    }
    constexpr Ufixed operator--(int) {
        Ufixed tmp = *this;
        *this -= 1;
        return tmp;
    }

    static constexpr Range::value_type frac_bits() {
        auto const lsb = R.direction == Direction::DOWNTO ? R.right : R.left;
        return -lsb;
    }

    static constexpr Range::value_type int_bits() {
        auto const msb = R.direction == Direction::DOWNTO ? R.left : R.right;
        return msb + 1;
    }

    static constexpr double resolution() {
        long long exp = (R.direction == Direction::DOWNTO) ? R.right : R.left;
        double result = 1.0;

        if (exp > 0) {
            for (long long i = 0; i < exp; ++i) {
                result *= 2.0;
            }
        } else if (exp < 0) {
            for (long long i = 0; i > exp; --i) {
                result /= 2.0;
            }
        }

        return result;
    }

    constexpr auto begin() { return value_.begin(); }
    constexpr auto rbegin() { return value_.rbegin(); }
    constexpr auto begin() const { return value_.begin(); }
    constexpr auto rbegin() const { return value_.rbegin(); }

    constexpr auto end() { return value_.end(); }
    constexpr auto rend() { return value_.rend(); }
    constexpr auto end() const { return value_.end(); }
    constexpr auto rend() const { return value_.rend(); }

    constexpr auto operator[](Range::value_type idx) {
        auto const offset = offset_of(R, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        size_t bit_pos = R.length() - 1 - offset.value();

        return value_[bit_pos];
    }

    constexpr auto operator[](Range::value_type idx) const {
        auto const offset = offset_of(R, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        size_t bit_pos = R.length() - 1 - offset.value();

        return value_[bit_pos];
    }

  private:
    friend struct bits_fn;

    UInt<R.length()> value_{};
};

template <Range R1, Range R2>
constexpr auto operator+(Ufixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a + Ufixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator-(Ufixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a - Ufixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator*(Ufixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a * Ufixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator/(Ufixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a / Ufixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator%(Ufixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a % Ufixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator+(Unsigned<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Ufixed<f_range>(a) + b;
}

template <Range R1, Range R2>
constexpr auto operator-(Unsigned<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Ufixed<f_range>(a) - b;
}

template <Range R1, Range R2>
constexpr auto operator*(Unsigned<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Ufixed<f_range>(a) * b;
}

template <Range R1, Range R2>
constexpr auto operator/(Unsigned<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Ufixed<f_range>(a) / b;
}

template <Range R1, Range R2>
constexpr auto operator%(Unsigned<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Ufixed<f_range>(a) % b;
}

}  // namespace detail

template <Range R>
inline constexpr bool is_fixed<detail::Ufixed<R>> = true;

// see detail::make_fixed_range for the rules
template <auto... Args>
using Ufixed = detail::Ufixed<detail::make_fixed_range<Args...>()>;

// VHDL fixed_pkg-style division helpers. The result ranges are the same as the
// corresponding operators. Guard bits affect the quotient only; fixed-point
// remainder and modulo are exactly representable at their prescribed range.
template <Range R1, Range R2>
constexpr auto divide(
    detail::Ufixed<R1> const& lhs,
    detail::Ufixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divide(rhs, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto remainder(
    detail::Ufixed<R1> const& lhs,
    detail::Ufixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits).second;
}

template <Range R1, Range R2>
constexpr auto rem(
    detail::Ufixed<R1> const& lhs,
    detail::Ufixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return remainder(lhs, rhs, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto modulo(
    detail::Ufixed<R1> const& lhs,
    detail::Ufixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return remainder(lhs, rhs, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto mod(
    detail::Ufixed<R1> const& lhs,
    detail::Ufixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return modulo(lhs, rhs, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto divrem(
    detail::Ufixed<R1> const& lhs,
    detail::Ufixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto divmod(
    detail::Ufixed<R1> const& lhs,
    detail::Ufixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits);
}

template <Range R>
constexpr auto reciprocal(
    detail::Ufixed<R> const& value,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return divide(Ufixed<0, 0>{1}, value, rounding, guard_bits);
}

template <auto... Args, typename X>
    requires(
        sizeof...(Args) > 0 && sizeof...(Args) <= 3
        && detail::is_coconext_ufixed_v<std::remove_cvref_t<X>>
    )
constexpr auto resize(X&& x, overflow_mode ovf, round_mode rnd) {
    constexpr Range TargetRange = detail::make_fixed_range<Args...>();
    return Ufixed<TargetRange>(detail::resize(std::forward<X>(x), ovf, rnd));
}

template <Range R>
constexpr auto reverse(detail::Ufixed<R> const& v) noexcept {
    if constexpr (R.direction == Direction::TO) {
        return detail::Ufixed<reverse(R)>(detail::bits(v).reverse());
    } else {
        return detail::Ufixed<reverse(R)>(detail::bits(v));
    }
}

}  // namespace coconext::types

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Ufixed<R>> {
    char presentation = 'd';

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') {
            presentation = *it++;
            if (presentation != 'd' && presentation != 'b') {
                throw std::format_error("Invalid format specifier for Ufixed");
            }
        }
        if (it != end && *it != '}') {
            throw std::format_error("Invalid format string");
        }

        if (presentation == 'd' && R.direction != coconext::types::Direction::DOWNTO) {
            throw std::format_error("Decimal format requires downto Direction");
        }

        return it;
    }

    auto format(
        coconext::types::detail::Ufixed<R> const& v, std::format_context& ctx
    ) const {
        std::string str_r;
        switch (presentation) {
        case 'b': {
            constexpr auto F = coconext::types::detail::Ufixed<R>::frac_bits();
            constexpr auto I = coconext::types::detail::Ufixed<R>::int_bits();
            constexpr size_t decimal_pos = F > 0 && I > 0 ? static_cast<size_t>(F) : 0;
            str_r = coconext::types::detail::bits(v).to_binary_string(decimal_pos);
            break;
        }
        default: {
            constexpr auto F = coconext::types::detail::Ufixed<R>::frac_bits();
            str_r =
                coconext::types::detail::bits(v).template to_fixed_decimal_string<F>(false);
            break;
        }
        }
        return std::format_to(ctx.out(), "Ufixed{}{{{}}}", R, str_r);
    }
};

template <coconext::types::Range R>
struct std::hash<coconext::types::detail::Ufixed<R>> {
    size_t operator()(coconext::types::detail::Ufixed<R> const& v) const noexcept {
        std::string_view type_name = typeid(coconext::types::detail::Ufixed<R>).name();
        size_t ufixed_seed = std::hash<std::string_view>{}(type_name);
        constexpr size_t W = R.length();
        size_t value_hash = 0;

        if constexpr (W > 0) {
            if constexpr (!coconext::types::detail::UInt<W>::is_wide) {
                auto raw_val = coconext::types::detail::bits(v).raw();
                if constexpr (sizeof(raw_val) > sizeof(size_t)) {
                    uint64_t low = static_cast<uint64_t>(raw_val);
                    uint64_t high = static_cast<uint64_t>(raw_val >> 64);
                    value_hash = coconext::types::detail::hash_combine(low, high);
                } else {
                    value_hash = std::hash<decltype(raw_val)>{}(raw_val);
                }
            } else {
                auto val = coconext::types::detail::bits(v).raw();
                constexpr size_t num_words = (W + 63) / 64;
                for (size_t i = 0; i < num_words; ++i) {
                    value_hash =
                        coconext::types::detail::hash_combine(value_hash, val.word(i));
                }
            }
        }

        return coconext::types::detail::hash_combine(ufixed_seed, R, value_hash);
    }
};

#endif  // COCONEXT_UFIXED_HPP

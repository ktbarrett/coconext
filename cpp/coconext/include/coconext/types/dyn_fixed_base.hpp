#ifndef COCONEXT_DYN_FIXED_BASE_HPP
#define COCONEXT_DYN_FIXED_BASE_HPP

#include <algorithm>
#include <cassert>
#include <cmath>
#include <coconext/types/common_math.hpp>
#include <coconext/types/dyn_int_base.hpp>
#include <coconext/types/dyn_signed.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/resize_mode.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace coconext::types::detail {

class DynUfixed;
class DynSfixed;

namespace dyn_fixed_detail {

inline void require_downto(Range range) {
    if (range.direction != Direction::DOWNTO) {
        throw std::invalid_argument(
            "Fixed-point numeric operation requires DOWNTO direction"
        );
    }
}

inline void require_numeric_range(Range range) {
    require_downto(range);
    if (range.length() == 0) {
        throw std::domain_error("Null-range fixed point has no numeric value");
    }
}

inline void validate_storage(Range range, size_t width) {
    if (range.length() != width) {
        throw std::invalid_argument("Fixed-point storage width does not match its range");
    }
}

inline Range::value_type checked_add(Range::value_type lhs, Range::value_type rhs) {
    assert(
        !(rhs > 0 && lhs > std::numeric_limits<Range::value_type>::max() - rhs)
        && !(rhs < 0 && lhs < std::numeric_limits<Range::value_type>::min() - rhs)
    );
    return lhs + rhs;
}

inline Range::value_type checked_sub(Range::value_type lhs, Range::value_type rhs) {
    assert(
        !(rhs < 0 && lhs > std::numeric_limits<Range::value_type>::max() + rhs)
        && !(rhs > 0 && lhs < std::numeric_limits<Range::value_type>::min() + rhs)
    );
    return lhs - rhs;
}

inline size_t checked_size_add(size_t lhs, size_t rhs) {
    assert(lhs <= std::numeric_limits<size_t>::max() - rhs);
    return lhs + rhs;
}

inline Range add_range(Range lhs, Range rhs) {
    require_downto(lhs);
    require_downto(rhs);
    return {
        checked_add(std::max(lhs.left, rhs.left), 1),
        Direction::DOWNTO,
        std::min(lhs.right, rhs.right)
    };
}

inline Range multiply_range(Range lhs, Range rhs) {
    require_downto(lhs);
    require_downto(rhs);
    return {
        checked_add(checked_add(lhs.left, rhs.left), 1),
        Direction::DOWNTO,
        checked_add(lhs.right, rhs.right)
    };
}

inline Range unsigned_quotient_range(Range lhs, Range rhs) {
    require_downto(lhs);
    require_downto(rhs);
    return {
        checked_sub(lhs.left, rhs.right),
        Direction::DOWNTO,
        checked_sub(checked_sub(lhs.right, rhs.left), 1)
    };
}

inline Range signed_quotient_range(Range lhs, Range rhs) {
    require_downto(lhs);
    require_downto(rhs);
    return {
        checked_add(checked_sub(lhs.left, rhs.right), 1),
        Direction::DOWNTO,
        checked_sub(lhs.right, rhs.left)
    };
}

// The ordering is checked before this helper is called. Unsigned subtraction
// then represents the full mathematical distance without signed overflow.
inline size_t index_distance(Range::value_type high, Range::value_type low) {
    if (high < low) {
        throw std::invalid_argument("Invalid fixed-point alignment");
    }
    return static_cast<size_t>(high) - static_cast<size_t>(low);
}

inline DynUInt wrapped_negate(DynUInt value) {
    return DynUInt::wrapping_negate(std::move(value));
}

inline DynUInt unsigned_magnitude(DynSInt const& value) {
    if (value.width() == 0) {
        return DynUInt(0);
    }
    auto raw = value.logical_bits();
    return value.is_negative() ? wrapped_negate(std::move(raw)) : std::move(raw);
}

inline DynUInt shift_left_widened(DynUInt const& value, size_t shift) {
    size_t const width = checked_size_add(value.width(), shift);
    return DynUInt(width, value) << shift;
}

inline DynSInt shift_left_widened(DynSInt const& value, size_t shift) {
    size_t const width = checked_size_add(value.width(), shift);
    return DynSInt(width, value) << shift;
}

struct aligned_magnitude {
    explicit aligned_magnitude(size_t width) : bits(width) {}

    DynUInt bits;
    bool overflow = false;
    bool discarded = false;
    bool half_bit = false;
    bool lower_bits = false;
};

inline aligned_magnitude align_magnitude(
    DynUInt const& source,
    Range::value_type source_right,
    Range::value_type target_right,
    size_t result_width
) {
    if (result_width == 0) {
        throw std::invalid_argument(
            "Fixed-point alignment requires a nonzero result width"
        );
    }

    aligned_magnitude result(result_width);
    size_t const source_width = source.width();
    if (source_width == 0) {
        return result;
    }

    if (source_right >= target_right) {
        size_t const shift = index_distance(source_right, target_right);
        size_t const copied =
            shift < result_width ? std::min(source_width, result_width - shift) : 0;
        for (size_t i = 0; i < copied; ++i) {
            result.bits.set_bit(i + shift, source.get_bit(i));
        }
        for (size_t i = copied; i < source_width; ++i) {
            if (source.get_bit(i)) {
                result.overflow = true;
                break;
            }
        }
        return result;
    }

    size_t const drop = index_distance(target_right, source_right);
    size_t const remaining = drop < source_width ? source_width - drop : 0;
    size_t const copied = std::min(remaining, result_width);
    for (size_t i = 0; i < copied; ++i) {
        result.bits.set_bit(i, source.get_bit(i + drop));
    }
    if (remaining > result_width) {
        for (size_t i = drop + result_width; i < source_width; ++i) {
            if (source.get_bit(i)) {
                result.overflow = true;
                break;
            }
        }
    }

    result.half_bit = drop <= source_width && source.get_bit(drop - 1);
    size_t const lower_count = std::min(source_width, drop - 1);
    for (size_t i = 0; i < lower_count; ++i) {
        if (source.get_bit(i)) {
            result.lower_bits = true;
            break;
        }
    }
    result.discarded = result.half_bit || result.lower_bits;
    return result;
}

inline void round_magnitude(aligned_magnitude& value, round_mode mode, bool negative) {
    bool round_up = false;
    switch (mode) {
    case round_mode::truncate:
        round_up = negative && value.discarded;
        break;
    case round_mode::round_to_zero:
        break;
    case round_mode::round_to_pos:
        round_up = !negative && value.discarded;
        break;
    case round_mode::round:
        round_up = value.half_bit;
        break;
    case round_mode::round_to_even:
        round_up = value.half_bit
                && (value.lower_bits || (value.bits.width() != 0 && value.bits.get_bit(0)));
        break;
    }

    if (!round_up) {
        return;
    }
    if (value.bits.popcount() == value.bits.width()) {
        value.overflow = true;
    }
    value.bits = DynUInt(
        value.bits.width(), value.bits + DynUInt(value.bits.width(), std::uint64_t{1})
    );
}

inline DynUInt convert_unsigned_magnitude(
    DynUInt const& source, Range::value_type source_right, Range target
) {
    require_downto(target);
    if (target.length() == 0) {
        auto aligned = align_magnitude(source, source_right, target.right, 1);
        if (aligned.discarded || aligned.overflow || aligned.bits.popcount() != 0) {
            throw std::out_of_range(
                "value cannot be represented exactly in destination DynUfixed"
            );
        }
        return DynUInt(0);
    }
    size_t const extended_width = checked_size_add(target.length(), 1);
    auto aligned = align_magnitude(source, source_right, target.right, extended_width);
    bool const out_of_range = aligned.overflow || aligned.bits.get_bit(target.length());
    if (aligned.discarded || out_of_range) {
        throw std::out_of_range(
            "value cannot be represented exactly in destination DynUfixed"
        );
    }
    return DynUInt(target.length(), aligned.bits);
}

inline DynSInt convert_signed_magnitude(
    DynUInt const& source, bool negative, Range::value_type source_right, Range target
) {
    require_downto(target);
    size_t const target_width = target.length();
    auto aligned = align_magnitude(
        source, source_right, target.right, checked_size_add(target_width, 1)
    );
    if (target_width == 0) {
        if (aligned.discarded || aligned.overflow || aligned.bits.popcount() != 0) {
            throw std::out_of_range(
                "value cannot be represented exactly in destination DynSfixed"
            );
        }
        return DynSInt(0);
    }
    DynUInt negative_limit(target_width + 1);
    negative_limit.set_bit(target_width - 1, true);
    bool const out_of_range =
        aligned.overflow
        || (negative ? negative_limit < aligned.bits : !(aligned.bits < negative_limit));
    if (aligned.discarded || out_of_range) {
        throw std::out_of_range(
            "value cannot be represented exactly in destination DynSfixed"
        );
    }
    auto magnitude = DynUInt(target_width, aligned.bits);
    return DynSInt(target_width, negative ? wrapped_negate(magnitude) : magnitude);
}

template <size_t Width, bool SignedRepresentation>
inline Int<Width, SignedRepresentation> copy_to_static_int(DynUInt const& source) {
    if (source.width() != Width) {
        throw std::invalid_argument(
            "Dynamic integer width does not match static destination width"
        );
    }

    Int<Width, SignedRepresentation> result;
    for (size_t i = 0; i < Width; ++i) {
        result.set_bit(i, source.get_bit(i));
    }
    return result;
}

inline DynUInt resize_unsigned_magnitude(
    DynUInt const& source,
    Range::value_type source_right,
    Range target,
    overflow_mode overflow,
    round_mode rounding
) {
    require_downto(target);
    size_t const target_width = target.length();
    if (target_width == 0) {
        return DynUInt(0);
    }
    auto aligned = align_magnitude(
        source, source_right, target.right, checked_size_add(target_width, 1)
    );
    round_magnitude(aligned, rounding, false);
    bool const out_of_range = aligned.overflow || aligned.bits.get_bit(target_width);
    if (out_of_range && overflow == overflow_mode::saturate) {
        return ~DynUInt(target_width);
    }
    return DynUInt(target_width, aligned.bits);
}

inline DynSInt resize_signed_magnitude(
    DynUInt const& source,
    bool negative,
    Range::value_type source_right,
    Range target,
    overflow_mode overflow,
    round_mode rounding
) {
    require_downto(target);
    size_t const target_width = target.length();
    if (target_width == 0) {
        return DynSInt(0);
    }
    auto aligned = align_magnitude(
        source, source_right, target.right, checked_size_add(target_width, 1)
    );
    round_magnitude(aligned, rounding, negative);

    DynUInt negative_limit(target_width + 1);
    negative_limit.set_bit(target_width - 1, true);
    bool const out_of_range =
        aligned.overflow
        || (negative ? negative_limit < aligned.bits : !(aligned.bits < negative_limit));
    if (out_of_range && overflow == overflow_mode::saturate) {
        DynUInt sign_bit(target_width);
        sign_bit.set_bit(target_width - 1, true);
        return DynSInt(target_width, negative ? sign_bit : ~sign_bit);
    }

    auto magnitude = DynUInt(target_width, aligned.bits);
    return DynSInt(target_width, negative ? wrapped_negate(magnitude) : magnitude);
}

template <std::floating_point FloatType>
inline aligned_magnitude align_floating_magnitude(
    FloatType magnitude, Range::value_type target_right, size_t result_width
) {
    constexpr size_t significand_width = std::numeric_limits<FloatType>::digits;
    int exponent = 0;
    FloatType const fraction = std::frexp(magnitude, &exponent);
    FloatType significand = std::ldexp(fraction, static_cast<int>(significand_width));
    DynUInt significand_bits(significand_width);
    for (size_t bit = 0; bit < significand_width && significand >= FloatType{1}; ++bit) {
        FloatType const half = std::floor(significand / FloatType{2});
        if (significand - half * FloatType{2} >= FloatType{1}) {
            significand_bits.set_bit(bit, true);
        }
        significand = half;
    }
    Range::value_type const source_right =
        static_cast<Range::value_type>(exponent)
        - static_cast<Range::value_type>(significand_width);
    return align_magnitude(significand_bits, source_right, target_right, result_width);
}

inline std::pair<DynUInt, DynUInt> divide_fixed_magnitudes(
    DynUInt const& dividend,
    DynUInt const& divisor,
    size_t result_width,
    Range::value_type quotient_right,
    bool negative,
    round_mode rounding,
    size_t guard_bits
) {
    if (divisor.width() == 0 || divisor.popcount() == 0) {
        throw std::domain_error("Division by zero");
    }

    auto [integer_quotient, remainder] = divrem(dividend, divisor);
    DynUInt const exact_remainder(remainder);
    aligned_magnitude rounded(checked_size_add(result_width, 1));

    auto integer_bit = [&](size_t index) {
        return index < integer_quotient.width() && integer_quotient.get_bit(index);
    };

    if (quotient_right >= 0) {
        size_t const first = static_cast<size_t>(quotient_right);
        for (size_t i = 0; i < rounded.bits.width(); ++i) {
            rounded.bits.set_bit(i, integer_bit(checked_size_add(first, i)));
        }
    } else {
        size_t const fraction_bits = index_distance(0, quotient_right);
        for (size_t i = fraction_bits; i < rounded.bits.width(); ++i) {
            rounded.bits.set_bit(i, integer_bit(i - fraction_bits));
        }
    }

    DynUInt const extended_divisor(divisor.width() + 1, divisor);
    auto next_quotient_bit = [&] {
        auto doubled = DynUInt(divisor.width() + 1, remainder) << 1;
        bool const bit = !(doubled < extended_divisor);
        if (bit) {
            doubled = DynUInt(doubled.width(), doubled - extended_divisor);
        }
        remainder = DynUInt(divisor.width(), doubled);
        return bit;
    };

    if (quotient_right < 0) {
        size_t const fraction_bits = index_distance(0, quotient_right);
        for (size_t i = fraction_bits; i > 0; --i) {
            bool const bit = next_quotient_bit();
            if (i <= rounded.bits.width()) {
                rounded.bits.set_bit(i - 1, bit);
            }
        }
    }

    for (size_t i = 0; i < guard_bits; ++i) {
        bool bit;
        if (quotient_right > 0) {
            size_t const integer_guards = static_cast<size_t>(quotient_right);
            bit = i < integer_guards ? integer_bit(integer_guards - i - 1)
                                     : next_quotient_bit();
        } else {
            bit = next_quotient_bit();
        }
        if (i == 0) {
            rounded.half_bit = bit;
        } else {
            rounded.lower_bits = rounded.lower_bits || bit;
        }
    }
    rounded.discarded = rounded.half_bit || rounded.lower_bits;
    round_magnitude(rounded, rounding, negative);
    return {DynUInt(result_width, rounded.bits), exact_remainder};
}

inline std::pair<DynSInt, DynSInt> divrem_signed_fixed(
    DynSInt const& dividend,
    DynSInt const& divisor,
    size_t result_width,
    Range::value_type quotient_right,
    bool modulo,
    round_mode rounding,
    size_t guard_bits
) {
    if (divisor.width() == 0 || divisor.popcount() == 0) {
        throw std::domain_error("Division by zero");
    }

    bool const lhs_negative = dividend.width() != 0 && dividend.is_negative();
    bool const rhs_negative = divisor.is_negative();
    auto lhs_magnitude = unsigned_magnitude(dividend);
    auto rhs_magnitude = unsigned_magnitude(divisor);
    bool const quotient_negative = lhs_negative != rhs_negative;

    auto [quotient_magnitude, remainder_magnitude] = divide_fixed_magnitudes(
        lhs_magnitude,
        rhs_magnitude,
        result_width,
        quotient_right,
        quotient_negative,
        rounding,
        guard_bits
    );
    DynSInt quotient(
        result_width,
        quotient_negative ? wrapped_negate(quotient_magnitude) : quotient_magnitude
    );

    bool remainder_negative = lhs_negative;
    if (modulo && remainder_magnitude.popcount() != 0 && lhs_negative != rhs_negative) {
        remainder_magnitude =
            DynUInt(rhs_magnitude.width(), rhs_magnitude - remainder_magnitude);
        remainder_negative = rhs_negative;
    }
    DynSInt remainder(
        remainder_magnitude.width(),
        remainder_negative ? wrapped_negate(remainder_magnitude) : remainder_magnitude
    );
    return {std::move(quotient), std::move(remainder)};
}

template <NativeInteger T>
inline DynUInt native_magnitude(T value, bool& negative) {
    constexpr size_t source_width =
        std::numeric_limits<T>::digits + (std::numeric_limits<T>::is_signed ? 1 : 0);
    if constexpr (std::numeric_limits<T>::is_signed) {
        negative = value < 0;
        DynSInt source(source_width, value);
        return unsigned_magnitude(source);
    } else {
        negative = false;
        return DynUInt(source_width, value);
    }
}

inline long double scaled_long_double(
    std::string const& raw, Range::value_type right
) noexcept {
    long double value = std::strtold(raw.c_str(), nullptr);
    if (right > std::numeric_limits<int>::max()) {
        return value == 0.0L
                 ? value
                 : std::copysign(std::numeric_limits<long double>::infinity(), value);
    }
    if (right < std::numeric_limits<int>::min()) {
        return std::copysign(0.0L, value);
    }
    return std::ldexp(value, static_cast<int>(right));
}

inline std::string fixed_decimal_string(
    DynUInt magnitude, bool negative, Range::value_type right
) {
    if (magnitude.width() == 0) {
        return "";
    }
    std::string result;
    if (right >= 0) {
        size_t const shift = static_cast<size_t>(right);
        magnitude = shift_left_widened(magnitude, shift);
        result = magnitude.to_decimal_string();
    } else {
        size_t const fractional_digits = index_distance(0, right);
        for (size_t i = 0; i < fractional_digits; ++i) {
            magnitude = magnitude * DynUInt(3, std::uint64_t{5});
        }
        result = magnitude.to_decimal_string();
        if (result.size() <= fractional_digits) {
            result.insert(0, fractional_digits + 1 - result.size(), '0');
        }
        result.insert(result.size() - fractional_digits, 1, '.');
    }
    if (negative && magnitude.popcount() != 0) {
        result.insert(result.begin(), '-');
    }
    return result;
}

inline std::string fixed_binary_string(DynUInt const& raw, Range range) {
    std::string result = raw.to_binary_string();
    Range::value_type const fraction =
        -(range.direction == Direction::DOWNTO ? range.right : range.left);
    Range::value_type const integer =
        (range.direction == Direction::DOWNTO ? range.left : range.right) + 1;
    if (fraction > 0 && integer > 0) {
        size_t const fractional_bits = static_cast<size_t>(fraction);
        if (fractional_bits < result.size()) {
            result.insert(result.size() - fractional_bits, 1, '.');
        }
    }
    return result;
}

inline DynUInt reverse_bits(DynUInt const& value) {
    DynUInt result(value.width());
    for (size_t i = 0; i < value.width(); ++i) {
        result.set_bit(value.width() - 1 - i, value.get_bit(i));
    }
    return result;
}

template <typename ShiftType>
inline size_t normalize_dynamic_shift(ShiftType const& amount, size_t limit) {
    using Clean = std::remove_cvref_t<ShiftType>;
    if constexpr (std::integral<Clean>) {
        if constexpr (std::signed_integral<Clean>) {
            if (amount < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }
        if constexpr (
            std::numeric_limits<Clean>::digits > std::numeric_limits<size_t>::digits
        )
        {
            if (amount > static_cast<Clean>(std::numeric_limits<size_t>::max())) {
                return limit;
            }
        }
        size_t const value = static_cast<size_t>(amount);
        return std::min(value, limit);
    } else if constexpr (std::same_as<Clean, DynUnsigned> || std::same_as<Clean, DynSigned>)
    {
        if constexpr (std::same_as<Clean, DynSigned>) {
            if (storage(amount).width() != 0 && storage(amount).is_negative()) {
                throw std::invalid_argument("Negative shift amount");
            }
        }
        auto const& raw = storage(amount);
        size_t value = 0;
        for (size_t bit = raw.width(); bit > 0; --bit) {
            if (value > limit / 2) {
                return limit;
            }
            value *= 2;
            if (raw.get_bit(bit - 1)) {
                if (value >= limit) {
                    return limit;
                }
                ++value;
            }
        }
        return std::min(value, limit);
    } else {
        return normalize_shift_amount(amount, limit);
    }
}

}  // namespace dyn_fixed_detail
}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_FIXED_BASE_HPP

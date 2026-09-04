#ifndef COCONEXT_COMMON_MATH_HPP
#define COCONEXT_COMMON_MATH_HPP

#include <coconext/types/concepts.hpp>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace coconext::types::detail {

template <typename ShiftType>
constexpr size_t normalize_shift_amount(ShiftType const& shift_amount, size_t limit) {
    using CleanType = std::remove_cvref_t<ShiftType>;

    static_assert(
        std::is_integral_v<CleanType> || is_coconext_unsigned_v<CleanType>
            || is_coconext_signed_v<CleanType>,
        "Shift amount can only be a native integer, Signed, or Unsigned"
    );

    if constexpr (std::is_integral_v<CleanType>) {
        if constexpr (std::is_signed_v<CleanType>) {
            if (shift_amount < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        if constexpr (
            std::numeric_limits<CleanType>::digits > std::numeric_limits<size_t>::digits
        )
        {
            if (shift_amount > static_cast<CleanType>(std::numeric_limits<size_t>::max())) {
                return limit;
            }
        }

        size_t const value = static_cast<size_t>(shift_amount);
        return value < limit ? value : limit;
    } else if constexpr (
        is_coconext_unsigned_v<CleanType> || is_coconext_signed_v<CleanType>
    )
    {
        constexpr size_t width = CleanType::size();
        if constexpr (is_coconext_signed_v<CleanType> && width > 0) {
            if (bits(shift_amount).get_bit(width - 1)) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        if constexpr (width == 0) {
            return 0;
        } else {
            // Accumulate only up to the operand width. Every larger value has
            // the same shift result, so the shift count never needs narrowing.
            size_t value = 0;
            for (size_t bit_pos = width; bit_pos > 0; --bit_pos) {
                if (value > limit / 2) {
                    return limit;
                }
                value *= 2;
                if (value >= limit) {
                    return limit;
                }
                if (bits(shift_amount).get_bit(bit_pos - 1)) {
                    ++value;
                    if (value >= limit) {
                        return limit;
                    }
                }
            }
            return value;
        }
    } else {
        return 0;  // The static assertion above reports the unsupported type.
    }
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_COMMON_MATH_HPP

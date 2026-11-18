#ifndef COCONEXT_DETAIL_COMMON_MATH_HPP
#define COCONEXT_DETAIL_COMMON_MATH_HPP

#include <cstddef>
#include <limits>
#include <stdexcept>

#include <coconext/types/detail/concepts.hpp>

namespace coconext::types::detail {

template <NativeInteger ShiftType>
constexpr size_t normalize_shift_amount(ShiftType const& shift_amount, size_t limit) {
    if constexpr (std::numeric_limits<ShiftType>::is_signed) {
        if (shift_amount < 0) {
            throw std::invalid_argument("Negative shift amount");
        }
    }

    if constexpr (
        std::numeric_limits<ShiftType>::digits > std::numeric_limits<size_t>::digits
    )
    {
        if (shift_amount > static_cast<ShiftType>(std::numeric_limits<size_t>::max())) {
            return limit;
        }
    }

    size_t const value = static_cast<size_t>(shift_amount);
    return value < limit ? value : limit;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DETAIL_COMMON_MATH_HPP

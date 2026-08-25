#ifndef COCONEXT_RESIZE_MODE_HPP
#define COCONEXT_RESIZE_MODE_HPP

#include <cstddef>

namespace coconext::types {

// Match the IEEE fixed_pkg default. Division computes this many additional
// quotient bits before applying its rounding mode.
inline constexpr std::size_t fixed_guard_bits = 3;

enum class overflow_mode {
    wrap,     // drop MSBs
    saturate  // cap to max
};

enum class round_mode {
    truncate,  // drop LSBs
    round,     // away from zero
    round_to_even,
    round_to_zero,
    round_to_pos
};

}  // namespace coconext::types

#endif  // COCONEXT_RESIZE_MODE_HPP

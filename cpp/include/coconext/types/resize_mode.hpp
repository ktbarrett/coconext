#ifndef COCONEXT_RESIZE_MODE_HPP
#define COCONEXT_RESIZE_MODE_HPP

namespace coconext::types {

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

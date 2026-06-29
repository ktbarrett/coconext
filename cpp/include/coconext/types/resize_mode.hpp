#ifndef COCONEXT_RESIZE_MODE_HPP
#define COCONEXT_RESIZE_MODE_HPP

namespace coconext::types {

enum class overflow_mode {
    wrap,
    saturate
};
enum class round_mode {
    truncate,
    round
};

}  // namespace coconext::types

#endif  // COCONEXT_RESIZE_MODE_HPP

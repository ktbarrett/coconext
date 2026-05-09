#ifndef COCONEXT_DIRECTION_HPP
#define COCONEXT_DIRECTION_HPP

#include <stdexcept>
#include <string_view>

namespace coconext::types {

enum class Direction : bool {
    TO = 0,
    DOWNTO = 1,
};

constexpr std::string_view to_string(Direction direction) noexcept {
    if (direction == Direction::TO) {
        return "to";
    } else {
        return "downto";
    }
}

constexpr Direction to_direction(std::string_view value) {
    auto to_lower = [](char c) { return c | 0x20; };

    auto equal = [&](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < b.size(); ++i) {
            // we deliberately only lower a since we know b to be the literal
            if (to_lower(a[i]) != b[i]) {
                return false;
            }
        }
        return true;
    };

    if (equal(value, "to")) {
        return Direction::TO;
    } else if (equal(value, "downto")) {
        return Direction::DOWNTO;
    } else {
        throw std::invalid_argument("Invalid direction string");
    }
}

}  // namespace coconext::types

#endif  // COCONEXT_DIRECTION_HPP

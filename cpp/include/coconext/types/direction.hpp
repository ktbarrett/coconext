#ifndef COCONEXT_DIRECTION_HPP
#define COCONEXT_DIRECTION_HPP

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace coconext::types {

class Direction {
public:
    enum class value_type : int8_t {
        TO = 1,
        DOWNTO = -1,
    };
    using enum value_type;

public:
    constexpr Direction() noexcept = default;
    constexpr Direction(value_type value) noexcept : value_(value) {}
    constexpr value_type value() const noexcept { return value_; }

private:
    value_type value_{TO};
};

constexpr bool operator==(const Direction& lhs, const Direction& rhs) noexcept {
    return lhs.value() == rhs.value();
}

constexpr std::string_view to_string(const Direction& direction) noexcept {
    if (direction.value() == Direction::TO) {
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

namespace std {

template <>
struct hash<coconext::types::Direction> {
    size_t operator()(
        const coconext::types::Direction& direction) const noexcept {
        return std::hash<coconext::types::Direction::value_type>()(
            direction.value());
    }
};

}  // namespace std

#endif  // COCONEXT_DIRECTION_HPP

#ifndef COCONEXT_DIRECTION_HPP
#define COCONEXT_DIRECTION_HPP

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace coconext::types {

class Direction {
public:
    enum class value_type : uint8_t {
        TO,
        DOWNTO,
    };
    using enum value_type;

public:
    // ensures that these are constexpr since enum classes are not literal types
    constexpr Direction() noexcept = default;
    constexpr Direction(const Direction&) noexcept = default;
    constexpr Direction& operator=(const Direction&) noexcept = default;
    constexpr Direction(Direction&&) noexcept = default;
    constexpr Direction& operator=(Direction&&) noexcept = default;

    constexpr Direction(value_type value) noexcept : value_(value) {}
    template <typename T>
        requires requires { to_direction(std::declval<T>()); }
    explicit constexpr Direction(T&& value)
        : value_(to_direction(std::forward<T>(value)).value()) {}

public:
    constexpr value_type value() const noexcept { return value_; }

private:
    value_type value_ = TO;
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
    std::string str(value);
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    if (str == "to") {
        return Direction::TO;
    } else if (str == "downto") {
        return Direction::DOWNTO;
    } else {
        throw std::invalid_argument("Invalid direction string");
    }
}

}  // namespace coconext::types

namespace std {

template <>
class hash<coconext::types::Direction> {
public:
    size_t operator()(
        const coconext::types::Direction& direction) const noexcept {
        return std::hash<coconext::types::Direction::value_type>()(
            direction.value());
    }
};

}  // namespace std

#endif  // COCONEXT_DIRECTION_HPP

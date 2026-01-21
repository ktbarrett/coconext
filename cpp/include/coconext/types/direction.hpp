#ifndef COCONEXT_DIRECTION_HPP
#define COCONEXT_DIRECTION_HPP

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace coconext::types {

class Direction {
public:
    enum value_type : uint8_t {
        TO,
        DOWNTO,
    };

public:
    constexpr Direction() noexcept : value_(TO) {}
    constexpr Direction(value_type value) : value_(value) {
        if (value != TO && value != DOWNTO) {
            throw std::invalid_argument("Invalid direction value");
        }
    }
    template <typename T>
        requires requires { to_direction(std::declval<T>()); }
    explicit constexpr Direction(T&& value)
        : value_(to_direction(std::forward<T>(value)).value()) {}
    constexpr value_type value() const noexcept { return value_; }

private:
    static struct no_check_tag_t {
    } no_check_tag;
    constexpr Direction(value_type value, no_check_tag_t) : value_(value) {}
    friend constexpr Direction to_direction(std::string_view value);
    friend constexpr Direction operator""_dir(const char* str, size_t len);

private:
    value_type value_;
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
        return Direction(Direction::TO, Direction::no_check_tag);
    } else if (str == "downto") {
        return Direction(Direction::DOWNTO, Direction::no_check_tag);
    } else {
        throw std::invalid_argument("Invalid direction string");
    }
}

constexpr Direction operator""_dir(const char* str, size_t len) {
    return to_direction(std::string_view(str, len));
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

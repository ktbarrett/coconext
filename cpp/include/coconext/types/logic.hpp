#ifndef COCONEXT_LOGIC_HPP
#define COCONEXT_LOGIC_HPP

#include <coconext/types/concepts.hpp>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace coconext::types {

class Logic {
public:
    enum class value_type : uint8_t {
        _0,
        _1,
        X,
        Z,
        U,
        W,
        L,
        H,
        DC,
    };
    using enum value_type;

public:
    // ensures that these are constexpr since enum classes are not literal types
    constexpr Logic() noexcept = default;
    constexpr Logic(const Logic&) noexcept = default;
    constexpr Logic& operator=(const Logic&) noexcept = default;
    constexpr Logic(Logic&&) noexcept = default;
    constexpr Logic& operator=(Logic&&) noexcept = default;

    constexpr Logic(value_type value) noexcept : value_(value) {}
    template <typename T>
        requires requires { to_logic(std::declval<T>()); }
    explicit constexpr Logic(T&& value)
        : value_(to_logic(std::forward<T>(value)).value()) {}

public:
    constexpr value_type value() const noexcept { return value_; }

private:
    value_type value_ = _0;
};

class Bit : public Logic {
public:
    // ensures that these are constexpr since enum classes are not literal types
    constexpr Bit(const Bit&) noexcept = default;
    constexpr Bit& operator=(const Bit&) noexcept = default;
    constexpr Bit(Bit&&) noexcept = default;
    constexpr Bit& operator=(Bit&&) noexcept = default;

    constexpr Bit() noexcept : Logic(_0) {}
    constexpr Bit(value_type value) noexcept : Logic(value) {}
    template <typename T>
        requires requires { to_bit(std::declval<T>()); }
    explicit constexpr Bit(T&& value) : Logic(to_bit(std::forward<T>(value))) {}
};

constexpr bool operator==(const Logic& lhs, const Logic& rhs) noexcept {
    return lhs.value() == rhs.value();
}

template <Character CharType>
constexpr Logic to_logic(CharType value) {
    using enum Logic::value_type;
    constexpr struct {
        CharType chr;
        Logic val;
    } chr_map[] = {
        {'U', U},  {'u', U}, {'X', X}, {'x', X}, {'0', _0},
        {'1', _1}, {'Z', Z}, {'z', Z}, {'W', W}, {'w', W},
        {'L', L},  {'l', L}, {'H', H}, {'h', H}, {'-', DC},
    };
    for (const auto& [chr, val] : chr_map) {
        if (value == chr) {
            return val;
        }
    }
    throw std::invalid_argument("Invalid logic value");
}

constexpr Logic to_logic(std::string_view value) {
    if (value.size() != 1) {
        throw std::invalid_argument("Invalid logic value");
    }
    return to_logic(value[0]);
}

template <Integer IntType>
constexpr Logic to_logic(IntType value) {
    if (value == 0) {
        return Logic::_0;
    } else if (value == 1) {
        return Logic::_1;
    } else {
        throw std::invalid_argument("Invalid logic value");
    }
}

constexpr Logic to_logic(const Bit& value) { return value; }

template <Character CharType>
constexpr Bit to_bit(CharType value) {
    if (value == '0') {
        return Logic::_0;
    } else if (value == '1') {
        return Logic::_1;
    } else {
        throw std::invalid_argument("Invalid bit value");
    }
}

constexpr Bit to_bit(std::string_view value) {
    if (value.size() != 1) {
        throw std::invalid_argument("Invalid bit value");
    }
    return to_bit(value[0]);
}

template <Integer IntType>
constexpr Bit to_bit(IntType value) {
    if (value == 0) {
        return Logic::_0;
    } else if (value == 1) {
        return Logic::_1;
    } else {
        throw std::invalid_argument("Invalid bit value");
    }
}

constexpr Bit to_bit(const Logic& value) {
    if (value == Logic::_0 || value == Logic::_1) {
        return Bit(value.value());
    } else {
        throw std::invalid_argument("Invalid bit value");
    }
}

constexpr std::string_view to_string(const Logic& value) noexcept {
    constexpr char const* const str_map[] = {
        "0", "1", "X", "Z", "U", "W", "L", "H", "-",
    };
    return str_map[static_cast<size_t>(value.value())];
}

constexpr long long to_int(const Logic& value) {
    switch (value.value()) {
    case Logic::_0:
    case Logic::L:
        return 0;
    case Logic::_1:
    case Logic::H:
        return 1;
    default:
        throw std::invalid_argument(
            "Cannot convert Logic with non-binary value to integer");
    }
}

constexpr Logic operator|(const Logic& lhs, const Logic& rhs) noexcept {
    using enum Logic::value_type;
    constexpr Logic const table[9][9] = {
        // clang-format off
        // ------------------------------------------
        // ---|   0   1   X   Z   U   W   L   H   -
        // ------------------------------------------
        /* 0 */ {_0, _1,  X,  X,  U,  X, _0, _1,  X},
        /* 1 */ {_1, _1, _1, _1, _1, _1, _1, _1, _1},
        /* X */ { X, _1,  X,  X,  U,  X,  X, _1,  X},
        /* Z */ { X, _1,  X,  X,  U,  X,  X, _1,  X},
        /* U */ { U, _1,  U,  U,  U,  U,  U, _1,  U},
        /* W */ { X, _1,  X,  X,  U,  X,  X, _1,  X},
        /* L */ {_0, _1,  X,  X,  U,  X, _0, _1,  X},
        /* H */ {_1, _1, _1, _1, _1, _1, _1, _1, _1},
        /* - */ { X, _1,  X,  X,  U,  X,  X, _1,  X},
        // clang-format on
    };
    return table[static_cast<size_t>(lhs.value())]
                [static_cast<size_t>(rhs.value())];
}

constexpr Bit operator|(const Bit& lhs, const Bit& rhs) noexcept {
    return Bit::value_type(int(lhs.value()) | int(rhs.value()));
}

constexpr Logic operator&(const Logic& lhs, const Logic& rhs) noexcept {
    using enum Logic::value_type;
    constexpr Logic const table[9][9] = {
        // clang-format off
        // ------------------------------------------
        // ---|   0   1   X   Z   U   W   L   H   -
        // ------------------------------------------
        /* 0 */ {_0, _0, _0, _0, _0, _0, _0, _0, _0},
        /* 1 */ {_0, _1,  X,  X,  U,  X, _0, _1,  X},
        /* X */ {_0,  X,  X,  X,  U,  X, _0,  X,  X},
        /* Z */ {_0,  X,  X,  X,  U,  X, _0,  X,  X},
        /* U */ {_0,  U,  U,  U,  U,  U, _0,  U,  U},
        /* W */ {_0,  X,  X,  X,  U,  X, _0,  X,  X},
        /* L */ {_0, _0, _0, _0, _0, _0, _0, _0, _0},
        /* H */ {_0, _1,  X,  X,  U,  X, _0, _1,  X},
        /* - */ {_0,  X,  X,  X,  U,  X, _0,  X,  X},
        // clang-format on
    };
    return table[static_cast<size_t>(lhs.value())]
                [static_cast<size_t>(rhs.value())];
}

constexpr Bit operator&(const Bit& lhs, const Bit& rhs) noexcept {
    return Bit::value_type(int(lhs.value()) & int(rhs.value()));
}

constexpr Logic operator^(const Logic& lhs, const Logic& rhs) noexcept {
    using enum Logic::value_type;
    constexpr Logic const table[9][9] = {
        // clang-format off
        // ------------------------------------------
        // ---|   0   1   X   Z   U   W   L   H   -
        // ------------------------------------------
        /* 0 */ {_0, _1,  X,  X,  U,  X, _0, _1,  X},
        /* 1 */ {_1, _0,  X,  X,  U,  X, _1, _0,  X},
        /* X */ { X,  X,  X,  X,  U,  X,  X,  X,  X},
        /* Z */ { X,  X,  X,  X,  U,  X,  X,  X,  X},
        /* U */ { U,  U,  U,  U,  U,  U,  U,  U,  U},
        /* W */ { X,  X,  X,  X,  U,  X,  X,  X,  X},
        /* L */ {_0, _1,  X,  X,  U,  X, _0, _1,  X},
        /* H */ {_1, _0,  X,  X,  U,  X, _1, _0,  X},
        /* - */ { X,  X,  X,  X,  U,  X,  X,  X,  X},
        // clang-format on
    };
    return table[static_cast<size_t>(lhs.value())]
                [static_cast<size_t>(rhs.value())];
}

constexpr Bit operator^(const Bit& lhs, const Bit& rhs) noexcept {
    return Bit::value_type(int(lhs.value()) ^ int(rhs.value()));
}

constexpr Logic operator~(const Logic& value) noexcept {
    using enum Logic::value_type;
    constexpr Logic const table[9] = {
        // clang-format off
        /*
         0   1   X   Z   U   W   L   H   - */
        _1, _0,  X,  X,  U,  X, _1, _0,  X,
        // clang-format on
    };
    return table[static_cast<size_t>(value.value())];
}

constexpr Bit operator~(const Bit& value) noexcept {
    return value == Bit::_0 ? Bit::_1 : Bit::_0;
}

constexpr bool is_01(const Logic& value) noexcept {
    return value == Logic::_0 || value == Logic::_1 || value == Logic::L ||
           value == Logic::H;
}

enum class ResolveMethod {
    ERROR,
    WEAK,
    ZEROS,
    ONES,
    RANDOM,
};

Logic resolve(const Logic& value, ResolveMethod method);

inline Bit resolve(const Bit& value, ResolveMethod method) { return value; }

}  // namespace coconext::types
namespace std {

template <>
class hash<coconext::types::Logic> {
public:
    size_t operator()(const coconext::types::Logic& logic) const noexcept {
        return std::hash<coconext::types::Logic::value_type>()(logic.value());
    }
};

template <>
class hash<coconext::types::Bit> {
public:
    size_t operator()(const coconext::types::Bit& bit) const noexcept {
        return std::hash<coconext::types::Bit::value_type>()(bit.value());
    }
};

}  // namespace std

#endif  // COCONEXT_LOGIC_HPP

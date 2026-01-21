#ifndef COCONEXT_LOGIC_HPP
#define COCONEXT_LOGIC_HPP

#include <coconext/types/concepts.hpp>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace coconext::types {

class Logic {
public:
    enum value_type : uint8_t {
        U,
        X,
        _0,
        _1,
        Z,
        W,
        L,
        H,
        DC,
    };

public:
    constexpr Logic() noexcept : value_(U) {}
    constexpr Logic(value_type value) noexcept : value_(value) {}
    template <typename T>
        requires requires { to_logic(std::declval<T>()); }
    explicit constexpr Logic(T&& value)
        : value_(to_logic(std::forward<T>(value)).value()) {}

public:
    constexpr value_type value() const noexcept { return value_; }

private:
    value_type value_;
};

class Bit : public Logic {
public:
    constexpr Bit() noexcept : Logic(_0) {}
    constexpr Bit(value_type value) noexcept : Logic(value) {}
    template <typename T>
        requires requires { to_bit(std::declval<T>()); }
    explicit constexpr Bit(T&& value) : Logic(to_bit(std::forward<T>(value))) {}
};

constexpr bool operator==(const Logic& lhs, const Logic& rhs) noexcept {
    return lhs.value() == rhs.value();
}

constexpr Logic operator""_l(char c) {
    switch (c) {
    case 'U':
        return Logic::U;
    case 'X':
        return Logic::X;
    case '0':
        return Logic::_0;
    case '1':
        return Logic::_1;
    case 'Z':
        return Logic::Z;
    case 'W':
        return Logic::W;
    case 'L':
        return Logic::L;
    case 'H':
        return Logic::H;
    case '-':
        return Logic::DC;
    default:
        throw std::invalid_argument("Invalid logic value");
    }
}

constexpr Bit operator""_b(char c) {
    switch (c) {
    case '0':
        return Logic::_0;
    case '1':
        return Logic::_1;
    default:
        throw std::invalid_argument("Invalid bit value");
    }
}

template <Character CharType>
constexpr Logic to_logic(CharType value) {
    constexpr struct {
        CharType chr;
        Logic val;
    } chr_map[] = {
        {'U', 'U'_l}, {'u', 'U'_l}, {'X', 'X'_l}, {'x', 'X'_l}, {'0', '0'_l},
        {'1', '1'_l}, {'Z', 'Z'_l}, {'z', 'Z'_l}, {'W', 'W'_l}, {'w', 'W'_l},
        {'L', 'L'_l}, {'l', 'L'_l}, {'H', 'H'_l}, {'h', 'H'_l}, {'-', '-'_l},
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
        return '0'_l;
    } else if (value == 1) {
        return '1'_l;
    } else {
        throw std::invalid_argument("Invalid logic value");
    }
}

constexpr Logic to_logic(const Bit& value) { return value; }

template <Character CharType>
constexpr Bit to_bit(CharType value) {
    if (value == '0') {
        return '0'_b;
    } else if (value == '1') {
        return '1'_b;
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
        return '0'_b;
    } else if (value == 1) {
        return '1'_b;
    } else {
        throw std::invalid_argument("Invalid bit value");
    }
}

constexpr Bit to_bit(const Logic& value) {
    if (value == '0'_l || value == '1'_l) {
        return Bit(value.value());
    } else {
        throw std::invalid_argument("Invalid bit value");
    }
}

constexpr std::string_view to_string(const Logic& value) noexcept {
    constexpr char const* const str_map[] = {
        "U", "X", "0", "1", "Z", "W", "L", "H", "-",
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
    constexpr Logic const table[9][9] = {
        // ---------------------
        //  U X 0 1 Z W L H -
        // ---------------------
        /* U */ {'U'_l, 'U'_l, 'U'_l, '1'_l, 'U'_l, 'U'_l, 'U'_l, '1'_l, 'U'_l},
        /* X */ {'U'_l, 'X'_l, 'X'_l, '1'_l, 'X'_l, 'X'_l, 'X'_l, '1'_l, 'X'_l},
        /* 0 */ {'U'_l, 'X'_l, '0'_l, '1'_l, 'X'_l, 'X'_l, '0'_l, '1'_l, 'X'_l},
        /* 1 */ {'1'_l, '1'_l, '1'_l, '1'_l, '1'_l, '1'_l, '1'_l, '1'_l, '1'_l},
        /* Z */ {'U'_l, 'X'_l, 'X'_l, '1'_l, 'X'_l, 'X'_l, 'X'_l, '1'_l, 'X'_l},
        /* W */ {'U'_l, 'X'_l, 'X'_l, '1'_l, 'X'_l, 'X'_l, 'X'_l, '1'_l, 'X'_l},
        /* L */ {'U'_l, 'X'_l, '0'_l, '1'_l, 'X'_l, 'X'_l, '0'_l, '1'_l, 'X'_l},
        /* H */ {'1'_l, '1'_l, '1'_l, '1'_l, '1'_l, '1'_l, '1'_l, '1'_l, '1'_l},
        /* - */ {'U'_l, 'X'_l, 'X'_l, '1'_l, 'X'_l, 'X'_l, 'X'_l, '1'_l, 'X'_l},
    };
    return table[static_cast<size_t>(lhs.value())]
                [static_cast<size_t>(rhs.value())];
}

constexpr Bit operator|(const Bit& lhs, const Bit& rhs) noexcept {
    return to_bit(Logic(lhs) | Logic(rhs));
}

constexpr Logic operator&(const Logic& lhs, const Logic& rhs) noexcept {
    constexpr Logic const table[9][9] = {
        // ---------------------
        //  U X 0 1 Z W L H -
        // ---------------------
        /* U */ {'U'_l, 'U'_l, '0'_l, 'U'_l, 'U'_l, 'U'_l, '0'_l, 'U'_l, 'U'_l},
        /* X */ {'U'_l, 'X'_l, '0'_l, 'X'_l, 'X'_l, 'X'_l, '0'_l, 'X'_l, 'X'_l},
        /* 0 */ {'0'_l, '0'_l, '0'_l, '0'_l, '0'_l, '0'_l, '0'_l, '0'_l, '0'_l},
        /* 1 */ {'U'_l, 'X'_l, '0'_l, '1'_l, 'X'_l, 'X'_l, '0'_l, '1'_l, 'X'_l},
        /* Z */ {'U'_l, 'X'_l, '0'_l, 'X'_l, 'X'_l, 'X'_l, '0'_l, 'X'_l, 'X'_l},
        /* W */ {'U'_l, 'X'_l, '0'_l, 'X'_l, 'X'_l, 'X'_l, '0'_l, 'X'_l, 'X'_l},
        /* L */ {'0'_l, '0'_l, '0'_l, '0'_l, '0'_l, '0'_l, '0'_l, '0'_l, '0'_l},
        /* H */ {'U'_l, 'X'_l, '0'_l, '1'_l, 'X'_l, 'X'_l, '0'_l, '1'_l, 'X'_l},
        /* - */ {'U'_l, 'X'_l, '0'_l, 'X'_l, 'X'_l, 'X'_l, '0'_l, 'X'_l, 'X'_l},
    };
    return table[static_cast<size_t>(lhs.value())]
                [static_cast<size_t>(rhs.value())];
}

constexpr Bit operator&(const Bit& lhs, const Bit& rhs) noexcept {
    return to_bit(Logic(lhs) & Logic(rhs));
}

constexpr Logic operator^(const Logic& lhs, const Logic& rhs) noexcept {
    constexpr Logic const table[9][9] = {
        // ---------------------
        //  U X 0 1 Z W L H -
        // ---------------------
        /* U */ {'U'_l, 'U'_l, 'U'_l, 'U'_l, 'U'_l, 'U'_l, 'U'_l, 'U'_l, 'U'_l},
        /* X */ {'U'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l},
        /* 0 */ {'U'_l, 'X'_l, '0'_l, '1'_l, 'X'_l, 'X'_l, '0'_l, '1'_l, 'X'_l},
        /* 1 */ {'U'_l, 'X'_l, '1'_l, '0'_l, 'X'_l, 'X'_l, '1'_l, '0'_l, 'X'_l},
        /* Z */ {'U'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l},
        /* W */ {'U'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l},
        /* L */ {'U'_l, 'X'_l, '0'_l, '1'_l, 'X'_l, 'X'_l, '0'_l, '1'_l, 'X'_l},
        /* H */ {'U'_l, 'X'_l, '1'_l, '0'_l, 'X'_l, 'X'_l, '1'_l, '0'_l, 'X'_l},
        /* - */ {'U'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l, 'X'_l},
    };
    return table[static_cast<size_t>(lhs.value())]
                [static_cast<size_t>(rhs.value())];
}

constexpr Bit operator^(const Bit& lhs, const Bit& rhs) noexcept {
    return to_bit(Logic(lhs) ^ Logic(rhs));
}

constexpr Logic operator~(const Logic& value) noexcept {
    constexpr Logic const table[9] = {
        // ----------------
        //  U  X  0  1  Z  W  L  H  -
        // ----------------
        'U'_l, 'X'_l, '1'_l, '0'_l, 'X'_l, 'X'_l, '1'_l, '0'_l, 'X'_l,
    };
    return table[static_cast<size_t>(value.value())];
}

constexpr Bit operator~(const Bit& value) noexcept {
    return to_bit(~Logic(value));
}

constexpr bool is_01(const Logic& value) noexcept {
    return value == '0'_l || value == '1'_l || value == 'L'_l || value == 'H'_l;
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

#endif  // COCONEXT_LOGIC_HPP

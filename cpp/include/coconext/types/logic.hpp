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

    constexpr Logic() noexcept = default;
    constexpr Logic(value_type value) noexcept : value_(value) {}
    constexpr value_type value() const noexcept { return value_; }

  private:
    value_type value_ = _0;
};

class Bit {
  public:
    enum class value_type : uint8_t {
        _0,
        _1,
    };
    using enum value_type;

    constexpr Bit() noexcept = default;
    constexpr Bit(value_type value) noexcept : value_(value) {}
    constexpr value_type value() const noexcept { return value_; }

    // Implicit conversion from Bit to Logic mimics subtype upcasting.
    constexpr operator Logic() const noexcept {
        return value_ == _0 ? Logic::_0 : Logic::_1;
    }

  private:
    value_type value_ = _0;
};

constexpr bool operator==(Logic const& lhs, Logic const& rhs) noexcept {
    return lhs.value() == rhs.value();
}

constexpr bool operator==(Bit const& lhs, Bit const& rhs) noexcept {
    return lhs.value() == rhs.value();
}

template <Character CharType>
constexpr Logic to_logic(CharType c) {
    switch (c) {
    case '0':
        return Logic::_0;
    case '1':
        return Logic::_1;
    case 'X':
    case 'x':
        return Logic::X;
    case 'Z':
    case 'z':
        return Logic::Z;
    case 'U':
    case 'u':
        return Logic::U;
    case 'W':
    case 'w':
        return Logic::W;
    case 'L':
    case 'l':
        return Logic::L;
    case 'H':
    case 'h':
        return Logic::H;
    case '-':
        return Logic::DC;
    default:
        throw std::invalid_argument("Invalid logic literal");
    }
}

template <Character CharType>
constexpr Bit to_bit(CharType value) {
    if (value == '0') {
        return Bit::_0;
    } else if (value == '1') {
        return Bit::_1;
    } else {
        throw std::invalid_argument("Invalid bit value");
    }
}

constexpr Logic operator""_l(char c) { return to_logic(c); }

constexpr Bit operator""_b(char c) { return to_bit(c); }

constexpr Logic to_logic(std::string_view value) {
    if (value.size() != 1) {
        throw std::invalid_argument("Invalid logic value");
    }
    return to_logic(value[0]);
}

constexpr Logic to_logic(char const* value) {
    // Without this, string literals will choose the bool overload since const
    // char* can decay to bool which is defined by the language and the
    // string_view overload is defined in a library.
    return to_logic(std::string_view(value));
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

constexpr Logic to_logic(bool value) { return value ? Logic::_1 : Logic::_0; }

constexpr Logic to_logic(Bit const& value) { return value; }

constexpr Bit to_bit(std::string_view value) {
    if (value.size() != 1) {
        throw std::invalid_argument("Invalid bit value");
    }
    return to_bit(value[0]);
}

constexpr Bit to_bit(char const* value) {
    // Without this, string literals will choose the bool overload since const
    // char* can decay to bool which is defined by the language and the
    // string_view overload is defined in a library.
    return to_bit(std::string_view(value));
}

template <Integer IntType>
constexpr Bit to_bit(IntType value) {
    if (value == 0) {
        return Bit::_0;
    } else if (value == 1) {
        return Bit::_1;
    } else {
        throw std::invalid_argument("Invalid bit value");
    }
}

constexpr Bit to_bit(bool value) { return value ? Bit::_1 : Bit::_0; }

constexpr Bit to_bit(Logic const& value) {
    if (value == Logic::_0) {
        return Bit::_0;
    } else if (value == Logic::_1) {
        return Bit::_1;
    } else {
        throw std::invalid_argument("Invalid bit value");
    }
}

constexpr std::string_view to_string(Logic const& value) noexcept {
    constexpr char const* const str_map[] = {"0", "1", "X", "Z", "U", "W", "L", "H", "-"};
    return str_map[static_cast<size_t>(value.value())];
}

constexpr char to_char(Logic const& value) noexcept {
    constexpr char char_map[] = {'0', '1', 'X', 'Z', 'U', 'W', 'L', 'H', '-'};
    return char_map[static_cast<size_t>(value.value())];
}

constexpr int to_int(Logic const& value) {
    if (value.value() == Logic::_0 || value.value() == Logic::L) {
        return 0;
    } else if (value.value() == Logic::_1 || value.value() == Logic::H) {
        return 1;
    } else {
        throw std::invalid_argument(
            "Cannot convert Logic with non-binary value to integer"
        );
    }
}

constexpr Logic operator|(Logic const& lhs, Logic const& rhs) noexcept {
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
    return table[static_cast<size_t>(lhs.value())][static_cast<size_t>(rhs.value())];
}

constexpr Bit operator|(Bit const& lhs, Bit const& rhs) noexcept {
    return Bit::value_type(int(lhs.value()) | int(rhs.value()));
}

constexpr Logic operator&(Logic const& lhs, Logic const& rhs) noexcept {
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
    return table[static_cast<size_t>(lhs.value())][static_cast<size_t>(rhs.value())];
}

constexpr Bit operator&(Bit const& lhs, Bit const& rhs) noexcept {
    return Bit::value_type(int(lhs.value()) & int(rhs.value()));
}

constexpr Logic operator^(Logic const& lhs, Logic const& rhs) noexcept {
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
    return table[static_cast<size_t>(lhs.value())][static_cast<size_t>(rhs.value())];
}

constexpr Bit operator^(Bit const& lhs, Bit const& rhs) noexcept {
    return Bit::value_type(int(lhs.value()) ^ int(rhs.value()));
}

constexpr Logic operator~(Logic const& value) noexcept {
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

constexpr Bit operator~(Bit const& value) noexcept {
    return value.value() == Bit::_0 ? Bit::_1 : Bit::_0;
}

constexpr bool is_01(Logic const& value) noexcept {
    return value == Logic::_0 || value == Logic::_1 || value == Logic::L
        || value == Logic::H;
}

constexpr bool is_01(Bit const&) noexcept { return true; }

enum class ResolveMethod {
    ERROR,
    WEAK,
    ZEROS,
    ONES,
    RANDOM,
};

Logic resolve(Logic const& value, ResolveMethod method);

inline Bit resolve(Bit const& value, ResolveMethod method) { return value; }

}  // namespace coconext::types

template <>
struct std::hash<coconext::types::Logic> {
    size_t operator()(coconext::types::Logic const& logic) const noexcept {
        return std::hash<coconext::types::Logic::value_type>()(logic.value());
    }
};

template <>
struct std::hash<coconext::types::Bit> {
    size_t operator()(coconext::types::Bit const& bit) const noexcept {
        return std::hash<coconext::types::Bit::value_type>()(bit.value());
    }
};

#endif  // COCONEXT_LOGIC_HPP

#ifndef COCONEXT_LOGIC_HPP
#define COCONEXT_LOGIC_HPP

#include <coconext/types/concepts.hpp>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace coconext::types {

enum class ResolveMethod {
    ERROR,
    WEAK,
    ZEROS,
    ONES,
    RANDOM,
};

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

    constexpr bool is_resolvable() const noexcept {
        return value_ == _0 || value_ == _1 || value_ == L || value_ == H;
    }

    Logic resolve(ResolveMethod method) const;

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

    constexpr bool is_resolvable() const noexcept { return true; }

    constexpr Bit resolve(ResolveMethod) const noexcept { return *this; }

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

constexpr std::string_view to_string(Bit const& value) noexcept {
    return value.value() == Bit::_0 ? "0" : "1";
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

template <typename T>
struct is_logic : std::false_type {};

template <>
struct is_logic<Logic> : std::true_type {};

template <>
struct is_logic<Bit> : std::true_type {};

template <typename T>
concept LogicType = is_logic<std::remove_cv_t<T>>::value;

static_assert(LogicType<Logic>);
static_assert(LogicType<Bit>);
static_assert(!LogicType<int>);

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

template <>
struct std::formatter<coconext::types::Logic> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Logic formatter takes no format spec");
        }
        return it;
    }

    auto format(coconext::types::Logic const& v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "Logic{{{}}}", coconext::types::to_string(v));
    }
};

template <>
struct std::formatter<coconext::types::Bit> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Bit formatter takes no format spec");
        }
        return it;
    }

    auto format(coconext::types::Bit v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "Bit{{{}}}", coconext::types::to_string(v));
    }
};

// Pull in the Logic/Bit-specialized array formatters so any TU that can name
// Logic or Bit also sees the LogicType-constrained std::formatter
// specializations for DynArray<Logic/Bit>, Array<Logic/Bit, R>, and
// their ArraySlice views. Without this, a TU that includes only logic.hpp +
// (dyn_array|array).hpp would instantiate the generic Array formatter
// for those element types, while a TU with the umbrella header would
// instantiate the terse LogicType form -- an ODR violation. The recursive
// include is safe: logic_array.hpp's guarded include of logic.hpp is a no-op
// here (we're at the bottom of logic.hpp's body), and Logic/Bit are fully
// defined above.
#include <coconext/types/logic_array.hpp>

#endif  // COCONEXT_LOGIC_HPP

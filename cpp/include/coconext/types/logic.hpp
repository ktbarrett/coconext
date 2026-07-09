#ifndef COCONEXT_LOGIC_HPP
#define COCONEXT_LOGIC_HPP

#include <coconext/types/concepts.hpp>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
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

class Bit;

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

    template <Character C>
    constexpr explicit Logic(C c) {
        switch (c) {
        case '0':
            value_ = _0;
            return;
        case '1':
            value_ = _1;
            return;
        case 'X':
        case 'x':
            value_ = X;
            return;
        case 'Z':
        case 'z':
            value_ = Z;
            return;
        case 'U':
        case 'u':
            value_ = U;
            return;
        case 'W':
        case 'w':
            value_ = W;
            return;
        case 'L':
        case 'l':
            value_ = L;
            return;
        case 'H':
        case 'h':
            value_ = H;
            return;
        case '-':
            value_ = DC;
            return;
        default:
            throw std::invalid_argument(
                std::string("Invalid logic literal: '") + static_cast<char>(c) + "'"
            );
        }
    }
    constexpr explicit Logic(std::string_view s)
        : Logic(
              (s.size() == 1) ? s[0] : throw std::invalid_argument("Invalid logic value")
          ) {}
    constexpr explicit Logic(char const* s) : Logic(std::string_view(s)) {}
    template <Integer I>
    constexpr explicit Logic(I v) {
        if (v == 0) {
            value_ = _0;
        } else if (v == 1) {
            value_ = _1;
        } else {
            throw std::invalid_argument("Invalid logic value");
        }
    }
    constexpr explicit Logic(bool v) noexcept : value_(v ? _1 : _0) {}

    constexpr value_type value() const noexcept { return value_; }

    // Egress conversion operators.
    template <Integer T>
    constexpr explicit operator T() const {
        if (value_ == _0 || value_ == L) {
            return T(0);
        } else if (value_ == _1 || value_ == H) {
            return T(1);
        } else {
            throw std::invalid_argument(
                "Cannot convert Logic with non-binary value to integer"
            );
        }
    }
    template <Character C>
    constexpr explicit operator C() const noexcept {
        constexpr char char_map[] = {'0', '1', 'X', 'Z', 'U', 'W', 'L', 'H', '-'};
        return static_cast<C>(char_map[static_cast<size_t>(value_)]);
    }

    explicit constexpr operator bool() const {
        switch (value_) {
        case _0:
        case L:
            return false;
        case _1:
        case H:
            return true;
        default:
            throw std::out_of_range("Cannot convert Logic with non-0/1 value to bool");
        }
    }

    std::optional<Bit> resolve(ResolveMethod method) const noexcept;
    std::optional<Bit> resolve() const noexcept;

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

    template <Character C>
    constexpr explicit Bit(C c) {
        if (c == '0') {
            value_ = _0;
        } else if (c == '1') {
            value_ = _1;
        } else {
            throw std::invalid_argument(
                std::string("Invalid bit value: '") + static_cast<char>(c) + "'"
            );
        }
    }
    constexpr explicit Bit(std::string_view s)
        : Bit((s.size() == 1) ? s[0] : throw std::invalid_argument("Invalid bit value")) {}
    constexpr explicit Bit(char const* s) : Bit(std::string_view(s)) {}
    template <Integer I>
    constexpr explicit Bit(I v) {
        if (v == 0) {
            value_ = _0;
        } else if (v == 1) {
            value_ = _1;
        } else {
            throw std::invalid_argument("Invalid bit value");
        }
    }
    constexpr explicit Bit(bool v) noexcept : value_(v ? _1 : _0) {}
    constexpr explicit Bit(Logic const& v) {
        if (v.value() == Logic::_0 || v.value() == Logic::L) {
            value_ = _0;
        } else if (v.value() == Logic::_1 || v.value() == Logic::H) {
            value_ = _1;
        } else {
            throw std::invalid_argument("Invalid bit value");
        }
    }

    constexpr value_type value() const noexcept { return value_; }

    constexpr std::optional<Bit> resolve(ResolveMethod) const noexcept { return *this; }
    constexpr std::optional<Bit> resolve() const noexcept { return *this; }

    template <Integer T>
    constexpr explicit operator T() const noexcept {
        return value_ == _0 ? T(0) : T(1);
    }
    template <Character C>
    constexpr explicit operator C() const noexcept {
        return value_ == _0 ? C('0') : C('1');
    }

    // Implicit conversion from Bit to Logic mimics subtype upcasting.
    constexpr operator Logic() const noexcept {
        return value_ == _0 ? Logic::_0 : Logic::_1;
    }

    explicit constexpr operator bool() const noexcept { return value_ != _0; }

  private:
    value_type value_ = _0;
};

inline std::optional<Bit> Logic::resolve() const noexcept {
    return resolve(ResolveMethod::WEAK);
}

constexpr bool operator==(Logic const& lhs, Logic const& rhs) noexcept {
    return lhs.value() == rhs.value();
}

constexpr bool operator==(Bit const& lhs, Bit const& rhs) noexcept {
    return lhs.value() == rhs.value();
}

// Prevent cross-type equality.
bool operator==(Logic const&, Bit const&) = delete;
bool operator==(Bit const&, Logic const&) = delete;

consteval Logic operator""_l(char c) { return Logic(c); }

consteval Bit operator""_b(char c) { return Bit(c); }

constexpr std::string_view to_string(Logic const& value) noexcept {
    constexpr char const* const str_map[] = {"0", "1", "X", "Z", "U", "W", "L", "H", "-"};
    return str_map[static_cast<size_t>(value.value())];
}

constexpr std::string_view to_string(Bit const& value) noexcept {
    return value.value() == Bit::_0 ? "0" : "1";
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

constexpr Logic& operator&=(Logic& lhs, Logic const& rhs) noexcept {
    return lhs = lhs & rhs;
}
constexpr Logic& operator|=(Logic& lhs, Logic const& rhs) noexcept {
    return lhs = lhs | rhs;
}
constexpr Logic& operator^=(Logic& lhs, Logic const& rhs) noexcept {
    return lhs = lhs ^ rhs;
}
constexpr Bit& operator&=(Bit& lhs, Bit const& rhs) noexcept { return lhs = lhs & rhs; }
constexpr Bit& operator|=(Bit& lhs, Bit const& rhs) noexcept { return lhs = lhs | rhs; }
constexpr Bit& operator^=(Bit& lhs, Bit const& rhs) noexcept { return lhs = lhs ^ rhs; }

constexpr Logic& inplace_not(Logic& v) noexcept { return v = ~v; }
constexpr Bit& inplace_not(Bit& v) noexcept { return v = ~v; }

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

#endif  // COCONEXT_LOGIC_HPP

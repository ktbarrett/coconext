#ifndef COCONEXT_INT_COMMON_HPP
#define COCONEXT_INT_COMMON_HPP

#include <algorithm>
#include <coconext/types/direction.hpp>
#include <coconext/types/range.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

namespace coconext::types::detail {

// Storage for Unsigned/Signed is a single native 64-bit word, so widths are
// limited to 1..64 for now. Wider types will need a different storage strategy.
constexpr unsigned int_max_width = std::numeric_limits<uint64_t>::digits;

constexpr void check_width(unsigned w) {
    if (w < 1 || w > int_max_width) {
        throw std::out_of_range(
            "integer width " + std::to_string(w) + " out of range (1.."
            + std::to_string(int_max_width) + ")"
        );
    }
}

// Mask with the low `w` bits set. w must be in 1..64 (see check_width). The
// w==64 case is special-cased because `1ull << 64` is undefined behavior.
constexpr uint64_t uint_mask(unsigned w) noexcept {
    return w >= int_max_width ? ~uint64_t{0} : ((uint64_t{1} << w) - 1);
}

// Reduce an unsigned value to its low `w` bits (two's-complement wrap for the
// Unsigned interpretation). Collapses to identity at w==64.
constexpr uint64_t uint_wrap(uint64_t v, unsigned w) noexcept { return v & uint_mask(w); }

// Interpret the low `w` bits of `bits` as a two's-complement signed value and
// sign-extend into a full int64_t. Collapses to a plain reinterpret at w==64.
constexpr int64_t sint_wrap(uint64_t bits, unsigned w) noexcept {
    auto const masked = uint_wrap(bits, w);
    auto const sign_bit = uint64_t{1} << (w - 1);
    // If the sign bit is set, set all bits above w-1.
    return static_cast<int64_t>((masked ^ sign_bit) - sign_bit);
}

// Build a {n-1 DOWNTO 0} Range from a length, the HDL convention for numeric
// types. Used by Unsigned/Signed/DynUnsigned/DynSigned constructors that take
// just a width.
constexpr Range int_downto_range(size_t n) {
    return Range{static_cast<Range::value_type>(n) - 1, Direction::DOWNTO, 0};
}

// Range NTTP dispatcher for the Unsigned<...>/Signed<...> template aliases.
// Same shape as the logic_array `make_logic_static_range`: defaults to DOWNTO
// when the user didn't pick a direction explicitly.
//   `Unsigned<8>`        -> {7 DOWNTO 0}
//   `Unsigned<Range{R}>` -> R (passthrough)
//   `Unsigned<7, 0>`     -> {7 DOWNTO 0}  (auto)
//   `Unsigned<3, 3>`     -> {3 DOWNTO 3}  (default DOWNTO when L == R)
//   `Unsigned<0, 7>`     -> {0 TO 7}      (auto)
//   `Unsigned<L, D, R>`  -> {L D R}       (explicit)
template <auto... Args>
constexpr Range make_int_range() {
    static_assert(
        sizeof...(Args) >= 1 && sizeof...(Args) <= 3,
        "Unsigned/Signed takes 1 to 3 range args"
    );
    constexpr auto t = std::tuple{Args...};
    if constexpr (sizeof...(Args) == 1) {
        using First = std::remove_cvref_t<decltype(std::get<0>(t))>;
        if constexpr (std::is_same_v<First, Range>) {
            return std::get<0>(t);
        } else {
            static_assert(
                std::integral<First>,
                "single template arg must be a Range value or an integral length"
            );
            static_assert(std::get<0>(t) >= 0, "length must be non-negative");
            return int_downto_range(static_cast<size_t>(std::get<0>(t)));
        }
    } else if constexpr (sizeof...(Args) == 2) {
        constexpr Range r{
            static_cast<Range::value_type>(std::get<0>(t)),
            static_cast<Range::value_type>(std::get<1>(t))
        };
        if constexpr (r.left == r.right) {
            return Range{r.left, Direction::DOWNTO, r.right};
        } else {
            return r;
        }
    } else {  // 3
        static_assert(
            std::is_same_v<std::remove_cvref_t<decltype(std::get<1>(t))>, Direction>,
            "three-arg form requires (left, Direction, right)"
        );
        return Range{
            static_cast<Range::value_type>(std::get<0>(t)),
            std::get<1>(t),
            static_cast<Range::value_type>(std::get<2>(t))
        };
    }
}

// Result range for a binary op between two fixed-width numeric types: width
// max(width(a), width(b)), normalized to {N-1 DOWNTO 0} (VHDL numeric_std
// convention).
template <Range A, Range B>
inline constexpr Range int_result_range =
    int_downto_range(std::max(A.length(), B.length()));

}  // namespace coconext::types::detail

#endif  // COCONEXT_INT_COMMON_HPP

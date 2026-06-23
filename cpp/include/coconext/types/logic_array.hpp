#ifndef COCONEXT_LOGIC_ARRAY_HPP
#define COCONEXT_LOGIC_ARRAY_HPP

#include <algorithm>
#include <coconext/types/array.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/string_literal.hpp>
#include <coconext/types/vector.hpp>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>

// The Logic/Bit Vector ctors below delegate to VectorImpl ctors that are
// only constexpr in C++23 (gated by COCONEXT_VECTOR_CONSTEXPR inside
// vector.hpp, which #undefs the macro at its end). Mirror the gating here
// so clang doesn't diagnose "constexpr ctor never produces a constant
// expression" under C++20.
#if __cplusplus >= 202302L
#define COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR constexpr
#else
#define COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR
#endif

namespace coconext::types {

namespace detail {

// Build a {N-1 DOWNTO 0} range from a length, matching HDL bit-vector
// convention. Used by Logic/Bit array constructors and the LogicArray/BitArray
// template aliases.
constexpr Range logic_downto_range(size_t n) {
    if (n > static_cast<size_t>(std::numeric_limits<Range::value_type>::max())) {
        throw std::length_error("logic array length overflows Range::value_type");
    }
    return Range{static_cast<Range::value_type>(n) - 1, Direction::DOWNTO, 0};
}

// Like make_static_range, but the length-only form defaults to DOWNTO, and
// the (L, H) form picks DOWNTO for L == R (where the generic auto-direction
// would pick TO). Used by LogicArray<>/BitArray<> aliases so:
//   `LogicArray<8>`      -> {7 DOWNTO 0}   (instead of {0 TO 7})
//   `LogicArray<7, 0>`   -> {7 DOWNTO 0}   (auto-direction; unchanged)
//   `LogicArray<3, 3>`   -> {3 DOWNTO 3}   (instead of {3 TO 3} -- the only
//                                           2-arg case that differs from the
//                                           generic auto-direction rule)
//   `LogicArray<0, 7>`   -> {0 TO 7}       (auto-direction; unchanged)
// The 3-arg `(L, D, H)` form still respects the user's explicit direction.
template <auto... Args>
constexpr Range make_logic_static_range() {
    if constexpr (sizeof...(Args) == 1) {
        constexpr auto first = std::get<0>(std::tuple{Args...});
        using First = std::remove_cvref_t<decltype(first)>;
        if constexpr (std::integral<First>) {
            static_assert(
                first >= 0, "LogicArray<N>/BitArray<N>: N (length) must be non-negative"
            );
            static_assert(
                static_cast<long long>(first)
                    <= std::numeric_limits<Range::value_type>::max(),
                "LogicArray<N>/BitArray<N>: N overflows Range::value_type"
            );
            return Range{static_cast<Range::value_type>(first) - 1, Direction::DOWNTO, 0};
        } else {
            return make_static_range<Args...>();
        }
    } else if constexpr (sizeof...(Args) == 2) {
        constexpr auto r = make_static_range<Args...>();
        if constexpr (r.left == r.right) {
            return Range{r.left, Direction::DOWNTO, r.right};
        } else {
            return r;
        }
    } else {
        return make_static_range<Args...>();
    }
}

}  // namespace detail

template <>
class Vector<Logic> : public detail::VectorImpl<Logic> {
  public:
    using detail::VectorImpl<Logic>::VectorImpl;
    using detail::VectorImpl<Logic>::operator=;

    // Length-only constructors default to DOWNTO (HDL bit-vector convention).
    // The base ctors default to TO; these specializations override that for
    // logic arrays so `Vector<Logic>({...})` and friends produce ranges that
    // match what HDL code expects.
    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(size_t length)
        : detail::VectorImpl<Logic>(detail::logic_downto_range(length)) {}

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::initializer_list<Logic> init)
        : detail::VectorImpl<Logic>(init, detail::logic_downto_range(init.size())) {}

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, Logic>
              && (!std::derived_from<std::remove_cvref_t<R>, detail::VectorImpl<Logic>>)
    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(R const& obj)
        : detail::VectorImpl<Logic>(
              obj, detail::logic_downto_range(std::ranges::size(obj))
          ) {}
};

template <>
class Vector<Bit> : public detail::VectorImpl<Bit> {
  public:
    using detail::VectorImpl<Bit>::VectorImpl;
    using detail::VectorImpl<Bit>::operator=;

    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(size_t length)
        : detail::VectorImpl<Bit>(detail::logic_downto_range(length)) {}

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::initializer_list<Bit> init)
        : detail::VectorImpl<Bit>(init, detail::logic_downto_range(init.size())) {}

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, Bit>
              && (!std::derived_from<std::remove_cvref_t<R>, detail::VectorImpl<Bit>>)
    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(R const& obj)
        : detail::VectorImpl<Bit>(obj, detail::logic_downto_range(std::ranges::size(obj))) {
    }
};

template <RangedSequence T>
    requires LogicType<std::ranges::range_value_t<T>>
auto resolve(T const& self, ResolveMethod method) {
    if constexpr (StaticRangedSequence<T>) {
        std::optional<detail::Array<Bit, std::remove_cvref_t<T>::static_range>> result{
            std::in_place
        };
        auto out = result->begin();
        for (auto const& v : self) {
            auto r = v.resolve(method);
            if (!r) {
                return decltype(result){std::nullopt};
            }
            *out++ = *r;
        }
        return result;
    } else {
        std::optional<Vector<Bit>> result{std::in_place, self.range()};
        auto out = result->begin();
        for (auto const& v : self) {
            auto r = v.resolve(method);
            if (!r) {
                return decltype(result){std::nullopt};
            }
            *out++ = *r;
        }
        return result;
    }
}

template <RangedSequence T>
    requires LogicType<std::ranges::range_value_t<T>>
auto resolve(T const& self) {
    return resolve(self, ResolveMethod::WEAK);
}

template <RangedSequence T>
    requires LogicType<std::ranges::range_value_t<T>>
auto and_reduce(T const& self) {
    using Elem = std::ranges::range_value_t<T>;
    Elem result{Elem::_1};
    for (auto const& v : self) {
        result = result & v;
    }
    return result;
}

template <RangedSequence T>
    requires LogicType<std::ranges::range_value_t<T>>
auto or_reduce(T const& self) {
    using Elem = std::ranges::range_value_t<T>;
    Elem result{Elem::_0};
    for (auto const& v : self) {
        result = result | v;
    }
    return result;
}

template <RangedSequence T>
    requires LogicType<std::ranges::range_value_t<T>>
auto xor_reduce(T const& self) {
    using Elem = std::ranges::range_value_t<T>;
    Elem result{Elem::_0};
    for (auto const& v : self) {
        result = result ^ v;
    }
    return result;
}

using LogicVector = Vector<Logic>;
using BitVector = Vector<Bit>;

// LogicArray<N> / BitArray<N> default to {N-1 DOWNTO 0} for the length-only
// form (HDL bit-vector convention). Explicit-Range and (L, [D,] H) forms pass
// through unchanged. The generic `Array<Logic, N>` sugar still produces TO --
// users wanting HDL conventions should prefer these aliases.
template <auto... Args>
using LogicArray = detail::Array<Logic, detail::make_logic_static_range<Args...>()>;

template <auto... Args>
using BitArray = detail::Array<Bit, detail::make_logic_static_range<Args...>()>;

template <typename T>
concept LogicArrayType =
    RangedSequence<T> && LogicType<std::ranges::range_value_t<std::remove_cvref_t<T>>>;

// -- Bitwise array operations -----------------------------------------------

namespace detail {

template <RangedSequence LHS, RangedSequence RHS, typename Op>
    requires LogicType<std::ranges::range_value_t<LHS>>
          && LogicType<std::ranges::range_value_t<RHS>>
auto logic_binop(LHS const& lhs, RHS const& rhs, Op op) {
    using result_elem = decltype(op(
        std::declval<std::ranges::range_value_t<LHS>>(),
        std::declval<std::ranges::range_value_t<RHS>>()
    ));
    // When both sides have compile-time-known ranges, fold the length check
    // into a static_assert and return a stack-allocated static Array. A
    // runtime range on either side forces a heap-allocated Vector.
    if constexpr (StaticRangedSequence<LHS> && StaticRangedSequence<RHS>) {
        constexpr auto LR = std::remove_cvref_t<LHS>::static_range;
        constexpr auto RR = std::remove_cvref_t<RHS>::static_range;
        static_assert(
            LR.length() == RR.length(), "Bitwise operation requires arrays of equal length"
        );
        // LR's length is already bounded by Range::value_type (int32_t).
        Array<
            result_elem,
            Range{static_cast<Range::value_type>(LR.length()) - 1, Direction::DOWNTO, 0}>
            result{};
        std::transform(
            std::ranges::begin(lhs),
            std::ranges::end(lhs),
            std::ranges::begin(rhs),
            result.begin(),
            op
        );
        return result;
    } else {
        if (lhs.range().length() != rhs.range().length()) {
            throw std::invalid_argument(
                "Bitwise operation requires arrays of equal length, got "
                + std::to_string(lhs.range().length()) + " and "
                + std::to_string(rhs.range().length())
            );
        }
        // lhs.range() was constructed validly so its length already fits in
        // Range::value_type.
        auto const n = static_cast<Range::value_type>(lhs.range().length());
        Vector<result_elem> result(Range{n - 1, Direction::DOWNTO, 0});
        std::transform(
            std::ranges::begin(lhs),
            std::ranges::end(lhs),
            std::ranges::begin(rhs),
            result.begin(),
            op
        );
        return result;
    }
}

// Scalar broadcast: per-element op(elem, scalar). Same static/dynamic dispatch
// shape as logic_binop, but no length-check branch (a scalar fits any array).
template <RangedSequence Arr, LogicType Scalar, typename Op>
    requires LogicType<std::ranges::range_value_t<Arr>>
auto logic_binop_scalar(Arr const& arr, Scalar const& s, Op op) {
    using result_elem = decltype(op(
        std::declval<std::ranges::range_value_t<Arr>>(), std::declval<Scalar>()
    ));
    if constexpr (StaticRangedSequence<Arr>) {
        constexpr auto AR = std::remove_cvref_t<Arr>::static_range;
        Array<
            result_elem,
            Range{static_cast<Range::value_type>(AR.length()) - 1, Direction::DOWNTO, 0}>
            result{};
        std::transform(
            std::ranges::begin(arr),
            std::ranges::end(arr),
            result.begin(),
            [&s, &op](auto const& v) { return op(v, s); }
        );
        return result;
    } else {
        auto const n = static_cast<Range::value_type>(arr.range().length());
        Vector<result_elem> result(Range{n - 1, Direction::DOWNTO, 0});
        std::transform(
            std::ranges::begin(arr),
            std::ranges::end(arr),
            result.begin(),
            [&s, &op](auto const& v) { return op(v, s); }
        );
        return result;
    }
}

}  // namespace detail

template <RangedSequence LHS, RangedSequence RHS>
    requires LogicArrayType<LHS> && LogicArrayType<RHS>
auto operator&(LHS const& lhs, RHS const& rhs) {
    return detail::logic_binop(lhs, rhs, [](auto const& a, auto const& b) {
        return a & b;
    });
}

template <RangedSequence LHS, RangedSequence RHS>
    requires LogicArrayType<LHS> && LogicArrayType<RHS>
auto operator|(LHS const& lhs, RHS const& rhs) {
    return detail::logic_binop(lhs, rhs, [](auto const& a, auto const& b) {
        return a | b;
    });
}

template <RangedSequence LHS, RangedSequence RHS>
    requires LogicArrayType<LHS> && LogicArrayType<RHS>
auto operator^(LHS const& lhs, RHS const& rhs) {
    return detail::logic_binop(lhs, rhs, [](auto const& a, auto const& b) {
        return a ^ b;
    });
}

// Scalar-on-left broadcasts a single Bit/Logic across an array.
template <LogicType Scalar, RangedSequence Arr>
    requires LogicArrayType<Arr>
auto operator&(Scalar const& s, Arr const& arr) {
    return detail::logic_binop_scalar(arr, s, [](auto const& v, auto const& sc) {
        return sc & v;
    });
}

template <LogicType Scalar, RangedSequence Arr>
    requires LogicArrayType<Arr>
auto operator|(Scalar const& s, Arr const& arr) {
    return detail::logic_binop_scalar(arr, s, [](auto const& v, auto const& sc) {
        return sc | v;
    });
}

template <LogicType Scalar, RangedSequence Arr>
    requires LogicArrayType<Arr>
auto operator^(Scalar const& s, Arr const& arr) {
    return detail::logic_binop_scalar(arr, s, [](auto const& v, auto const& sc) {
        return sc ^ v;
    });
}

// Scalar-on-right mirror.
template <RangedSequence Arr, LogicType Scalar>
    requires LogicArrayType<Arr>
auto operator&(Arr const& arr, Scalar const& s) {
    return detail::logic_binop_scalar(arr, s, [](auto const& v, auto const& sc) {
        return v & sc;
    });
}

template <RangedSequence Arr, LogicType Scalar>
    requires LogicArrayType<Arr>
auto operator|(Arr const& arr, Scalar const& s) {
    return detail::logic_binop_scalar(arr, s, [](auto const& v, auto const& sc) {
        return v | sc;
    });
}

template <RangedSequence Arr, LogicType Scalar>
    requires LogicArrayType<Arr>
auto operator^(Arr const& arr, Scalar const& s) {
    return detail::logic_binop_scalar(arr, s, [](auto const& v, auto const& sc) {
        return v ^ sc;
    });
}

// -- Compound bitwise assignment -------------------------------------------
//
// Free functions taking the LHS by forwarding reference so they bind to both
// owning arrays (`arr &= mask`) and slice rvalues (`arr[{2, 1}] &= mask`).
// RHS may be another array (length-checked) or a scalar Bit/Logic (broadcast).
// Element-type compatibility is enforced by the underlying `v = v <op> rhs`
// assignment -- e.g. `BitArray &= LogicArray` fails to compile because the
// elementwise `Bit & Logic` returns Logic and Logic isn't assignable to Bit.

namespace detail {

template <typename LHS, typename Scalar, typename Op>
constexpr void logic_inplace_scalar(LHS& lhs, Scalar const& s, Op op) {
    for (auto& v : lhs) {
        v = op(v, s);
    }
}

template <typename LHS, typename RHS, typename Op>
constexpr void logic_inplace_array(LHS& lhs, RHS const& rhs, Op op) {
    // When both sides have compile-time-known ranges, fold the length check
    // into a static_assert -- mismatch becomes a compile error instead of a
    // runtime throw, and the runtime branch drops out of generated code.
    if constexpr (StaticRangedSequence<LHS> && StaticRangedSequence<RHS>) {
        static_assert(
            std::remove_cvref_t<LHS>::static_range.length()
                == std::remove_cvref_t<RHS>::static_range.length(),
            "Bitwise compound assignment requires arrays of equal length"
        );
    } else if (lhs.range().length() != rhs.range().length()) {
        throw std::invalid_argument(
            "Bitwise compound assignment requires arrays of equal length, got "
            + std::to_string(lhs.range().length()) + " and "
            + std::to_string(rhs.range().length())
        );
    }
    auto it = std::ranges::begin(rhs);
    for (auto& v : lhs) {
        v = op(v, *it++);
    }
}

}  // namespace detail

template <typename LHS, LogicType Scalar>
    requires LogicArrayType<std::remove_cvref_t<LHS>>
constexpr decltype(auto) operator&=(LHS&& lhs, Scalar const& rhs) {
    detail::logic_inplace_scalar(lhs, rhs, [](auto const& a, auto const& b) {
        return a & b;
    });
    return std::forward<LHS>(lhs);
}

template <typename LHS, RangedSequence RHS>
    requires LogicArrayType<std::remove_cvref_t<LHS>> && LogicArrayType<RHS>
constexpr decltype(auto) operator&=(LHS&& lhs, RHS const& rhs) {
    detail::logic_inplace_array(lhs, rhs, [](auto const& a, auto const& b) {
        return a & b;
    });
    return std::forward<LHS>(lhs);
}

template <typename LHS, LogicType Scalar>
    requires LogicArrayType<std::remove_cvref_t<LHS>>
constexpr decltype(auto) operator|=(LHS&& lhs, Scalar const& rhs) {
    detail::logic_inplace_scalar(lhs, rhs, [](auto const& a, auto const& b) {
        return a | b;
    });
    return std::forward<LHS>(lhs);
}

template <typename LHS, RangedSequence RHS>
    requires LogicArrayType<std::remove_cvref_t<LHS>> && LogicArrayType<RHS>
constexpr decltype(auto) operator|=(LHS&& lhs, RHS const& rhs) {
    detail::logic_inplace_array(lhs, rhs, [](auto const& a, auto const& b) {
        return a | b;
    });
    return std::forward<LHS>(lhs);
}

template <typename LHS, LogicType Scalar>
    requires LogicArrayType<std::remove_cvref_t<LHS>>
constexpr decltype(auto) operator^=(LHS&& lhs, Scalar const& rhs) {
    detail::logic_inplace_scalar(lhs, rhs, [](auto const& a, auto const& b) {
        return a ^ b;
    });
    return std::forward<LHS>(lhs);
}

template <typename LHS, RangedSequence RHS>
    requires LogicArrayType<std::remove_cvref_t<LHS>> && LogicArrayType<RHS>
constexpr decltype(auto) operator^=(LHS&& lhs, RHS const& rhs) {
    detail::logic_inplace_array(lhs, rhs, [](auto const& a, auto const& b) {
        return a ^ b;
    });
    return std::forward<LHS>(lhs);
}

// In-place complement of an array. Like the compound assignment ops, taken by
// forwarding reference so it binds to slice rvalues too.
template <typename Arr>
    requires LogicArrayType<std::remove_cvref_t<Arr>>
constexpr decltype(auto) inplace_not(Arr&& arr) {
    for (auto& v : arr) {
        v = ~v;
    }
    return std::forward<Arr>(arr);
}

// -- Concatenation ---------------------------------------------------------

namespace detail {

template <typename T>
concept ConcatOperand = LogicType<std::remove_cvref_t<T>> || LogicArrayType<T>;

template <typename T>
struct concat_elem_type {
    using type = std::remove_cvref_t<T>;
};

template <typename T>
    requires LogicArrayType<T>
struct concat_elem_type<T> {
    using type = std::ranges::range_value_t<std::remove_cvref_t<T>>;
};

template <typename T>
using concat_elem_t = typename concat_elem_type<T>::type;

template <typename T>
constexpr size_t concat_static_size() {
    if constexpr (LogicType<std::remove_cvref_t<T>>) {
        return 1;
    } else {
        return std::remove_cvref_t<T>::static_range.length();
    }
}

template <typename T>
constexpr size_t concat_runtime_size(T const& t) {
    if constexpr (LogicType<std::remove_cvref_t<T>>) {
        return 1;
    } else {
        return t.range().length();
    }
}

template <typename Elem, typename OutIt, typename T>
constexpr void concat_copy_one(OutIt& out, T const& t) {
    if constexpr (LogicType<std::remove_cvref_t<T>>) {
        *out++ = static_cast<Elem>(t);
    } else {
        for (auto const& v : t) {
            *out++ = static_cast<Elem>(v);
        }
    }
}

}  // namespace detail

// Variadic concat of Logic/Bit scalars and Logic/Bit arrays. First argument
// occupies the high bits; within each operand, elements are taken in iteration
// order (begin to end) regardless of the operand's direction. Result is a
// static `Array<Elem, {N-1 DOWNTO 0}>` when every operand has a compile-time
// size (scalar or StaticRangedSequence), else a runtime `Vector<Elem>`.
// Element type is `std::common_type_t<...>` over the operand element types
// (Logic if any operand is Logic, else Bit).
template <typename... Args>
    requires(sizeof...(Args) >= 1) && (... && detail::ConcatOperand<Args>)
auto concat(Args const&... args) {
    using result_elem = std::common_type_t<detail::concat_elem_t<Args>...>;
    constexpr bool all_static =
        (... && (LogicType<std::remove_cvref_t<Args>> || StaticRangedSequence<Args>));
    if constexpr (all_static) {
        constexpr size_t N = (0 + ... + detail::concat_static_size<Args>());
        static_assert(
            N <= static_cast<size_t>(std::numeric_limits<Range::value_type>::max()),
            "concat result length overflows Range::value_type"
        );
        Array<
            result_elem,
            Range{static_cast<Range::value_type>(N) - 1, Direction::DOWNTO, 0}>
            result{};
        auto out = result.begin();
        (detail::concat_copy_one<result_elem>(out, args), ...);
        return result;
    } else {
        size_t const total = (size_t{0} + ... + detail::concat_runtime_size(args));
        Vector<result_elem> result(total);
        auto out = result.begin();
        (detail::concat_copy_one<result_elem>(out, args), ...);
        return result;
    }
}

template <RangedSequence T>
    requires LogicArrayType<T>
auto operator~(T const& arr) {
    using elem_t = std::ranges::range_value_t<T>;
    if constexpr (StaticRangedSequence<T>) {
        constexpr auto AR = std::remove_cvref_t<T>::static_range;
        // AR's length is already bounded by Range::value_type (int32_t).
        Array<
            elem_t,
            Range{static_cast<Range::value_type>(AR.length()) - 1, Direction::DOWNTO, 0}>
            result{};
        std::transform(
            std::ranges::begin(arr),
            std::ranges::end(arr),
            result.begin(),
            [](auto const& v) { return ~v; }
        );
        return result;
    } else {
        // arr.range() was constructed validly so its length already fits in
        // Range::value_type.
        auto const n = static_cast<Range::value_type>(arr.range().length());
        Vector<elem_t> result(Range{n - 1, Direction::DOWNTO, 0});
        std::transform(
            std::ranges::begin(arr),
            std::ranges::end(arr),
            result.begin(),
            [](auto const& v) { return ~v; }
        );
        return result;
    }
}

// -- Conversion to/from string ------------------------------------------------

namespace detail {

template <typename ElemT, typename CharToElem>
Vector<ElemT> parse_logic_string(std::string_view s, CharToElem char_to_elem) {
    size_t count = 0;
    for (char c : s) {
        if (c != '_') {
            ++count;
        }
    }
    if (count > static_cast<size_t>(std::numeric_limits<Range::value_type>::max())) {
        throw std::length_error("logic string too long for Range::value_type");
    }
    auto const n = static_cast<Range::value_type>(count);
    Vector<ElemT> result(Range{n - 1, Direction::DOWNTO, 0});
    size_t j = 0;
    for (char c : s) {
        if (c != '_') {
            *(result.begin() + j++) = char_to_elem(c);
        }
    }
    return result;
}

template <StringLiteral S>
constexpr size_t count_non_underscore() {
    size_t n = 0;
    for (size_t i = 0; i < S.size; ++i) {
        if (S.data[i] != '_') {
            ++n;
        }
    }
    return n;
}

// Emit '<prefix>[range]{"<bit-string>"}' for Logic/Bit-element arrays.
template <RangedSequence ArrayT, typename OutIt>
    requires LogicType<std::ranges::range_value_t<ArrayT>>
OutIt format_logic_array(std::string_view prefix, ArrayT const& arr, OutIt out) {
    out = std::format_to(out, "{}{}{{\"", prefix, arr.range());
    for (auto const& elem : arr) {
        *out++ = to_char(elem);
    }
    *out++ = '"';
    *out++ = '}';
    return out;
}

}  // namespace detail

template <RangedSequence T>
    requires LogicType<std::ranges::range_value_t<T>>
std::string to_string(T const& arr) {
    std::string result;
    result.reserve(arr.range().length());
    for (auto const& elem : arr) {
        result += to_char(elem);
    }
    return result;
}

inline Vector<Logic> to_logic_array(std::string_view s) {
    return detail::parse_logic_string<Logic>(s, [](char c) { return to_logic(c); });
}

inline Vector<Bit> to_bit_array(std::string_view s) {
    return detail::parse_logic_string<Bit>(s, [](char c) { return to_bit(c); });
}

// -- String-literal UDL ----------------------------------------------------

template <StringLiteral S>
constexpr auto operator""_l() {
    constexpr auto N = detail::count_non_underscore<S>();
    static_assert(
        N <= static_cast<size_t>(std::numeric_limits<Range::value_type>::max()),
        "logic literal too long for Range::value_type"
    );
    constexpr Range R{static_cast<Range::value_type>(N) - 1, Direction::DOWNTO, 0};
    LogicArray<R> result{};
    auto out = result.begin();
    for (auto in = S.data; in != S.data + S.size; ++in) {
        if (*in != '_') {
            *out++ = to_logic(*in);
        }
    }
    return result;
}

template <StringLiteral S>
constexpr auto operator""_b() {
    constexpr auto N = detail::count_non_underscore<S>();
    static_assert(
        N <= static_cast<size_t>(std::numeric_limits<Range::value_type>::max()),
        "bit literal too long for Range::value_type"
    );
    constexpr Range R{static_cast<Range::value_type>(N) - 1, Direction::DOWNTO, 0};
    BitArray<R> result{};
    auto out = result.begin();
    for (auto in = S.data; in != S.data + S.size; ++in) {
        if (*in != '_') {
            *out++ = to_bit(*in);
        }
    }
    return result;
}

}  // namespace coconext::types

// Per-type formatters for Logic/Bit-element arrays. These are more-specialized
// partial specializations of std::formatter than the corresponding non-logic
// formatters in array.hpp / vector.hpp / array_base.hpp, so they win by C++20
// partial-ordering rules when both headers are visible.
//
// Output format: `<Prefix>[range]{"<bit-string>"}`. The prefix encodes the
// type kind (Array / Vector / ArraySlice) and the element type (Logic / Bit).

#define COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(PREFIX, ...)                                 \
    struct std::formatter<__VA_ARGS__> {                                                   \
        constexpr auto parse(std::format_parse_context& ctx) {                             \
            auto it = ctx.begin();                                                         \
            if (it != ctx.end() && *it != '}') {                                           \
                throw std::format_error(PREFIX " formatter takes no format spec");         \
            }                                                                              \
            return it;                                                                     \
        }                                                                                  \
        auto format(__VA_ARGS__ const& v, std::format_context& ctx) const {                \
            return coconext::types::detail::format_logic_array(PREFIX, v, ctx.out());      \
        }                                                                                  \
    }

template <coconext::types::Range R>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(
    "LogicArray", coconext::types::detail::Array<coconext::types::Logic, R>
);

template <coconext::types::Range R>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(
    "BitArray", coconext::types::detail::Array<coconext::types::Bit, R>
);

template <>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(
    "LogicVector", coconext::types::Vector<coconext::types::Logic>
);

template <>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(
    "BitVector", coconext::types::Vector<coconext::types::Bit>
);

template <typename ArrayT>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Logic>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(
    "LogicArraySlice", coconext::types::ArraySlice<ArrayT>
);

template <typename ArrayT>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Bit>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER("BitArraySlice", coconext::types::ArraySlice<ArrayT>);

template <typename ArrayT, coconext::types::Range R>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Logic>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(
    "LogicArraySlice", coconext::types::StaticArraySlice<ArrayT, R>
);

template <typename ArrayT, coconext::types::Range R>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Bit>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(
    "BitArraySlice", coconext::types::StaticArraySlice<ArrayT, R>
);

#undef COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER

#undef COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR

#endif  // COCONEXT_LOGIC_ARRAY_HPP

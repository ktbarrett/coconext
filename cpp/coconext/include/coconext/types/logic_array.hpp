#ifndef COCONEXT_LOGIC_ARRAY_HPP
#define COCONEXT_LOGIC_ARRAY_HPP

#include <algorithm>
#include <coconext/types/array.hpp>
#include <coconext/types/bit_array.hpp>
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

// The Logic/Bit Vector ctors below delegate to VectorImpl ctors that are only constexpr in
// C++23.
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

    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::string_view s)
        : Vector(s, detail::logic_downto_range(s.size())) {}
    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(char const* s)
        : Vector(std::string_view(s)) {}

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::string_view s, Range r)
        : detail::VectorImpl<Logic>(r) {
        if (s.size() != r.length()) {
            throw std::invalid_argument(
                "String of length " + std::to_string(s.size())
                + " does not match Range length " + std::to_string(r.length())
            );
        }
        auto out = this->begin();
        for (char c : s) {
            *out++ = Logic(c);
        }
    }
    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(char const* s, Range r)
        : Vector(std::string_view(s), r) {}
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

    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::string_view s)
        : Vector(s, detail::logic_downto_range(s.size())) {}
    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(char const* s)
        : Vector(std::string_view(s)) {}

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::string_view s, Range r)
        : detail::VectorImpl<Bit>(r) {
        if (s.size() != r.length()) {
            throw std::invalid_argument(
                "String of length " + std::to_string(s.size())
                + " does not match Range length " + std::to_string(r.length())
            );
        }
        auto out = this->begin();
        for (char c : s) {
            *out++ = Bit(c);
        }
    }
    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(char const* s, Range r)
        : Vector(std::string_view(s), r) {}
};

namespace detail {

template <Range R>
class Array<Logic, R> : public ArrayImpl<Logic, R> {
  public:
    using ArrayImpl<Logic, R>::ArrayImpl;
    using ArrayImpl<Logic, R>::operator=;

    explicit constexpr Array(std::string_view s) : ArrayImpl<Logic, R>() {
        if (s.size() != R.length()) {
            throw std::invalid_argument(
                "String of length " + std::to_string(s.size())
                + " does not match Array length " + std::to_string(R.length())
            );
        }
        auto out = this->begin();
        for (char c : s) {
            *out++ = Logic(c);
        }
    }
    explicit constexpr Array(char const* s) : Array(std::string_view(s)) {}
};

}  // namespace detail

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
    // runtime range on either side forces a heap-allocated Vector. The result
    // Range is always normalized to {N-1 DOWNTO 0} (HDL convention).
    if constexpr (StaticRangedSequence<LHS> && StaticRangedSequence<RHS>) {
        constexpr auto LR = std::remove_cvref_t<LHS>::static_range;
        constexpr auto RR = std::remove_cvref_t<RHS>::static_range;
        static_assert(
            LR.length() == RR.length(), "Bitwise operation requires arrays of equal length"
        );
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
// Result Range is normalized to {N-1 DOWNTO 0}, matching logic_binop.
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
        Array<
            elem_t,
            Range{std::remove_cvref_t<T>::static_range.length() - 1, Direction::DOWNTO, 0}>
            result{};
        std::transform(
            std::ranges::begin(arr),
            std::ranges::end(arr),
            result.begin(),
            [](auto const& v) { return ~v; }
        );
        return result;
    } else {
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

template <RangedSequence ArrayT, typename OutIt>
    requires LogicType<std::ranges::range_value_t<ArrayT>>
OutIt format_logic_array(std::string_view prefix, ArrayT const& arr, OutIt out) {
    out = std::format_to(out, "{}{}{{\"", prefix, arr.range());
    for (auto const& elem : arr) {
        *out++ = char(elem);
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
        result += char(elem);
    }
    return result;
}

}  // namespace coconext::types

namespace coconext::literals {

template <coconext::types::StringLiteral S>
consteval auto operator""_l() {
    constexpr auto N = coconext::types::detail::count_non_underscore<S>();
    static_assert(
        N <= static_cast<size_t>(
            std::numeric_limits<coconext::types::Range::value_type>::max()
        ),
        "logic literal too long for Range::value_type"
    );
    constexpr coconext::types::Range R{
        static_cast<coconext::types::Range::value_type>(N) - 1,
        coconext::types::Direction::DOWNTO,
        0
    };
    coconext::types::LogicArray<R> result{};
    auto out = result.begin();
    for (auto in = S.data; in != S.data + S.size; ++in) {
        if (*in != '_') {
            *out++ = coconext::types::Logic(*in);
        }
    }
    return result;
}

}  // namespace coconext::literals

// -- std::formatter specializations -------------------------------------------

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
    "LogicStaticArraySlice", coconext::types::StaticArraySlice<ArrayT, R>
);

template <typename ArrayT, coconext::types::Range R>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Bit>
COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER(
    "BitStaticArraySlice", coconext::types::StaticArraySlice<ArrayT, R>
);

#undef COCONEXT_DEFINE_LOGIC_ARRAY_FORMATTER

#undef COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR

#endif  // COCONEXT_LOGIC_ARRAY_HPP

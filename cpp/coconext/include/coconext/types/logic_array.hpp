#ifndef COCONEXT_LOGIC_ARRAY_HPP
#define COCONEXT_LOGIC_ARRAY_HPP

#include <algorithm>
#include <coconext/types/array.hpp>
#include <coconext/types/bit_array.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/logic_array_common.hpp>
#include <coconext/types/string_literal.hpp>
#include <cstddef>
#include <format>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

namespace coconext::types {

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

template <auto... Args>
using LogicArray = detail::Array<Logic, detail::make_logic_static_range<Args...>()>;

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
    for (auto&& v : lhs) {
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
    for (auto&& v : lhs) {
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
    for (auto&& v : arr) {
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

#endif  // COCONEXT_LOGIC_ARRAY_HPP

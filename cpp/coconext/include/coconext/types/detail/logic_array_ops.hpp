#ifndef COCONEXT_DETAIL_LOGIC_ARRAY_OPS_HPP
#define COCONEXT_DETAIL_LOGIC_ARRAY_OPS_HPP

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <coconext/types/detail/array_base.hpp>
#include <coconext/types/detail/bit_array.hpp>
#include <coconext/types/detail/bit_vector.hpp>
#include <coconext/types/detail/logic.hpp>
#include <coconext/types/detail/logic_array_base.hpp>
#include <coconext/types/direction.hpp>
#include <coconext/types/range.hpp>

namespace coconext::types {

template <typename ArrayT>
    requires LogicType<std::ranges::range_value_t<ArrayT>>
std::optional<Vector<Bit>> ArraySlice<ArrayT>::resolve(ResolveMethod method) const {
    std::optional<Vector<Bit>> result{std::in_place, this->range()};
    auto out = result->begin();
    for (std::ranges::range_value_t<ArrayT> v : *this) {
        auto r = v.resolve(method);
        if (!r) {
            return std::nullopt;
        }
        *out++ = *r;
    }
    return result;
}

template <typename ArrayT, Range R>
    requires LogicType<std::ranges::range_value_t<ArrayT>>
std::optional<detail::Array<Bit, R>> StaticArraySlice<ArrayT, R>::resolve(
    ResolveMethod method
) const {
    std::optional<detail::Array<Bit, R>> result{std::in_place};
    if constexpr (R.length() != 0) {
        auto out = result->begin();
        for (std::ranges::range_value_t<ArrayT> v : *this) {
            auto r = v.resolve(method);
            if (!r) {
                return std::nullopt;
            }
            *out++ = *r;
        }
    }
    return result;
}

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

#endif  // COCONEXT_DETAIL_LOGIC_ARRAY_OPS_HPP

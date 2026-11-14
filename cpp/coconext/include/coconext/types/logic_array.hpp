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
        // TODO Consider supporting visual separators like `_`
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

template <StaticLogicArrayType LHS, StaticLogicArrayType RHS, typename Op>
auto logic_binop(LHS const& lhs, RHS const& rhs, Op op) {
    using result_elem = decltype(op(
        std::declval<std::ranges::range_value_t<LHS>>(),
        std::declval<std::ranges::range_value_t<RHS>>()
    ));
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
}

// Scalar broadcast: per-element op(elem, scalar). Same static/dynamic dispatch
// shape as logic_binop, but no length-check branch (a scalar fits any array).
// Result Range is normalized to {N-1 DOWNTO 0}, matching logic_binop.
template <StaticLogicArrayType Arr, LogicType Scalar, typename Op>
auto logic_binop_scalar(Arr const& arr, Scalar const& s, Op op) {
    using result_elem = decltype(op(
        std::declval<std::ranges::range_value_t<Arr>>(), std::declval<Scalar>()
    ));
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
}

// -- Compound bitwise assignment -------------------------------------------

template <StaticLogicArrayType LHS, LogicType Scalar, typename Op>
constexpr void logic_inplace_scalar(LHS& lhs, Scalar const& s, Op op) {
    for (auto&& v : lhs) {
        v = op(v, s);
    }
}

template <StaticLogicArrayType LHS, StaticLogicArrayType RHS, typename Op>
constexpr void logic_inplace_array(LHS& lhs, RHS const& rhs, Op op) {
    static_assert(
        std::remove_cvref_t<LHS>::static_range.length()
            == std::remove_cvref_t<RHS>::static_range.length(),
        "Bitwise compound assignment requires arrays of equal length"
    );
    auto it = std::ranges::begin(rhs);
    for (auto&& v : lhs) {
        v = op(v, *it++);
    }
}

}  // namespace detail

// -- Concatenation ---------------------------------------------------------

template <typename... Args>
    requires(sizeof...(Args) >= 1)
         && (... && (LogicType<std::remove_cvref_t<Args>> || StaticLogicArrayType<Args>))
auto concat(Args const&... args) {
    using result_elem = std::common_type_t<detail::concat_elem_t<Args>...>;
    constexpr size_t N = (0 + ... + detail::concat_static_size<Args>());
    static_assert(
        N <= static_cast<size_t>(std::numeric_limits<Range::value_type>::max()),
        "concat result length overflows Range::value_type"
    );
    Array<result_elem, Range{static_cast<Range::value_type>(N) - 1, Direction::DOWNTO, 0}>
        result{};
    auto out = result.begin();
    (detail::concat_copy_one<result_elem>(out, args), ...);
    return result;
}

template <StaticLogicArrayType T>
auto operator~(T const& arr) {
    Array<
        std::ranges::range_value_t<T>,
        Range{std::remove_cvref_t<T>::static_range.length() - 1, Direction::DOWNTO, 0}>
        result{};
    std::transform(
        std::ranges::begin(arr), std::ranges::end(arr), result.begin(), [](auto const& v) {
            return ~v;
        }
    );
    return result;
}

template <StaticLogicArrayType T>
auto resolve(T const& self, ResolveMethod method) {
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
}

}  // namespace coconext::types

// -- Conversion to/from string ------------------------------------------------

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

// -- formatting ---------------------------------------------------------------

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Array<coconext::types::Logic, R>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("LogicArray formatter takes no format spec");
        }
        return it;
    }
    auto format(
        coconext::types::detail::Array<coconext::types::Logic, R> const& v,
        std::format_context& ctx
    ) const {
        return coconext::types::detail::format_logic_array("LogicArray", v, ctx.out());
    }
};

#endif  // COCONEXT_LOGIC_ARRAY_HPP

#ifndef COCONEXT_LOGIC_VECTOR_HPP
#define COCONEXT_LOGIC_VECTOR_HPP

#include <coconext/types/logic.hpp>
#include <coconext/types/logic_array_common.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/vector.hpp>

// The LogicVector ctors below delegate to VectorImpl ctors that are only constexpr in
// C++23. BitVector uses DynUInt storage instead.
#if __cplusplus >= 202302L
#define COCONEXT_VECTOR_CONSTEXPR constexpr
#else
#define COCONEXT_VECTOR_CONSTEXPR
#endif

namespace coconext::types {

template <>
class Vector<Logic> : public detail::VectorImpl<Logic> {
  public:
    using detail::VectorImpl<Logic>::VectorImpl;
    using detail::VectorImpl<Logic>::operator=;

    explicit COCONEXT_VECTOR_CONSTEXPR Vector(size_t length)
        : detail::VectorImpl<Logic>(detail::logic_downto_range(length)) {}

    COCONEXT_VECTOR_CONSTEXPR Vector(std::initializer_list<Logic> init)
        : detail::VectorImpl<Logic>(init, detail::logic_downto_range(init.size())) {}

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, Logic>
              && (!std::derived_from<std::remove_cvref_t<R>, detail::VectorImpl<Logic>>)
    explicit COCONEXT_VECTOR_CONSTEXPR Vector(R const& obj)
        : detail::VectorImpl<Logic>(
              obj, detail::logic_downto_range(std::ranges::size(obj))
          ) {}

    explicit COCONEXT_VECTOR_CONSTEXPR Vector(std::string_view s)
        : Vector(s, detail::logic_downto_range(s.size())) {}
    explicit COCONEXT_VECTOR_CONSTEXPR Vector(char const* s)
        : Vector(std::string_view(s)) {}

    COCONEXT_VECTOR_CONSTEXPR Vector(std::string_view s, Range r)
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
    COCONEXT_VECTOR_CONSTEXPR Vector(char const* s, Range r)
        : Vector(std::string_view(s), r) {}
};

using LogicVector = Vector<Logic>;

template <LogicArrayType T>
auto resolve(T const& self, ResolveMethod method) {
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

namespace detail {

template <LogicArrayType LHS, LogicArrayType RHS, typename Op>
auto logic_binop(LHS const& lhs, RHS const& rhs, Op op) {
    using result_elem = decltype(op(
        std::declval<std::ranges::range_value_t<LHS>>(),
        std::declval<std::ranges::range_value_t<RHS>>()
    ));
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

// Scalar broadcast: per-element op(elem, scalar). Same static/dynamic dispatch
// shape as logic_binop, but no length-check branch (a scalar fits any array).
// Result Range is normalized to {N-1 DOWNTO 0}, matching logic_binop.
template <LogicArrayType Arr, LogicType Scalar, typename Op>
auto logic_binop_scalar(Arr const& arr, Scalar const& s, Op op) {
    using result_elem = decltype(op(
        std::declval<std::ranges::range_value_t<Arr>>(), std::declval<Scalar>()
    ));
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

// -- Compound bitwise assignment -------------------------------------------

template <LogicArrayType LHS, LogicType Scalar, typename Op>
constexpr void logic_inplace_scalar(LHS& lhs, Scalar const& s, Op op) {
    for (auto&& v : lhs) {
        v = op(v, s);
    }
}

template <LogicArrayType LHS, LogicArrayType RHS, typename Op>
constexpr void logic_inplace_array(LHS& lhs, RHS const& rhs, Op op) {
    if (lhs.range().length() != rhs.range().length()) {
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

template <LogicArrayType T>
auto operator~(T const& arr) {
    auto const n = static_cast<Range::value_type>(arr.range().length());
    Vector<std::ranges::range_value_t<T>> result(Range{n - 1, Direction::DOWNTO, 0});
    std::transform(
        std::ranges::begin(arr), std::ranges::end(arr), result.begin(), [](auto const& v) {
            return ~v;
        }
    );
    return result;
}

template <typename... Args>
    requires(sizeof...(Args) >= 1)
         && (... && (LogicType<std::remove_cvref_t<Args>> || LogicArrayType<Args>))
auto concat(Args const&... args) {
    using result_elem = std::common_type_t<detail::concat_elem_t<Args>...>;
    size_t const total = (size_t{0} + ... + detail::concat_runtime_size(args));
    Vector<result_elem> result(total);
    auto out = result.begin();
    (detail::concat_copy_one<result_elem>(out, args), ...);
    return result;
}

#undef COCONEXT_VECTOR_CONSTEXPR

}  // namespace coconext::types

template <>
struct std::formatter<coconext::types::Vector<coconext::types::Logic>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("LogicVector formatter takes no format spec");
        }
        return it;
    }
    auto format(
        coconext::types::Vector<coconext::types::Logic> const& v, std::format_context& ctx
    ) const {
        return coconext::types::detail::format_logic_array("LogicVector", v, ctx.out());
    }
};

#endif  // COCONEXT_LOGIC_VECTOR_HPP

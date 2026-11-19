#ifndef COCONEXT_DETAIL_LOGIC_ARRAY_BASE_HPP
#define COCONEXT_DETAIL_LOGIC_ARRAY_BASE_HPP

#include <concepts>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <coconext/types/detail/array_base.hpp>
#include <coconext/types/detail/concepts.hpp>
#include <coconext/types/detail/logic.hpp>
#include <coconext/types/detail/string_literal.hpp>
#include <coconext/types/direction.hpp>
#include <coconext/types/range.hpp>

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
        if constexpr (NativeInteger<First>) {
            static_assert(
                first >= 0, "LogicArray<N>/BitArray<N>: N (length) must be non-negative"
            );
            static_assert(
                fits_range_value_type<first>,
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

template <typename ArrayT>
    requires LogicType<std::ranges::range_value_t<ArrayT>>
class ArraySlice<ArrayT> : public detail::ArraySliceImpl<ArrayT> {
  public:
    using detail::ArraySliceImpl<ArrayT>::ArraySliceImpl;
    using detail::ArraySliceImpl<ArrayT>::operator=;

    constexpr ArraySlice& inplace_not() &
        requires(!std::is_const_v<ArrayT>)
    {
        std::as_const(*this).inplace_not();
        return *this;
    }

    constexpr ArraySlice&& inplace_not() &&
        requires(!std::is_const_v<ArrayT>)
    {
        inplace_not();
        return std::move(*this);
    }

    constexpr ArraySlice const& inplace_not() const&
        requires(!std::is_const_v<ArrayT>)
    {
        for (auto&& v : *this) {
            v = ~v;
        }
        return *this;
    }

    constexpr ArraySlice const&& inplace_not() const&&
        requires(!std::is_const_v<ArrayT>)
    {
        inplace_not();
        return std::move(*this);
    }

    constexpr auto and_reduce() const {
        using Elem = std::ranges::range_value_t<ArrayT>;
        Elem result{Elem::_1};
        for (Elem v : *this) {
            result = result & v;
        }
        return result;
    }

    constexpr auto or_reduce() const {
        using Elem = std::ranges::range_value_t<ArrayT>;
        Elem result{Elem::_0};
        for (Elem v : *this) {
            result = result | v;
        }
        return result;
    }

    constexpr auto xor_reduce() const {
        using Elem = std::ranges::range_value_t<ArrayT>;
        Elem result{Elem::_0};
        for (Elem v : *this) {
            result = result ^ v;
        }
        return result;
    }

    std::optional<Vector<Bit>> resolve(ResolveMethod method = ResolveMethod::WEAK) const;
};

template <typename ArrayT, Range R>
    requires LogicType<std::ranges::range_value_t<ArrayT>>
class StaticArraySlice<ArrayT, R> : public detail::StaticArraySliceImpl<ArrayT, R> {
  public:
    using detail::StaticArraySliceImpl<ArrayT, R>::StaticArraySliceImpl;
    using detail::StaticArraySliceImpl<ArrayT, R>::operator=;

    constexpr StaticArraySlice& inplace_not() &
        requires(!std::is_const_v<ArrayT>)
    {
        std::as_const(*this).inplace_not();
        return *this;
    }

    constexpr StaticArraySlice&& inplace_not() &&
        requires(!std::is_const_v<ArrayT>)
    {
        inplace_not();
        return std::move(*this);
    }

    constexpr StaticArraySlice const& inplace_not() const&
        requires(!std::is_const_v<ArrayT>)
    {
        for (auto&& v : *this) {
            v = ~v;
        }
        return *this;
    }

    constexpr StaticArraySlice const&& inplace_not() const&&
        requires(!std::is_const_v<ArrayT>)
    {
        inplace_not();
        return std::move(*this);
    }

    constexpr auto and_reduce() const {
        using Elem = std::ranges::range_value_t<ArrayT>;
        Elem result{Elem::_1};
        for (Elem v : *this) {
            result = result & v;
        }
        return result;
    }

    constexpr auto or_reduce() const {
        using Elem = std::ranges::range_value_t<ArrayT>;
        Elem result{Elem::_0};
        for (Elem v : *this) {
            result = result | v;
        }
        return result;
    }

    constexpr auto xor_reduce() const {
        using Elem = std::ranges::range_value_t<ArrayT>;
        Elem result{Elem::_0};
        for (Elem v : *this) {
            result = result ^ v;
        }
        return result;
    }

    std::optional<detail::Array<Bit, R>> resolve(
        ResolveMethod method = ResolveMethod::WEAK
    ) const;
};

template <typename T>
concept LogicArrayType =
    RangedSequence<T> && LogicType<std::ranges::range_value_t<std::remove_cvref_t<T>>>;

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

template <StringLiteral Prefix>
struct LogicArrayFormatter {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error(
                std::string(Prefix.data) + " formatter takes no format spec"
            );
        }
        return it;
    }

    template <typename ArrayT>
    auto format(ArrayT const& v, std::format_context& ctx) const {
        return format_logic_array(Prefix.data, v, ctx.out());
    }
};

}  // namespace detail

}  // namespace coconext::types

template <typename ArrayT>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Logic>
struct std::formatter<coconext::types::ArraySlice<ArrayT>>
    : coconext::types::detail::LogicArrayFormatter<"LogicArraySlice"> {};

template <typename ArrayT>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Bit>
struct std::formatter<coconext::types::ArraySlice<ArrayT>>
    : coconext::types::detail::LogicArrayFormatter<"BitArraySlice"> {};

template <typename ArrayT, coconext::types::Range R>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Logic>
struct std::formatter<coconext::types::StaticArraySlice<ArrayT, R>>
    : coconext::types::detail::LogicArrayFormatter<"LogicStaticArraySlice"> {};

template <typename ArrayT, coconext::types::Range R>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
          && std::same_as<
                 std::remove_cv_t<std::ranges::range_value_t<ArrayT>>,
                 coconext::types::Bit>
struct std::formatter<coconext::types::StaticArraySlice<ArrayT, R>>
    : coconext::types::detail::LogicArrayFormatter<"BitStaticArraySlice"> {};

#endif  // COCONEXT_DETAIL_LOGIC_ARRAY_BASE_HPP

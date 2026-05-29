#ifndef COCONEXT_LOGIC_ARRAY_HPP
#define COCONEXT_LOGIC_ARRAY_HPP

#include <algorithm>
#include <coconext/types/array.hpp>
#include <coconext/types/dyn_array.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/string_literal.hpp>
#include <cstddef>
#include <format>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>

namespace coconext::types {

namespace detail {

// CRTP mixin providing the Logic/Bit-specific query and resolution members.
// Inherited by the Logic/Bit specializations of Array, DynArray, ArraySlice,
// and DynArraySlice below. The non-Logic/Bit primaries do NOT inherit this,
// so `Array<int, R>::is_resolvable()` and friends don't exist.
template <typename Self>
struct LogicArrayMixin {
    bool is_resolvable() const noexcept {
        auto const& self = *static_cast<Self const*>(this);
        using Elem = std::ranges::range_value_t<Self>;
        if constexpr (std::same_as<Elem, Bit>) {
            // Every Bit is resolvable; skip the walk.
            return true;
        } else {
            return std::ranges::all_of(self, [](auto const& v) {
                return v.is_resolvable();
            });
        }
    }

    // Element-wise resolve. Returns a static `Array<Elem, R>` when Self has a
    // compile-time range, a heap-allocated `DynArray<Elem>` otherwise. The
    // returned array preserves Self's range (an owner returns the same shape;
    // a slice returns an owner sized like the slice's range).
    auto resolve(ResolveMethod method) const {
        auto const& self = *static_cast<Self const*>(this);
        using Elem = std::ranges::range_value_t<Self>;
        if constexpr (StaticRangedSequence<Self>) {
            ::coconext::types::Array<Elem, Self::range()> result{};
            std::ranges::transform(self, result.begin(), [method](auto const& v) {
                return v.resolve(method);
            });
            return result;
        } else {
            ::coconext::types::DynArray<Elem> result(self.range());
            std::ranges::transform(self, result.begin(), [method](auto const& v) {
                return v.resolve(method);
            });
            return result;
        }
    }
};

}  // namespace detail

// -- Logic/Bit specializations ---------------------------------------------
//
// These specializations make `Array<Logic, R>`, `Array<Bit, R>`,
// `DynArray<Logic>`, `DynArray<Bit>`, and slices over Logic/Bit owners
// inherit `LogicArrayMixin`, gaining `is_resolvable()` and `resolve(method)`
// as members. The primary templates remain unchanged for non-Logic element
// types -- e.g., `Array<int, R>` has no `is_resolvable()`.

namespace detail {

template <Range R>
class Array<Logic, R> : public ArrayImpl<Logic, R>,
                        public LogicArrayMixin<Array<Logic, R>> {
  public:
    using ArrayImpl<Logic, R>::ArrayImpl;
    using ArrayImpl<Logic, R>::operator=;
};

template <Range R>
class Array<Bit, R> : public ArrayImpl<Bit, R>, public LogicArrayMixin<Array<Bit, R>> {
  public:
    using ArrayImpl<Bit, R>::ArrayImpl;
    using ArrayImpl<Bit, R>::operator=;
};

}  // namespace detail

template <>
class DynArray<Logic> : public detail::DynArrayImpl<Logic>,
                        public detail::LogicArrayMixin<DynArray<Logic>> {
  public:
    using detail::DynArrayImpl<Logic>::DynArrayImpl;
    using detail::DynArrayImpl<Logic>::operator=;
};

template <>
class DynArray<Bit> : public detail::DynArrayImpl<Bit>,
                      public detail::LogicArrayMixin<DynArray<Bit>> {
  public:
    using detail::DynArrayImpl<Bit>::DynArrayImpl;
    using detail::DynArrayImpl<Bit>::operator=;
};

// Constrained partial specs that pick up any slice whose owner's element
// type is Logic or Bit, regardless of whether the owner is Array<...>,
// DynArray<...>, or const-qualified.
template <typename ArrayT>
    requires LogicType<std::ranges::range_value_t<ArrayT>>
class DynArraySlice<ArrayT> : public detail::DynArraySliceImpl<ArrayT>,
                              public detail::LogicArrayMixin<DynArraySlice<ArrayT>> {
  public:
    using detail::DynArraySliceImpl<ArrayT>::DynArraySliceImpl;
    using detail::DynArraySliceImpl<ArrayT>::operator=;
};

template <typename ArrayT, Range R>
    requires LogicType<std::ranges::range_value_t<ArrayT>>
class ArraySlice<ArrayT, R> : public detail::ArraySliceImpl<ArrayT, R>,
                              public detail::LogicArrayMixin<ArraySlice<ArrayT, R>> {
  public:
    using detail::ArraySliceImpl<ArrayT, R>::ArraySliceImpl;
    using detail::ArraySliceImpl<ArrayT, R>::operator=;
};

using DynLogicArray = DynArray<Logic>;
using DynBitArray = DynArray<Bit>;

template <auto... Args>
using LogicArray = Array<Logic, Args...>;

template <auto... Args>
using BitArray = Array<Bit, Args...>;

template <typename T>
concept LogicArrayType =
    ArrayType<T> && LogicType<std::ranges::range_value_t<std::remove_cvref_t<T>>>;

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
    // runtime range on either side forces a heap-allocated DynArray.
    if constexpr (StaticRangedSequence<LHS> && StaticRangedSequence<RHS>) {
        constexpr auto LR = std::remove_cvref_t<LHS>::range();
        constexpr auto RR = std::remove_cvref_t<RHS>::range();
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
        DynArray<result_elem> result(Range{n - 1, Direction::DOWNTO, 0});
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
        constexpr auto AR = std::remove_cvref_t<Arr>::range();
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
        DynArray<result_elem> result(Range{n - 1, Direction::DOWNTO, 0});
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

template <RangedSequence T>
    requires LogicArrayType<T>
auto operator~(T const& arr) {
    using elem_t = std::ranges::range_value_t<T>;
    if constexpr (StaticRangedSequence<T>) {
        constexpr auto AR = std::remove_cvref_t<T>::range();
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
        DynArray<elem_t> result(Range{n - 1, Direction::DOWNTO, 0});
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

template <LogicType T>
constexpr std::string_view logic_type_name() {
    if constexpr (std::same_as<T, Logic>) {
        return "Logic";
    } else {
        return "Bit";
    }
}

template <typename ElemT, typename CharToElem>
DynArray<ElemT> parse_logic_string(std::string_view s, CharToElem char_to_elem) {
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
    DynArray<ElemT> result(Range{n - 1, Direction::DOWNTO, 0});
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

template <RangedSequence ArrayT, typename OutIt>
OutIt format_typed_array(std::string_view type_name, ArrayT const& arr, OutIt out) {
    out = std::format_to(out, "{}{}{{", type_name, arr.range());
    bool first = true;
    for (auto const& elem : arr) {
        if (!first) {
            out = std::format_to(out, ", ");
        }
        out = std::format_to(out, "{}", to_string(elem));
        first = false;
    }
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

inline DynArray<Logic> to_logic_array(std::string_view s) {
    return detail::parse_logic_string<Logic>(s, [](char c) { return to_logic(c); });
}

inline DynArray<Bit> to_bit_array(std::string_view s) {
    return detail::parse_logic_string<Bit>(s, [](char c) { return to_bit(c); });
}

// Convert a range of Logic to a DynArray<Bit>. Throws if any element is not
// 0/1/L/H (i.e. every element must be resolvable). Constrained to sized_range
// so the result can be sized up-front from std::ranges::size in O(1) and the
// resolvability check can be fused with the fill into a single pass.
template <std::ranges::sized_range R>
    requires std::same_as<std::ranges::range_value_t<R>, Logic>
DynArray<Bit> to_bit_array(R const& range) {
    auto const sz = std::ranges::size(range);
    if (sz > static_cast<size_t>(std::numeric_limits<Range::value_type>::max())) {
        throw std::length_error("range too long for Range::value_type");
    }
    auto const n = static_cast<Range::value_type>(sz);
    DynArray<Bit> result(Range{n - 1, Direction::DOWNTO, 0});
    auto out = result.begin();
    for (Logic const& v : range) {
        if (!v.is_resolvable()) {
            throw std::invalid_argument(
                "Cannot convert non-resolvable Logic values to BitArray"
            );
        }
        *out++ = to_int(v) ? Bit::_1 : Bit::_0;
    }
    return result;
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

// One LogicType-constrained formatter for every array type that opts into
// is_array (DynArray, Array, DynArraySlice, ArraySlice). The constraint is a
// conjunction of the generic ArrayType constraint plus a LogicType check on
// the element type, so it subsumes the generic std::formatter<ArrayType T> in
// array_base.hpp via C++20 partial specialization ordering with constraints.
// Result: arrays of Logic/Bit print as "Logic[range]{0, 1, X}" instead of
// "[range]{Logic{0}, Logic{1}, Logic{X}}".
template <typename T>
    requires coconext::types::ArrayType<T>
          && coconext::types::detail::Formattable<std::ranges::range_value_t<T>>
          && coconext::types::LogicType<std::ranges::range_value_t<T>>
struct std::formatter<T> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("ArrayType<Logic/Bit> formatter takes no format spec");
        }
        return it;
    }

    auto format(T const& arr, std::format_context& ctx) const {
        return coconext::types::detail::format_typed_array(
            coconext::types::detail::logic_type_name<std::ranges::range_value_t<T>>(),
            arr,
            ctx.out()
        );
    }
};

#endif  // COCONEXT_LOGIC_ARRAY_HPP

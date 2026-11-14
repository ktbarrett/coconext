#ifndef COCONEXT_LOGIC_ARRAY_COMMON_HPP
#define COCONEXT_LOGIC_ARRAY_COMMON_HPP

#include <coconext/types/array.hpp>  // make_static_range
#include <coconext/types/logic.hpp>  // LogicType
#include <coconext/types/range.hpp>  // Range
#include <concepts>                  // integral
#include <cstddef>                   // size_t
#include <limits>                    // numeric_limits
#include <stdexcept>                 // length_error
#include <tuple>                     // tuple, get
#include <type_traits>               // remove_cvref_t

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

template <typename T>
concept LogicArrayType =
    RangedSequence<T> && LogicType<std::ranges::range_value_t<std::remove_cvref_t<T>>>;

template <typename T>
concept StaticLogicArrayType = LogicArrayType<T> && StaticRangedSequence<T>;

template <LogicArrayType T>
auto resolve(T const& self) {
    return resolve(self, ResolveMethod::WEAK);
}

template <LogicArrayType T>
auto and_reduce(T const& self) {
    using Elem = std::ranges::range_value_t<T>;
    Elem result{Elem::_1};
    for (auto const& v : self) {
        result = result & v;
    }
    return result;
}

template <LogicArrayType T>
auto or_reduce(T const& self) {
    using Elem = std::ranges::range_value_t<T>;
    Elem result{Elem::_0};
    for (auto const& v : self) {
        result = result | v;
    }
    return result;
}

template <LogicArrayType T>
auto xor_reduce(T const& self) {
    using Elem = std::ranges::range_value_t<T>;
    Elem result{Elem::_0};
    for (auto const& v : self) {
        result = result ^ v;
    }
    return result;
}

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

// -- Concatenation ------------------------------------------------------------

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

template <typename LHS, typename RHS, typename Op>
auto logic_binop(LHS const& lhs, RHS const& rhs, Op op);

template <typename Arr, typename Scalar, typename Op>
auto logic_binop_scalar(Arr const& arr, Scalar const& s, Op op);

template <typename LHS, typename RHS, typename Op>
constexpr void logic_inplace_array(LHS& lhs, RHS const& rhs, Op op);

template <typename LHS, typename Scalar, typename Op>
constexpr void logic_inplace_scalar(LHS& lhs, Scalar const& s, Op op);

}  // namespace detail

template <LogicArrayType LHS, LogicArrayType RHS>
auto operator&(LHS const& lhs, RHS const& rhs) {
    return detail::logic_binop(
        lhs, rhs, [](typename LHS::value_type const& a, typename RHS::value_type const& b) {
            return a & b;
        }
    );
}

template <LogicArrayType LHS, LogicArrayType RHS>
auto operator|(LHS const& lhs, RHS const& rhs) {
    return detail::logic_binop(
        lhs, rhs, [](typename LHS::value_type const& a, typename RHS::value_type const& b) {
            return a | b;
        }
    );
}

template <LogicArrayType LHS, LogicArrayType RHS>
auto operator^(LHS const& lhs, RHS const& rhs) {
    return detail::logic_binop(
        lhs, rhs, [](typename LHS::value_type const& a, typename RHS::value_type const& b) {
            return a ^ b;
        }
    );
}

// Scalar-on-left broadcasts a single Bit/Logic across an array.
template <LogicType Scalar, LogicArrayType Arr>
auto operator&(Scalar const& s, Arr const& arr) {
    return detail::logic_binop_scalar(arr, s, [](Arr const& v, Scalar const& sc) {
        return sc & v;
    });
}

template <LogicType Scalar, LogicArrayType Arr>
auto operator|(Scalar const& s, Arr const& arr) {
    return detail::logic_binop_scalar(arr, s, [](Arr const& v, Scalar const& sc) {
        return sc | v;
    });
}

template <LogicType Scalar, LogicArrayType Arr>
auto operator^(Scalar const& s, Arr const& arr) {
    return detail::logic_binop_scalar(arr, s, [](Arr const& v, Scalar const& sc) {
        return sc ^ v;
    });
}

// Scalar-on-right mirror.
template <LogicArrayType Arr, LogicType Scalar>
auto operator&(Arr const& arr, Scalar const& s) {
    return detail::logic_binop_scalar(arr, s, [](Arr const& v, Scalar const& sc) {
        return v & sc;
    });
}

template <LogicArrayType Arr, LogicType Scalar>
auto operator|(Arr const& arr, Scalar const& s) {
    return detail::logic_binop_scalar(arr, s, [](Arr const& v, Scalar const& sc) {
        return v | sc;
    });
}

template <LogicArrayType Arr, LogicType Scalar>
auto operator^(Arr const& arr, Scalar const& s) {
    return detail::logic_binop_scalar(arr, s, [](Arr const& v, Scalar const& sc) {
        return v ^ sc;
    });
}

template <LogicArrayType LHS, LogicType Scalar>
constexpr decltype(auto) operator&=(LHS&& lhs, Scalar const& rhs) {
    detail::logic_inplace_scalar(lhs, rhs, [](LHS const& a, Scalar const& b) {
        return a & b;
    });
    return std::forward<LHS>(lhs);
}

template <LogicArrayType LHS, LogicArrayType RHS>
constexpr decltype(auto) operator&=(LHS&& lhs, RHS const& rhs) {
    detail::logic_inplace_array(lhs, rhs, [](LHS const& a, RHS const& b) { return a & b; });
    return std::forward<LHS>(lhs);
}

template <LogicArrayType LHS, LogicType Scalar>
constexpr decltype(auto) operator|=(LHS&& lhs, Scalar const& rhs) {
    detail::logic_inplace_scalar(lhs, rhs, [](LHS const& a, Scalar const& b) {
        return a | b;
    });
    return std::forward<LHS>(lhs);
}

template <LogicArrayType LHS, LogicArrayType RHS>
constexpr decltype(auto) operator|=(LHS&& lhs, RHS const& rhs) {
    detail::logic_inplace_array(lhs, rhs, [](LHS const& a, RHS const& b) { return a | b; });
    return std::forward<LHS>(lhs);
}

template <LogicArrayType LHS, LogicType Scalar>
constexpr decltype(auto) operator^=(LHS&& lhs, Scalar const& rhs) {
    detail::logic_inplace_scalar(lhs, rhs, [](LHS const& a, Scalar const& b) {
        return a ^ b;
    });
    return std::forward<LHS>(lhs);
}

template <LogicArrayType LHS, LogicArrayType RHS>
constexpr decltype(auto) operator^=(LHS&& lhs, RHS const& rhs) {
    detail::logic_inplace_array(lhs, rhs, [](LHS const& a, RHS const& b) { return a ^ b; });
    return std::forward<LHS>(lhs);
}

template <LogicArrayType Arr>
constexpr decltype(auto) inplace_not(Arr&& arr) {
    for (auto&& v : arr) {
        v = ~v;
    }
    return std::forward<Arr>(arr);
}

}  // namespace coconext::types

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

#endif  // COCONEXT_LOGIC_ARRAY_COMMON_HPP

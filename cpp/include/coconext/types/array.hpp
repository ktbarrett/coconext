#ifndef COCONEXT_ARRAY_HPP
#define COCONEXT_ARRAY_HPP

#include <coconext/types/direction.hpp>
#include <coconext/types/dynamic_array.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/static_array.hpp>
#include <concepts>
#include <cstddef>
#include <limits>
#include <tuple>
#include <type_traits>

namespace coconext::types {

namespace detail {

// Build a static Range from the trailing template arguments of Array<T, ...>.
//   1 arg, integral N                -> Range(N)              i.e. {0, TO, N-1}
//   1 arg, Range R                   -> R
//   2 args, (left, right)            -> Range{left, right}    direction inferred
//   3 args, (left, Direction, right) -> Range{left, dir, right}
// Compile-time check that an integral NTTP value fits in
// Range::value_type without silent narrowing. Used by the 2/3-arg branches
// of make_static_range below to give a clean diagnostic instead of a
// silently-truncating static_cast.
template <auto V>
constexpr bool fits_range_value_type =
    static_cast<long long>(V) >= std::numeric_limits<Range::value_type>::min()
    && static_cast<long long>(V) <= std::numeric_limits<Range::value_type>::max();

template <auto... Args>
constexpr Range make_static_range() {
    using std::get;
    static_assert(
        sizeof...(Args) >= 1 && sizeof...(Args) <= 3,
        "Array<T, ...> takes 0 args (dynamic) or 1-3 range args (static)"
    );
    constexpr auto t = std::tuple{Args...};
    if constexpr (sizeof...(Args) == 1) {
        using First = std::remove_cvref_t<decltype(get<0>(t))>;
        static_assert(
            std::is_same_v<First, Range> || std::integral<First>,
            "single template arg must be a Range value or an integral length"
        );
        if constexpr (std::is_same_v<First, Range>) {
            return get<0>(t);
        } else {
            static_assert(get<0>(t) >= 0, "Array<T, N>: N (length) must be non-negative");
            return Range(static_cast<size_t>(get<0>(t)));
        }
    } else if constexpr (sizeof...(Args) == 2) {
        static_assert(
            fits_range_value_type<get<0>(t)>,
            "Array<T, L, H>: L does not fit in Range::value_type (int32_t)"
        );
        static_assert(
            fits_range_value_type<get<1>(t)>,
            "Array<T, L, H>: H does not fit in Range::value_type (int32_t)"
        );
        return Range{
            static_cast<Range::value_type>(get<0>(t)),
            static_cast<Range::value_type>(get<1>(t))
        };
    } else {  // 3
        static_assert(
            std::is_same_v<std::remove_cvref_t<decltype(get<1>(t))>, Direction>,
            "three-arg form requires (left, Direction, right)"
        );
        static_assert(
            fits_range_value_type<get<0>(t)>,
            "Array<T, L, D, H>: L does not fit in Range::value_type (int32_t)"
        );
        static_assert(
            fits_range_value_type<get<2>(t)>,
            "Array<T, L, D, H>: H does not fit in Range::value_type (int32_t)"
        );
        return Range{
            static_cast<Range::value_type>(get<0>(t)),
            get<1>(t),
            static_cast<Range::value_type>(get<2>(t))
        };
    }
}

// Dispatcher customization point selecting the array implementation backing
// Array<T, ...>. The empty-pack primary resolves to DynamicArray<T>; the
// non-empty pack specialization resolves to StaticArray<T, R> with R built
// from the trailing args by make_static_range.
template <typename T, auto... Args>
struct ArrayImpl {
    using type = StaticArray<T, make_static_range<Args...>()>;
};

template <typename T>
struct ArrayImpl<T> {
    using type = DynamicArray<T>;
};

}  // namespace detail

// Unified entry point for the array types. The number and types of the
// non-type template arguments select the underlying class:
//   Array<T>                                -> DynamicArray<T>
//   Array<T, N>           (integral N)      -> StaticArray<T, Range(N)>
//                                              i.e. {0, TO, N-1}
//   Array<T, R>           (Range R)         -> StaticArray<T, R>
//   Array<T, L, H>        (integral L, H)   -> StaticArray<T, Range{L, H}>
//                                              direction inferred from L vs H
//   Array<T, L, D, H>     (D = Direction)   -> StaticArray<T, Range{L, D, H}>
template <typename T, auto... Args>
using Array = typename detail::ArrayImpl<T, Args...>::type;

}  // namespace coconext::types

#endif  // COCONEXT_ARRAY_HPP

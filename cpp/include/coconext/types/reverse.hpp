#ifndef COCONEXT_REVERSE_HPP
#define COCONEXT_REVERSE_HPP

#include <algorithm>
#include <coconext/types/array.hpp>
#include <coconext/types/array_base.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/vector.hpp>
#include <ranges>

namespace coconext::types {

template <typename T, Range R>
constexpr detail::Array<T, reverse(R)> reverse(detail::Array<T, R> const& a) {
    detail::Array<T, reverse(R)> result;
    std::ranges::reverse_copy(a, result.begin());
    return result;
}

template <typename T>
Vector<T> reverse(Vector<T> const& v) {
    Vector<T> result(reverse(v.range()));
    std::ranges::reverse_copy(v, result.begin());
    return result;
}

template <typename ArrayT>
Vector<std::ranges::range_value_t<ArrayT>> reverse(ArraySlice<ArrayT> const& s) {
    Vector<std::ranges::range_value_t<ArrayT>> result(reverse(s.range()));
    std::ranges::reverse_copy(s, result.begin());
    return result;
}

template <typename ArrayT, Range R>
constexpr detail::Array<std::ranges::range_value_t<ArrayT>, reverse(R)> reverse(
    StaticArraySlice<ArrayT, R> const& s
) {
    detail::Array<std::ranges::range_value_t<ArrayT>, reverse(R)> result;
    std::ranges::reverse_copy(s, result.begin());
    return result;
}

}  // namespace coconext::types

#endif  // COCONEXT_REVERSE_HPP

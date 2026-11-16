#ifndef COCONEXT_REVERSE_HPP
#define COCONEXT_REVERSE_HPP

#include <algorithm>
#include <ranges>

#include <coconext/types/array.hpp>
#include <coconext/types/array_base.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/vector.hpp>

namespace coconext::types {

template <typename ArrayT>
auto detail::ArraySliceImpl<ArrayT>::reverse() const {
    Vector<std::ranges::range_value_t<ArrayT>> result(range().reverse());
    std::ranges::reverse_copy(*this, result.begin());
    return result;
}

template <typename ArrayT, Range R>
constexpr auto detail::StaticArraySliceImpl<ArrayT, R>::reverse() const {
    detail::Array<std::ranges::range_value_t<ArrayT>, R.reverse()> result;
    std::ranges::reverse_copy(*this, result.begin());
    return result;
}

}  // namespace coconext::types

#endif  // COCONEXT_REVERSE_HPP

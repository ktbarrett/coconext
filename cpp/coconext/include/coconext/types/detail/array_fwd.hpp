#ifndef COCONEXT_DETAIL_ARRAY_FWD_HPP
#define COCONEXT_DETAIL_ARRAY_FWD_HPP

#include <coconext/types/range.hpp>

namespace coconext::types {

namespace detail {

template <typename T, Range R>
class Array;

}  // namespace detail

template <typename ValueT>
class Vector;

template <typename ArrayT>
class ArraySlice;
template <typename ArrayT, Range R>
class StaticArraySlice;

}  // namespace coconext::types

#endif  // COCONEXT_DETAIL_ARRAY_FWD_HPP

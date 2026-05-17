#ifndef COCONEXT_ARRAY_HPP
#define COCONEXT_ARRAY_HPP

#include <coconext/types/dynamic_array.hpp>

namespace coconext::types {

namespace detail {

// Dispatcher customization point selecting the array implementation backing Array<T, ...>.
template <typename T>
struct ArrayImpl {
    using type = DynamicArray<T>;
};

}  // namespace detail

// Unified entry point for the array types.
template <typename T>
using Array = typename detail::ArrayImpl<T>::type;

}  // namespace coconext::types

#endif  // COCONEXT_ARRAY_HPP

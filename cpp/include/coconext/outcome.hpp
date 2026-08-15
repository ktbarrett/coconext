#ifndef COCONEXT_OUTCOME_HPP
#define COCONEXT_OUTCOME_HPP

#include <exception>

namespace coconext {

namespace detail {

template <typename T>
struct Value {
    T value;
};

template <>
struct Value<void> {};

}  // namespace detail

class CancelledError : public std::exception {
  public:
    char const* what() const noexcept override { return "CancelledError"; }
};

}  // namespace coconext

#endif  // COCONEXT_OUTCOME_HPP

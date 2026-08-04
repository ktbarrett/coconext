#ifndef COCONEXT_OUTCOME_HPP
#define COCONEXT_OUTCOME_HPP

#include <exception>
#include <string>
#include <string_view>

namespace coconext {

namespace detail {

template <typename T>
struct Value {
    T value;
};

template <>
struct Value<void> {};

struct Exception {
    std::exception_ptr exception;
};

}  // namespace detail

class Cancelled : public std::exception {
  public:
    Cancelled() noexcept : msg_() {}
    Cancelled(std::string_view message) : msg_(message) {}

    char const* what() const noexcept override { return msg_.c_str(); }

  private:
    std::string msg_;
};

}  // namespace coconext

#endif  // COCONEXT_OUTCOME_HPP

#ifndef COCONEXT_OUTCOME_HPP
#define COCONEXT_OUTCOME_HPP

#include <exception>
#include <string>
#include <string_view>

namespace coconext {

namespace detail {

template <typename T>
struct Result {
    T value;
};

template <>
struct Result<void> {};

struct Exception {
    std::exception_ptr exception;
};

class Cancelled : public std::exception {
  public:
    Cancelled() noexcept : msg_("") {}
    Cancelled(std::string_view message) : msg_(message) {}

    char const* what() const noexcept override { return msg_.c_str(); }

  private:
    std::string msg_;
};

}  // namespace detail

using Cancelled = detail::Cancelled;

}  // namespace coconext

#endif  // COCONEXT_OUTCOME_HPP

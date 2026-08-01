#ifndef COCONEXT_OUTCOME_HPP
#define COCONEXT_OUTCOME_HPP

#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

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

template <typename T>
class Outcome {
  public:
    Outcome(T value) : v_(detail::Result<T>{std::move(value)}) {}
    Outcome(std::exception_ptr exc) : v_(detail::Exception{exc}) {}

    bool has_value() const noexcept {
        return std::holds_alternative<detail::Result<T>>(v_);
    }
    bool has_exception() const noexcept {
        return std::holds_alternative<detail::Exception>(v_);
    }

    T value() const {
        if (std::holds_alternative<detail::Exception>(v_)) {
            std::rethrow_exception(std::get<detail::Exception>(v_).exception);
        }
        return std::get<detail::Result<T>>(v_).value;
    }
    std::exception_ptr exception() const noexcept {
        if (std::holds_alternative<detail::Exception>(v_)) {
            return std::get<detail::Exception>(v_).exception;
        }
        return nullptr;
    }

  private:
    std::variant<detail::Result<T>, detail::Exception> v_;
};

template <>
class Outcome<void> {
  public:
    Outcome() : v_(detail::Result<void>{}) {}
    Outcome(std::exception_ptr exc) : v_(detail::Exception{exc}) {}

    bool has_value() const noexcept {
        return std::holds_alternative<detail::Result<void>>(v_);
    }
    bool has_exception() const noexcept {
        return std::holds_alternative<detail::Exception>(v_);
    }

    void value() const {
        if (std::holds_alternative<detail::Exception>(v_)) {
            std::rethrow_exception(std::get<detail::Exception>(v_).exception);
        }
    }
    std::exception_ptr exception() const noexcept {
        if (std::holds_alternative<detail::Exception>(v_)) {
            return std::get<detail::Exception>(v_).exception;
        }
        return nullptr;
    }

  private:
    std::variant<detail::Result<void>, detail::Exception> v_;
};

}  // namespace coconext

#endif  // COCONEXT_OUTCOME_HPP

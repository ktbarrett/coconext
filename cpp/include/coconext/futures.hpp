#ifndef COCONEXT_FUTURES_HPP
#define COCONEXT_FUTURES_HPP

#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>

#include <coconext/cancelled.hpp>

namespace coconext::futures {

template <typename T>
class Future;

// The Awaiter for a Future
template <typename T>
class FutureAwaiter {
    template <typename U>
    friend class Future;

    Future<T>& future_;

    explicit FutureAwaiter(Future<T>& future) : future_(future) {}

  public:
    bool await_ready() const noexcept { return not future_.done(); }
    void await_suspend(std::coroutine_handle<> h) {
        // Implementation for suspending the coroutine until the future is ready.
    }
    T await_resume() { return future_.result(); }
};

// Single-shot, multiple-consumer awaitable object.
template <typename T>
class Future {
    std::optional<T> value_ = std::nullopt;
    std::exception_ptr exc_ = nullptr;
    bool cancelled_ = false;

  public:
    bool done() noexcept { return value_.has_value() || exc_ != nullptr; }
    bool cancelled() const noexcept { return cancelled_; }
    T result() {
        if (exc_) {
            std::rethrow_exception(exc_);
        }
        if (value_.has_value()) {
            return *value_;
        }
        throw std::runtime_error("Future does not have a result");
    }
    std::exception_ptr exception() const noexcept { return exc_; }
    void set_result(T const& value) { value_ = value; }
    void set_exception(std::exception_ptr exc) noexcept { exc_ = exc; }
    void cancel() noexcept {
        exc_ = std::make_exception_ptr(coconext::Cancelled());
        cancelled_ = true;
    }
    FutureAwaiter<T> operator co_await() { return FutureAwaiter<T>(*this); }
};

template <>
class Future<void> {
    std::exception_ptr exc_ = nullptr;
    bool done_ = false;
    bool cancelled_ = false;

  public:
    bool done() noexcept { return done_; }
    bool cancelled() const noexcept { return cancelled_; }
    void result() {
        if (exc_) {
            std::rethrow_exception(exc_);
        }
        if (done_) {
            return;
        }
        throw std::runtime_error("Future does not have a result");
    }
    std::exception_ptr exception() const noexcept { return exc_; }
    void set_result() { done_ = true; }
    void set_exception(std::exception_ptr exc) noexcept {
        exc_ = exc;
        done_ = true;
    }
    void cancel() noexcept {
        exc_ = std::make_exception_ptr(coconext::Cancelled());
        done_ = true;
        cancelled_ = true;
    }
    FutureAwaiter<void> operator co_await() { return FutureAwaiter<void>(*this); }
};

}  // namespace coconext::futures

#endif  // COCONEXT_FUTURES_HPP

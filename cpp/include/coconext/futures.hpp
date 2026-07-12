#ifndef COCONEXT_FUTURES_HPP
#define COCONEXT_FUTURES_HPP

#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>

#include <coconext/cancelled.hpp>
#include <coconext/event_loop.hpp>
#include <vector>

namespace coconext::futures {

template <typename T>
class Future;

namespace detail {

template <typename T>
class Awaiter {
    friend class Future<T>;
    explicit Awaiter(Future<T>& future) : future_(future) {}

  public:
    bool await_ready() const noexcept { return future_.done(); }
    void await_suspend(std::coroutine_handle<> h) {
        // Implementation for suspending the coroutine until the future is ready.
    }
    T await_resume() { return future_.result(); }

  private:
    Future<T>& future_;
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T>
class Future {
  public:
    // Future is not copyable as it represents a single event.
    Future(Future const&) = delete;
    Future& operator=(Future const&) = delete;

  public:
    bool done() const noexcept { return value_.has_value() || exc_ != nullptr; }
    bool cancelled() const noexcept { return cancelled_; }
    T result() const {
        if (exc_) {
            std::rethrow_exception(exc_);
        }
        if (value_.has_value()) {
            return *value_;
        }
        throw std::runtime_error("Future does not have a result");
    }
    std::exception_ptr exception() const noexcept { return exc_; }

  private:
    void do_callbacks() noexcept {
        for (auto& cb : callbacks_) {
            cb();
        }
    }

  public:
    void set_result(T const& value) noexcept {
        value_ = value;
        do_callbacks();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        exc_ = exc;
        do_callbacks();
    }
    void cancel() noexcept {
        exc_ = std::make_exception_ptr(coconext::Cancelled());
        cancelled_ = true;
        do_callbacks();
    }

  public:
    template <typename F>
    void add_callback(F&& f) {
        callbacks_.emplace_back(std::forward<F>(f));
    }

    detail::Awaiter<T> operator co_await() {
        // TODO
        return detail::Awaiter(*this);
    }

  private:
    std::optional<T> value_ = std::nullopt;
    std::exception_ptr exc_ = nullptr;
    bool cancelled_ = false;
    std::vector<std::function<void()>> callbacks_;
};

template <>
class Future<void> {
  public:
    // Future is not copyable as it represents a single event.
    Future(Future const&) = delete;
    Future& operator=(Future const&) = delete;

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

  private:
    void do_callbacks() noexcept {
        for (auto& cb : callbacks_) {
            cb();
        }
    }

  public:
    void set_result() {
        done_ = true;
        do_callbacks();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        exc_ = exc;
        done_ = true;
        do_callbacks();
    }
    void cancel() noexcept {
        exc_ = std::make_exception_ptr(coconext::Cancelled());
        done_ = true;
        cancelled_ = true;
        do_callbacks();
    }

  public:
    detail::Awaiter<void> operator co_await() { return detail::Awaiter<void>(*this); }

  private:
    std::exception_ptr exc_ = nullptr;
    bool done_ = false;
    bool cancelled_ = false;
    std::vector<std::function<void()>> callbacks_;
};

}  // namespace coconext::futures

#endif  // COCONEXT_FUTURES_HPP

#ifndef COCONEXT_FUTURES_HPP
#define COCONEXT_FUTURES_HPP

#include "coconext/event_deque.hpp"
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>

#include <coconext/cancelled.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/tasks.hpp>

namespace coconext::futures {

template <typename T>
class Future;

namespace detail {

// Futures and their Awaiters are tightly coupled, but since this has to be shared with the
// void overload, it's split out.
template <typename T>
class FutureAwaiter : coconext::event_loop::Event {
    friend class Future<T>;

    explicit FutureAwaiter(Future<T>& future) : future_(future) {}

  public:
    bool await_ready() const noexcept { return future_.done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) {
        task_promise_ = h.promise().get_promise_base();
    }
    T await_resume() {
        if (task_promise_->cancelled()) {
            throw coconext::Cancelled{};
        }
        return future_.result();
    }

  private:
    Future<T>& future_;
    coconext::tasks::detail::PromiseBase* task_promise_;
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
    void schedule_task_resumes() noexcept {
        // TODO Get EventLoop::Handle and steal_extend_back(deque_)
    }

  public:
    void set_result(T const& value) noexcept {
        value_ = value;
        schedule_task_resumes();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        exc_ = exc;
        schedule_task_resumes();
    }
    void cancel() noexcept {
        exc_ = std::make_exception_ptr(coconext::Cancelled{});
        cancelled_ = true;
        schedule_task_resumes();
    }

  public:
    detail::FutureAwaiter<T> operator co_await() {
        // TODO
        return detail::FutureAwaiter<T>(*this);
    }

  private:
    std::optional<T> value_ = std::nullopt;
    std::exception_ptr exc_ = nullptr;
    bool cancelled_ = false;
    coconext::event_loop::Cmarqueue deque_;
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
    void schedule_task_resumes() noexcept {
        // TODO Get EventLoop::Handle and steal_extend_back(deque_)
    }

  public:
    void set_result() {
        done_ = true;
        schedule_task_resumes();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        exc_ = exc;
        done_ = true;
        schedule_task_resumes();
    }
    void cancel() noexcept {
        exc_ = std::make_exception_ptr(coconext::Cancelled());
        done_ = true;
        cancelled_ = true;
        schedule_task_resumes();
    }

  public:
    detail::FutureAwaiter<void> operator co_await() {
        // TODO
        return detail::FutureAwaiter<void>(*this);
    }

  private:
    std::exception_ptr exc_ = nullptr;
    bool done_ = false;
    bool cancelled_ = false;
    coconext::event_loop::Cmarqueue deque_;
};

}  // namespace coconext::futures

#endif  // COCONEXT_FUTURES_HPP

#ifndef COCONEXT_FUTURES_HPP
#define COCONEXT_FUTURES_HPP

#include <coconext/intrusive_deque.hpp>
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>

#include <coconext/cancelled.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/tasks.hpp>
#include <vector>

namespace coconext {

template <typename T>
class Future;

namespace detail {

// Futures and their Awaiters are tightly coupled, but since this has to be shared with the
// void overload, it's split out.
template <typename T>
class FutureAwaiter : coconext::EventLoop::Event {
    friend class Future<T>;

    explicit FutureAwaiter(Future<T>& future) : future_(future) {}

    void event_run() override { parent_.resume(); }

  public:
    bool await_ready() const noexcept { return future_.done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) {
        task_context_ = h.promise().get_context();
        future_.bind_event_loop(task_context_->get_event_loop());
    }
    T await_resume() {
        if (task_context_->cancelled()) {
            throw coconext::Cancelled{};
        }
        return future_.result();
    }

  private:
    Future<T>& future_;
    std::coroutine_handle<> parent_;
    coconext::detail::TaskContext* task_context_;
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T>
class Future {
  public:
    // Future is not copyable as it represents a single event.
    Future(Future const&) = delete;
    Future& operator=(Future const&) = delete;
    // Future is moveable.
    Future(Future&&) = default;
    Future& operator=(Future&&) = default;

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

  public:
    template <typename F>
    void add_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

  private:
    void schedule_task_resumes() noexcept {
        detail::schedule_all_back(*event_loop_, std::move(deque_));
    }
    void do_callbacks() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
    }
    void bind_event_loop(coconext::EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Future is already bound to another EventLoop");
        }
    }

  public:
    void set_result(T const& value) noexcept {
        value_ = value;
        schedule_task_resumes();
        do_callbacks();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        exc_ = exc;
        schedule_task_resumes();
        do_callbacks();
    }
    void cancel() noexcept {
        exc_ = std::make_exception_ptr(coconext::Cancelled{});
        cancelled_ = true;
        schedule_task_resumes();
        do_callbacks();
    }

  public:
    detail::FutureAwaiter<T> operator co_await() { return detail::FutureAwaiter<T>(*this); }

  private:
    std::optional<T> value_ = std::nullopt;
    std::exception_ptr exc_ = nullptr;
    bool cancelled_ = false;
    // The Future starts un-bound to an EventLoop, and is bound when the first task awaits
    // it.
    coconext::EventLoop* event_loop_ = nullptr;
    coconext::detail::IntrusiveDeque<coconext::EventLoop::Event> deque_;
    std::vector<std::function<void()>> callbacks_;
};

template <>
class Future<void> {
  public:
    // Future is not copyable as it represents a single event.
    Future(Future const&) = delete;
    Future& operator=(Future const&) = delete;
    // Future is moveable.
    Future(Future&&) = default;
    Future& operator=(Future&&) = default;

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

  public:
    template <typename F>
    void add_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

  private:
    void schedule_task_resumes() noexcept {
        detail::schedule_all_back(*event_loop_, std::move(deque_));
    }
    void do_callbacks() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
    }
    void bind_event_loop(coconext::EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Future is already bound to another EventLoop");
        }
    }

  public:
    void set_result() {
        done_ = true;
        schedule_task_resumes();
        do_callbacks();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        exc_ = exc;
        done_ = true;
        schedule_task_resumes();
        do_callbacks();
    }
    void cancel() noexcept {
        exc_ = std::make_exception_ptr(coconext::Cancelled{});
        done_ = true;
        cancelled_ = true;
        schedule_task_resumes();
        do_callbacks();
    }

  public:
    detail::FutureAwaiter<void> operator co_await() {
        return detail::FutureAwaiter<void>(*this);
    }

  private:
    std::exception_ptr exc_ = nullptr;
    bool done_ = false;
    bool cancelled_ = false;
    coconext::EventLoop* event_loop_;
    coconext::detail::IntrusiveDeque<coconext::EventLoop::Event> deque_;
    std::vector<std::function<void()>> callbacks_;
};

}  // namespace coconext

#endif  // COCONEXT_FUTURES_HPP

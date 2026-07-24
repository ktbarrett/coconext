#ifndef COCONEXT_FUTURES_HPP
#define COCONEXT_FUTURES_HPP

#include <coconext/intrusive_deque.hpp>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
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
        future_.state_->bind_event_loop(task_context_->get_event_loop());
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

template <typename T>
struct FutureState {
    std::optional<T> value_ = std::nullopt;
    std::exception_ptr exc_ = nullptr;
    bool cancelled_ = false;
    // The Future starts un-bound to an EventLoop, and is bound when the first task
    // awaits it.
    coconext::EventLoop* event_loop_ = nullptr;
    coconext::detail::IntrusiveDeque<coconext::EventLoop::Event> deque_;
    std::vector<std::function<void()>> callbacks_;

    void schedule_task_resumes() noexcept {
        detail::schedule_all_back(*event_loop_, std::move(deque_));
    }
    void do_callbacks() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
    }

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

    void bind_event_loop(coconext::EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Future is already bound to another EventLoop");
        }
    }
};

template <>
struct FutureState<void> {
    std::exception_ptr exc_ = nullptr;
    bool done_ = false;
    bool cancelled_ = false;
    coconext::EventLoop* event_loop_ = nullptr;
    coconext::detail::IntrusiveDeque<coconext::EventLoop::Event> deque_;
    std::vector<std::function<void()>> callbacks_;

    void schedule_task_resumes() noexcept {
        detail::schedule_all_back(*event_loop_, std::move(deque_));
    }
    void do_callbacks() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
    }

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

    void bind_event_loop(coconext::EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Future is already bound to another EventLoop");
        }
    }
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T>
class Future {
  public:
    bool done() const noexcept {
        return state_->value_.has_value() || state_->exc_ != nullptr;
    }
    bool cancelled() const noexcept { return state_->cancelled_; }
    T result() const {
        if (state_->exc_) {
            std::rethrow_exception(state_->exc_);
        }
        if (state_->value_.has_value()) {
            return *(state_->value_);
        }
        throw std::runtime_error("Future does not have a result");
    }
    std::exception_ptr exception() const noexcept { return state_->exc_; }

    template <typename F>
    void add_callback(F&& callback) {
        state_->callbacks_.emplace_back(std::forward<F>(callback));
    }

    detail::FutureAwaiter<T> operator co_await() { return detail::FutureAwaiter<T>(*this); }

  private:
    std::shared_ptr<detail::FutureState<T>> state_ =
        std::make_shared<detail::FutureState<T>>();
};

template <>
class Future<void> {
  public:
    bool done() noexcept { return state_->done_; }
    bool cancelled() const noexcept { return state_->cancelled_; }
    void result() {
        if (state_->exc_) {
            std::rethrow_exception(state_->exc_);
        }
        if (state_->done_) {
            return;
        }
        throw std::runtime_error("Future does not have a result");
    }
    std::exception_ptr exception() const noexcept { return state_->exc_; }

    template <typename F>
    void add_callback(F&& callback) {
        state_->callbacks_.emplace_back(std::forward<F>(callback));
    }

    detail::FutureAwaiter<void> operator co_await() {
        return detail::FutureAwaiter<void>(*this);
    }

  private:
    std::shared_ptr<detail::FutureState<void>> state_ =
        std::make_shared<detail::FutureState<void>>();
};

}  // namespace coconext

#endif  // COCONEXT_FUTURES_HPP

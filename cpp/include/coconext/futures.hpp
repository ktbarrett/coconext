#ifndef COCONEXT_FUTURES_HPP
#define COCONEXT_FUTURES_HPP

#include <coconext/intrusive_deque.hpp>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>

#include <coconext/cancelled.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/tasks.hpp>
#include <variant>
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
struct Value {
    T value;
};

template <>
struct Value<void> {};

template <typename T>
class FutureState {
  public:
    bool done() const noexcept { return !std::holds_alternative<std::monostate>(result_); }
    bool cancelled() const noexcept { return std::holds_alternative<Cancelled>(result_); }
    T result() const {
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        if (std::holds_alternative<Value<T>>(result_)) {
            return std::get<Value<T>>(result_).value;
        }
        throw std::runtime_error("Future does not have a result");
    }
    std::exception_ptr exception() const noexcept {
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            return std::get<std::exception_ptr>(result_);
        }
        return nullptr;
    }

    template <typename F>
    void add_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

    void schedule_task_resumes() noexcept {
        detail::schedule_all_back(*event_loop_, std::move(deque_));
    }
    void do_callbacks() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
    }

    void set_result(T const& value) noexcept {
        result_ = Value<T>{value};
        schedule_task_resumes();
        do_callbacks();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        result_ = exc;
        schedule_task_resumes();
        do_callbacks();
    }
    void cancel() noexcept {
        result_ = Cancelled{};
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

  private:
    std::variant<std::monostate, Value<T>, std::exception_ptr, Cancelled> result_;
    // The Future starts un-bound to an EventLoop, and is bound when the first task
    // awaits it.
    coconext::EventLoop* event_loop_ = nullptr;
    coconext::detail::IntrusiveDeque<coconext::EventLoop::Event> deque_;
    std::vector<std::function<void()>> callbacks_;
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T>
class Future {
  public:
    bool done() const noexcept { return state_->done(); }
    bool cancelled() const noexcept { return state_->cancelled(); }
    T result() const { return state_->result(); }
    std::exception_ptr exception() const noexcept { return state_->exception(); }

    template <typename F>
    void add_callback(F&& callback) {
        state_->add_callback(std::forward<F>(callback));
    }

    detail::FutureAwaiter<T> operator co_await() { return detail::FutureAwaiter<T>(*this); }

  private:
    std::shared_ptr<detail::FutureState<T>> state_ =
        std::make_shared<detail::FutureState<T>>();
};

}  // namespace coconext

#endif  // COCONEXT_FUTURES_HPP

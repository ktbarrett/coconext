#ifndef COCONEXT_FUTURES_HPP
#define COCONEXT_FUTURES_HPP

#include <coconext/intrusive_deque.hpp>
#include <coroutine>
#include <exception>
#include <functional>
#include <stdexcept>

#include <coconext/cancelled.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/task.hpp>
#include <variant>
#include <vector>

namespace coconext {

namespace detail {

template <typename T>
class FutureStateBase {
  public:
    bool done() const noexcept { return !std::holds_alternative<std::monostate>(result_); }
    bool cancelled() const noexcept { return std::holds_alternative<Cancelled>(result_); }
    T result() const {
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        if (std::holds_alternative<detail::ResultValue<T>>(result_)) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::get<detail::ResultValue<T>>(result_).value;
            }
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

    void inc_ref() noexcept { ref_count_++; }
    void dec_ref() noexcept {
        ref_count_--;
        if (ref_count_ == 0) {
            cancel();
            delete this;
        }
    }

  protected:
    void set_result(detail::ResultValue<T> value) noexcept {
        result_ = std::move(value);
        schedule_task_resumes();
        do_callbacks();
    }

  private:
    coconext::detail::IntrusiveDeque<coconext::EventLoop::Event> deque_;
    std::vector<std::function<void()>> callbacks_;
    std::variant<std::monostate, detail::ResultValue<T>, std::exception_ptr, Cancelled>
        result_;
    // The Future starts un-bound to an EventLoop, and is bound when the first task
    // awaits it.
    coconext::EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
};

template <typename T>
class FutureState : public FutureStateBase<T> {
    using Base = FutureStateBase<T>;

  public:
    void set_result(T&& value) noexcept {
        Base::set_result(detail::ResultValue<T>{std::move(value)});
    }
    void set_result(T const& value) noexcept {
        Base::set_result(detail::ResultValue<T>{value});
    }

  private:
    // This makes set_result private for subclasses.
    using Base::set_result;
};

template <>
class FutureState<void> : public FutureStateBase<void> {
    using Base = FutureStateBase<void>;

  public:
    void set_void() noexcept { Base::set_result(detail::ResultValue<void>{}); }

  private:
    // This makes set_result private for subclasses.
    using Base::set_result;
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T>
class Future {
  public:
    Future() : state_(new detail::FutureState<T>{}) {}
    ~Future() { state_->dec_ref(); }
    Future(Future const& other) : state_(other.state_) { state_->inc_ref(); }
    Future(Future&& other) noexcept : state_(other.state_) { other.state_ = nullptr; }
    Future& operator=(Future const& other) {
        if (this != &other) {
            state_->dec_ref();
            state_ = other.state_;
            state_->inc_ref();
        }
        return *this;
    }
    Future& operator=(Future&& other) noexcept {
        if (this != &other) {
            state_->dec_ref();
            state_ = other.state_;
            other.state_ = nullptr;
        }
        return *this;
    }
    T result() const { return state_->result(); }

    bool done() const noexcept { return state_->done(); }
    bool cancelled() const noexcept { return state_->cancelled(); }
    std::exception_ptr exception() const noexcept { return state_->exception(); }

    template <typename F>
    void add_callback(F&& callback) {
        state_->add_callback(std::forward<F>(callback));
    }

    class Awaiter : coconext::EventLoop::Event {
      public:
        bool await_ready() const noexcept { return future_.done(); }
        template <typename PromiseType>
        void await_suspend(std::coroutine_handle<PromiseType> h) {
            task_ = h.promise().get_task();
            future_.state_->bind_event_loop(task_->get_event_loop());
        }
        T await_resume() {
            if (task_->cancelled()) {
                throw coconext::Cancelled{};
            }
            return future_.result();
        }

      private:
        explicit Awaiter(Future<T>& future) : future_(future) {}
        void event_run() override { parent_.resume(); }

        Future<T>& future_;
        std::coroutine_handle<> parent_;
        coconext::Task<>* task_;
    };

    auto operator co_await() { return Awaiter(*this); }

  protected:
    detail::FutureState<T>* state_;
};

}  // namespace coconext

#endif  // COCONEXT_FUTURES_HPP

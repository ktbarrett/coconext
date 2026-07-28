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
class FutureState;

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

    void fire() noexcept {
        event_loop_->acquire().schedule_all_back(std::move(deque_));
        for (auto& callback : callbacks_) {
            callback();
        }
    }

    void set_exception(std::exception_ptr exc) noexcept {
        result_ = exc;
        fire();
    }
    void cancel() noexcept {
        result_ = Cancelled{};
        fire();
    }

    void bind_event_loop(detail::EventLoop* loop) {
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

  private:
    void set_result(detail::ResultValue<T> value) noexcept {
        result_ = std::move(value);
        fire();
    }
    friend class FutureState<T>;

    detail::IntrusiveDeque<detail::Event> deque_;
    std::vector<std::function<void()>> callbacks_;
    std::variant<std::monostate, detail::ResultValue<T>, std::exception_ptr, Cancelled>
        result_;
    // The Future starts un-bound to an EventLoop, and is bound when the first task
    // awaits it.
    detail::EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
};

template <typename T>
class FutureState : public FutureStateBase<T> {
  public:
    void set_result(T&& value) noexcept {
        set_result(detail::ResultValue<T>{std::move(value)});
    }
    void set_result(T const& value) noexcept { set_result(detail::ResultValue<T>{value}); }
};

template <>
class FutureState<void> : public FutureStateBase<void> {
  public:
    void set_void() noexcept { set_result(detail::ResultValue<void>{}); }
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T>
class Future {
  public:
    Future() : handle_(new detail::FutureState<T>{}) {}
    ~Future() { handle_->dec_ref(); }
    Future(Future const& other) : handle_(other.handle_) { handle_->inc_ref(); }
    Future(Future&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Future& operator=(Future const& other) {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }
    Future& operator=(Future&& other) noexcept {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    T result() const { return handle_->result(); }

    bool done() const noexcept { return handle_->done(); }
    bool cancelled() const noexcept { return handle_->cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }

    template <typename F>
    void add_callback(F&& callback) {
        handle_->add_callback(std::forward<F>(callback));
    }

    class Awaiter : detail::Event {
      public:
        bool await_ready() const noexcept { return future_.done(); }
        template <typename PromiseType>
        void await_suspend(std::coroutine_handle<PromiseType> h) {
            task_ = h.promise().get_task();
            future_.handle_->bind_event_loop(task_->get_event_loop());
        }
        T await_resume() {
            if (task_->cancelled()) {
                throw coconext::Cancelled{};
            }
            return future_.result();
        }

      private:
        explicit Awaiter(Future<T>& future) : future_(future) {}
        void event_run() override {
            detail::current_task_ = task_;
            parent_.resume();
        }

        Future<T>& future_;
        std::coroutine_handle<> parent_;
        detail::TaskState<>* task_;
    };

    auto operator co_await() { return Awaiter(*this); }

  protected:
    detail::FutureState<T>* handle_;
};

}  // namespace coconext

#endif  // COCONEXT_FUTURES_HPP

#ifndef COCONEXT_FUTURES_HPP
#define COCONEXT_FUTURES_HPP

#include <coconext/intrusive_deque.hpp>
#include <coroutine>
#include <exception>
#include <functional>
#include <stdexcept>

#include <coconext/event_loop.hpp>
#include <coconext/future.hpp>
#include <coconext/outcome.hpp>
#include <coconext/task.hpp>
#include <type_traits>
#include <variant>
#include <vector>

namespace coconext {

namespace detail {

template <typename T>
class FutureState;

template <typename T>
class FutureStateBase {
    friend class FutureState<T>;

  public:
    bool done() const noexcept { return !std::holds_alternative<std::monostate>(result_); }
    bool cancelled() const noexcept { return std::holds_alternative<Cancelled>(result_); }
    T result() const {
        if (std::holds_alternative<Exception>(result_)) {
            std::rethrow_exception(std::get<Exception>(result_).exception);
        }
        if (std::holds_alternative<Result<T>>(result_)) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::get<Result<T>>(result_).value;
            }
        }
        throw std::runtime_error("Future does not have a result");
    }
    std::exception_ptr exception() const noexcept {
        if (std::holds_alternative<Exception>(result_)) {
            return std::get<Exception>(result_).exception;
        }
        return nullptr;
    }

    template <typename F>
    void add_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

    void set_exception(std::exception_ptr exc) noexcept {
        result_ = Exception{exc};
        on_done();
    }
    void cancel() noexcept {
        result_ = Cancelled{};
        on_done();
    }
    void set_result(Result<T> value) noexcept {
        result_ = std::move(value);
        on_done();
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
    void on_done() noexcept {
        Future<T> future =
            Future<T>::from_state(static_cast<detail::FutureState<T>*>(this));
        for (auto& callback : callbacks_) {
            callback(future);
        }
        event_loop_->acquire().schedule_all_back(std::move(deque_));
    }

    void bind_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Future is already bound to another EventLoop");
        }
    }

    IntrusiveDeque<Event> deque_;
    std::vector<std::function<void(Future<T>&)>> callbacks_;
    std::variant<std::monostate, Result<T>, Exception, Cancelled> result_;
    // The Future starts un-bound to an EventLoop, and is bound when the first task
    // awaits it.
    EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
};

template <typename T>
class FutureState : public FutureStateBase<T> {
  public:
    void set_result(T&& value) noexcept { set_result(Result<T>{std::move(value)}); }
    void set_result(T const& value) noexcept { set_result(Result<T>{value}); }
};

template <>
class FutureState<void> : public FutureStateBase<void> {
  public:
    void set_void() noexcept { set_result(Result<void>{}); }
};

template <typename T>
class FutureAwaiter : Event {
    friend class Future<T>;

  public:
    bool await_ready() const noexcept { return future_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) {
        parent_ = h;
        parent_task_ = h.promise().get_task();
        parent_task_->set_pending(this);
        future_->bind_event_loop(parent_task_->get_event_loop());
    }
    T await_resume() {
        if (parent_task_->cancelled()) {
            throw coconext::Cancelled{};
        }
        return future_->result();
    }

  private:
    explicit FutureAwaiter(FutureState<T>* future) : future_(future) {}

    void event_run() override { parent_.resume(); }

    FutureState<T>* future_;
    std::coroutine_handle<> parent_;
    TaskState<>* parent_task_;
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T, typename StateT = detail::FutureState<T>>
    requires std::is_base_of_v<detail::FutureStateBase<T>, StateT>
class Future {
  public:
    Future() : handle_(new StateT{}) {}
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

    auto operator co_await() { return detail::FutureAwaiter(handle_); }

    static Future from_state(StateT* state) { return Future{state}; }
    StateT* get_state() const noexcept { return handle_; }

  private:
    explicit Future(StateT* state) : handle_(state) { handle_->inc_ref(); }

    StateT* handle_;
};

}  // namespace coconext

#endif  // COCONEXT_FUTURES_HPP

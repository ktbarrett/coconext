#ifndef COCONEXT_TASK_HPP
#define COCONEXT_TASK_HPP

#include "intrusive_deque.hpp"
#include <coroutine>
#include <exception>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <coconext/event_loop.hpp>
#include <coconext/outcome.hpp>
#include <vector>

namespace coconext {

namespace detail {

class Erased {};

}  // namespace detail

template <typename T = detail::Erased>
class Task;

template <typename T>
class Coro;

template <typename T, typename StateT>
class Future;

class TaskManager;

namespace detail {

class TaskManagerState;

class TaskManagerSharedState : public IntrusiveDequeNode {
    friend class TaskManagerState;

  private:
    void remove_child() { deque_remove(); }
    using IntrusiveDequeNode::deque_remove;
};

template <typename T = Erased>
class TaskState;

template <typename T>
class FutureState;

template <typename T>
class FutureAwaiter;

template <>
class TaskState<Erased> : public TaskManagerSharedState {
    template <typename>
    friend class FutureAwaiter;

  public:
    virtual void inc_ref() noexcept = 0;
    virtual void dec_ref() noexcept = 0;

    virtual TaskState<>* get_task() noexcept = 0;
    virtual EventLoop* get_event_loop() noexcept = 0;

    virtual bool unstarted() const noexcept = 0;
    virtual bool cancelled() const noexcept = 0;
    virtual bool done() const noexcept = 0;
    virtual std::exception_ptr exception() const noexcept = 0;

    virtual void start_soon(TaskManagerState* loop) = 0;
    virtual void cancel() noexcept = 0;
    virtual void uncancel() = 0;

  private:
    virtual void set_pending(Event* future_awaiter) = 0;
};

template <typename T>
class TaskStateBase : public TaskState<Erased> {
    friend class TaskState<T>;

    struct Scheduled : detail::Event {
        explicit Scheduled(TaskStateBase<T>& task) : task_(&task) {}

        void event_run() override {
            auto handle = std::coroutine_handle<TaskState<T>>::from_promise(
                *static_cast<TaskState<T>*>(task_)
            );
            handle.resume();
        }

        TaskStateBase<Erased>* task_;
    };

    struct Pending {
        Event* event;
    };

  public:
    Task<T> get_return_object() { return Task<T>{static_cast<TaskState<T>*>(this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() { set_result(Exception{std::current_exception()}); }

    TaskState<>* get_task() noexcept override { return this; }
    EventLoop* get_event_loop() noexcept override { return event_loop_; }

    void add_done_callback(std::function<void(Task<T>&)> callback) {
        if (done()) {
            throw std::runtime_error("Task is already done, cannot add callback");
        }
        callbacks_.push_back(std::move(callback));
    }

    bool unstarted() const noexcept override {
        return std::holds_alternative<std::monostate>(state_);
    }
    bool cancelled() const noexcept override { return cancelled_; }
    bool done() const noexcept override {
        return std::holds_alternative<Result>(state_)
            || std::holds_alternative<Exception>(state_);
    }
    T result() {
        if (!done()) {
            throw std::runtime_error("Task not completed yet.");
        }
        if (std::holds_alternative<Exception>(state_)) {
            std::rethrow_exception(std::get<Exception>(state_).exception);
        }
        if (std::holds_alternative<Result<T>>(state_)) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::get<Result<T>>(state_).value;
            }
        }
        throw std::runtime_error("Task does not have a result");
    }
    std::exception_ptr exception() const noexcept override {
        if (std::holds_alternative<Exception>(state_)) {
            return std::get<Exception>(state_).exception;
        }
        return nullptr;
    }

    void cancel() noexcept override {
        if (done()) {
            return;
        }
        cancelled_++;
        if (unstarted()) {
            // We will never get the chance to check cancelled/uncancelled count, so we
            // force it done now.
            state_ = Exception{std::make_exception_ptr(Cancelled{})};
            return;
        } else if (std::holds_alternative<Pending>(state_)) {
            auto& pending = std::get<Pending>(state_);
            pending.event->event_unschedule();
            // Must return reference to value in variant, otherwise we have a dangling
            // pointer.
            event_loop_->acquire().schedule_back(&std::get<Scheduled>(state_));
        }
        // Not done, unstarted, or pending? Already scheduled, we will steal its place in
        // line.
    }
    void uncancel() override {
        if (done()) {
            return;
        }
        if (cancelled_ == 0) {
            throw std::runtime_error("TaskManager is not cancelled");
        }
        cancelled_--;
    }

    void start_soon(detail::TaskManagerState* tm) override;

    void inc_ref() noexcept override { ref_count_++; }
    void dec_ref() noexcept override {
        ref_count_--;
        if (ref_count_ == 0) {
            auto handle = std::coroutine_handle<TaskState<T>>::from_promise(
                *static_cast<TaskState<T>*>(this)
            );
            // This is basically a "delete this", so no more code should follow this line.
            handle.destroy();
        }
    }

    void bind_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Task is already bound to another EventLoop");
        }
    }

    ~TaskStateBase();

    class DoneFutureState : public FutureState<void> {
        template <typename PromiseType>
        void await_suspend(std::coroutine_handle<PromiseType> h) {
            FutureState<void>::await_suspend(h);
            task_->bind_event_loop(h.promise().get_task()->get_event_loop());
        }

      private:
        TaskStateBase<T>* task_;
    };

    using DoneFuture = Future<void, DoneFutureState>;

    DoneFuture wait_complete() const noexcept;

  private:
    void set_result(Result<T>&& value) {
        state_ = std::move(value);
        // TODO handle cancelled_ > 0
        on_done();
    }

    void on_done();

    void set_pending(Event* future_awaiter) override { state_ = Pending{future_awaiter}; }

    std::variant<std::monostate, Scheduled, Pending, Result<T>, Exception> state_;
    std::vector<std::function<void(Task<T>&)>> callbacks_;
    mutable DoneFutureState* wait_complete_future_ = nullptr;
    detail::TaskManagerState* task_manager_ = nullptr;
    EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

template <typename T>
class TaskState : public TaskStateBase<T> {
  public:
    void return_value(T value) { set_result(Result<T>{std::move(value)}); }
};

template <>
class TaskState<void> : public TaskStateBase<void> {
  public:
    void return_void() { set_result(Result<void>{}); }
};

template <typename T>
Task<T> wrap_impl(Coro<T>&& coro) {
    co_return co_await std::move(coro);
}

}  // namespace detail

template <typename T>
class Task {
  public:
    ~Task() {
        if (handle_) {
            handle_->dec_ref();
        }
    }

    Task(Task const& other) : handle_(other.handle_) { handle_->inc_ref(); }
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Task(Coro<T> coro) : Task(std::move(detail::wrap_impl(std::move(coro)))) {}

    Task& operator=(Task const& other) {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    void add_done_callback(std::function<void(Task<T>&)> callback) {
        handle_->add_done_callback(std::move(callback));
    }

    bool unstarted() const noexcept { return handle_->unstarted(); }
    bool done() const noexcept { return handle_->done(); }
    bool cancelled() const noexcept { return handle_->cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }
    T result() { return handle_->result(); }

    void start_soon(TaskManager& loop);

    void cancel() noexcept { handle_->cancel(); }
    void uncancel() { handle_->uncancel(); }

    detail::TaskState<T>* get_state() const noexcept { return handle_; }
    static Task<T> from_state(detail::TaskState<T>* state) { return Task<T>{state}; }

    typename detail::TaskState<T>::DoneFuture wait_complete() const noexcept;

    Coro<T> wait_result() {
        co_await wait_complete();
        co_return result();
    }

  private:
    explicit Task(detail::TaskState<T>* s) : handle_(s) { handle_->inc_ref(); }

    detail::TaskState<T>* handle_;
};

}  // namespace coconext

#endif  // COCONEXT_TASK_HPP

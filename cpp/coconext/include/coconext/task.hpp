#ifndef COCONEXT_TASK_HPP
#define COCONEXT_TASK_HPP

#include <coconext/awaitable.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/intrusive_deque.hpp>
#include <coconext/not_null.hpp>
#include <coconext/outcome.hpp>

#include <coroutine>
#include <cstddef>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace coconext {

namespace detail {

class Erased {};

template <typename T>
class RunTaskManager;

template <typename T>
class CoroStateBase;

}  // namespace detail

template <typename T = detail::Erased>
class TaskState;

template <typename T>
class Task;

template <typename T>
class Coro;

class TaskManager;

class TaskContext;

namespace detail {

inline thread_local TaskState<>* current_task = nullptr;

template <typename AwaitableStateT>
class AwaitableAwaiter : private Event {
  public:
    using value_type = typename AwaitableStateT::value_type;
    using coconext_awaiter = void;

    [[nodiscard]] explicit AwaitableAwaiter(not_null<AwaitableStateT*> awaitable)
        : awaitable_(awaitable) {
        awaitable_->inc_ref();
    }

    AwaitableAwaiter(AwaitableAwaiter const&) = delete;
    AwaitableAwaiter& operator=(AwaitableAwaiter const&) = delete;

    ~AwaitableAwaiter() {
        event_unschedule();
        awaitable_->dec_ref();
    }

    [[nodiscard]] bool await_ready() const noexcept { return awaitable_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h);
    [[nodiscard]] value_type await_resume();

  private:
    void event_run() noexcept override;

    not_null<AwaitableStateT*> awaitable_;
    std::coroutine_handle<> parent_ = nullptr;
    TaskState<>* task_ = nullptr;
};

}  // namespace detail

namespace detail {

template <typename T>
class TaskStateBase;

}

template <>
class TaskState<detail::Erased> : public detail::IntrusiveDequeNode {
    template <typename>
    friend class Task;
    friend class TaskManager;
    template <typename>
    friend class detail::AwaitableAwaiter;
    template <typename>
    friend class Coro;
    template <typename>
    friend class detail::TaskStateBase;
    friend class TaskContext;
    friend TaskContext lookup_context();

    struct Scheduled : detail::Event {
        [[nodiscard]] explicit Scheduled(not_null<TaskState<>*> task) : task_(task) {}

        void event_run() noexcept override {
            // on_resume() re-emplaces state_, which destroys *this. Copy the fields
            // we need onto the stack before that happens.
            auto task = task_;
            // If this resume() ends the task, return_void/return_value will be called, then
            // on_done, which will deregister the Task from the TaskManager, which may be
            // the final dec_ref and destroy the promise. That's fine and all until you
            // realize that final_suspend is called after that: use-after-free. There is no
            // reasonable time from within the coroutine to decide it's safe to release
            // itself, so we just have to play the refcount dance so this routine is in
            // charge of its destruction.
            task->inc_ref();
            auto& current_task = detail::current_task;
            auto previous_task = current_task;
            current_task = task;
            task->on_resume();
            task->handle_.resume();
            current_task = previous_task;
            task->dec_ref();
        }

        not_null<TaskState<>*> task_;
    };

    struct Pending {
        [[nodiscard]] explicit Pending(not_null<detail::Event*> event) : event(event) {}

        not_null<detail::Event*> event;
    };

    struct Running {};
    struct Succeeded {};
    struct Failed {
        std::exception_ptr exception;
    };
    struct Cancelled {};
    using Outcome = std::variant<Succeeded, Failed, Cancelled>;
    struct Done {
        Outcome outcome;
    };

  public:
    using value_type = detail::Erased;

    [[nodiscard]] bool started() const noexcept {
        return !std::holds_alternative<std::monostate>(state_);
    }
    [[nodiscard]] bool running() const noexcept {
        return std::holds_alternative<Running>(state_);
    }
    [[nodiscard]] bool cancelling() const noexcept { return cancellation_requested_; }
    [[nodiscard]] bool cancelled() const noexcept {
        auto done_state = std::get_if<Done>(&state_);
        return done_state != nullptr
            && std::holds_alternative<Cancelled>(done_state->outcome);
    }
    [[nodiscard]] bool done() const noexcept {
        return std::holds_alternative<Done>(state_);
    }
    [[nodiscard]] std::exception_ptr exception() const {
        if (!done()) {
            throw std::runtime_error("Not done");
        }
        auto const& outcome = std::get<Done>(state_).outcome;
        if (auto failed = std::get_if<Failed>(&outcome)) {
            return failed->exception;
        }
        if (cancelled()) {
            return std::make_exception_ptr(CancelledError{});
        }
        return nullptr;
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

    void start_soon(TaskContext const& context);

    void cancel() {
        if (done()) {
            return;
        }
        cancellation_requested_ = true;
        if (running()) {
            throw CancelledError{};
        }
        if (!started()) {
            mark_cancelled();
            return;
        } else if (std::holds_alternative<Scheduled>(state_)) {
            std::get<Scheduled>(state_).event_unschedule();
            mark_cancelled();
            return;
        } else if (std::holds_alternative<Pending>(state_)) {
            auto& pending = std::get<Pending>(state_);
            pending.event->event_unschedule();
            event_loop_->acquire().schedule_back(pending.event);
        }
    }

  protected:
    void set_exception(std::exception_ptr exc) noexcept {
        assert(exc);
        on_done(Failed{std::move(exc)});
    }

    void mark_value() noexcept {
        if (cancellation_requested_) {
            mark_cancellation_ignored();
            return;
        }
        on_done(Succeeded{});
    }

    void mark_cancelled() noexcept { on_done(Cancelled{}); }

    void mark_cancellation_ignored() noexcept {
        try {
            throw std::runtime_error("Task ignored cancellation");
        } catch (...) {
            set_exception(std::current_exception());
        }
    }

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            handle_.destroy();
        }
    }

    void uncancel() noexcept { cancellation_requested_ = false; }

    [[nodiscard]] TaskContext get_context() noexcept;

    [[nodiscard]] detail::EventLoop* get_event_loop() const noexcept { return event_loop_; }
    [[nodiscard]] TaskManager* get_task_manager() const noexcept { return task_manager_; }
    [[nodiscard]] TaskManager* get_global_task_manager() const noexcept {
        return global_task_manager_;
    }

    void register_waiter(not_null<detail::Event*> awaiter) noexcept {
        waiters_.push_back(awaiter);
    }

    void on_awaited(TaskContext const& context);

    void on_done(Outcome outcome) noexcept;

    void on_resume() noexcept { state_ = Running{}; }

    void on_awaiting(not_null<detail::Event*> awaiter) {
        if (cancellation_requested_) {
            throw std::runtime_error("Task ignored cancellation");
        }
        state_ = Pending{awaiter};
    }

  private:
    std::variant<std::monostate, Scheduled, Pending, Running, Done> state_;
    detail::IntrusiveDeque<detail::Event> waiters_;
    std::vector<std::function<void()>> callbacks_;
    detail::EventLoop* event_loop_ = nullptr;
    TaskManager* task_manager_ = nullptr;
    TaskManager* global_task_manager_ = nullptr;
    std::coroutine_handle<> handle_;
    size_t ref_count_{0};
    bool cancellation_requested_ = false;
};

namespace detail {

template <typename T>
class TaskStateBase : public TaskState<> {
    friend class TaskState<T>;

  public:
    using value_type = T;

    [[nodiscard]] Task<T> get_return_object() noexcept {
        auto self = static_cast<TaskState<T>*>(this);
        handle_ = std::coroutine_handle<TaskState<T>>::from_promise(*self);
        return Task<T>{self};
    }
    [[nodiscard]] std::suspend_always initial_suspend() noexcept { return {}; }
    [[nodiscard]] std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() noexcept {
        try {
            throw;
        } catch (CancelledError const&) {
            if (this->cancelling()) {
                this->mark_cancelled();
            } else {
                this->set_exception(std::current_exception());
            }
        } catch (...) {
            if (this->cancelling()) {
                this->mark_cancellation_ignored();
            } else {
                this->set_exception(std::current_exception());
            }
        }
    }
};

}  // namespace detail

template <typename T>
class TaskState : public detail::TaskStateBase<T> {
  public:
    template <typename U>
        requires std::is_convertible_v<U, T>
    void return_value(U&& value) noexcept {
        this->value_ = std::forward<U>(value);
        TaskState<>::mark_value();
    }

    [[nodiscard]] T result() const {
        if (!TaskState<>::done()) {
            throw std::runtime_error("Not done");
        }
        if (TaskState<>::cancelled()) {
            throw CancelledError{};
        }
        if (auto exc = TaskState<>::exception()) {
            std::rethrow_exception(exc);
        }
        return *value_;
    }

  private:
    std::optional<T> value_;
};

template <>
class TaskState<void> : public detail::TaskStateBase<void> {
  public:
    void return_void() noexcept { TaskState<>::mark_value(); }

    void result() const {
        if (!TaskState<>::done()) {
            throw std::runtime_error("Not done");
        }
        if (TaskState<>::cancelled()) {
            throw CancelledError{};
        }
        if (auto exc = TaskState<>::exception()) {
            std::rethrow_exception(exc);
        }
    }
};

template <typename T>
class Task {
    friend class TaskManager;
    template <typename>
    friend class detail::RunTaskManager;
    template <typename>
    friend class detail::TaskStateBase;

  public:
    using value_type = T;
    using promise_type = TaskState<T>;

    [[nodiscard]] Task(Task const& other) noexcept : handle_(other.handle_) {
        handle_->inc_ref();
    }
    Task& operator=(Task const& other) noexcept {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }

    ~Task() noexcept { handle_->dec_ref(); }

    template <typename F>
    void add_done_callback(F&& callback) {
        handle_->add_done_callback(std::forward<F>(callback));
    }

    [[nodiscard]] bool started() const noexcept { return handle_->started(); }
    [[nodiscard]] bool done() const noexcept { return handle_->done(); }
    [[nodiscard]] bool cancelling() const noexcept { return handle_->cancelling(); }
    [[nodiscard]] bool cancelled() const noexcept { return handle_->cancelled(); }
    [[nodiscard]] std::exception_ptr exception() const { return handle_->exception(); }
    [[nodiscard]] T result() const { return handle_->result(); }

    void cancel() { handle_->cancel(); }

    [[nodiscard]] not_null<TaskState<T>*> get_state() const noexcept { return handle_; }

    [[nodiscard]] CoconextAwaitable auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<TaskState<T>>(handle_);
    }

  private:
    [[nodiscard]] explicit Task(not_null<TaskState<T>*> state) noexcept : handle_(state) {
        handle_->inc_ref();
    }

    template <CoconextAwaitable A>
        requires std::same_as<await_result_t<A>, T>
    [[nodiscard]] Task(A awaitable) : Task(std::move(wrap_impl(std::move(awaitable)))) {}

    template <CoconextAwaitable A>
        requires std::same_as<await_result_t<A>, T>
    [[nodiscard]] static Task<T> wrap_impl(A awaitable) {
        co_return co_await std::move(awaitable);
    }

    not_null<TaskState<T>*> handle_;
};

class TaskContext final {
    friend TaskContext lookup_context();
    friend class TaskManager;
    friend class TaskState<detail::Erased>;
    template <typename>
    friend class detail::CoroStateBase;
    template <typename>
    friend class detail::RunTaskManager;

  public:
    [[nodiscard]] TaskState<>* get_task() const noexcept { return task_; }
    [[nodiscard]] TaskManager* get_global_task_manager() const noexcept {
        return global_task_manager_;
    }
    [[nodiscard]] not_null<detail::EventLoop*> get_event_loop() const noexcept {
        return event_loop_;
    }

  private:
    TaskContext(
        not_null<detail::EventLoop*> event_loop,
        TaskManager* global_task_manager,
        TaskState<>* task
    ) noexcept
        : event_loop_(event_loop), global_task_manager_(global_task_manager), task_(task) {}

    not_null<detail::EventLoop*> event_loop_;
    TaskManager* global_task_manager_ = nullptr;
    TaskState<>* task_ = nullptr;
};

namespace detail {

class TaskContextAwaiter {
  public:
    using coconext_awaiter = void;

    [[nodiscard]] bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    bool await_suspend(std::coroutine_handle<PromiseType> parent) noexcept {
        context_ = parent.promise().get_context();
        return false;
    }

    [[nodiscard]] TaskContext await_resume() const noexcept { return *context_; }

  private:
    std::optional<TaskContext> context_;
};

}  // namespace detail

[[nodiscard]] inline CoconextAwaitable auto get_context() {
    return detail::TaskContextAwaiter{};
}

[[nodiscard]] inline not_null<TaskState<>*> lookup_task() {
    if (detail::current_task == nullptr) {
        throw std::runtime_error("No current task");
    }
    return detail::current_task;
}

[[nodiscard]] inline TaskContext lookup_context() { return lookup_task()->get_context(); }

inline TaskContext TaskState<>::get_context() noexcept {
    return TaskContext{not_null{event_loop_}, global_task_manager_, this};
}

inline void TaskState<>::start_soon(TaskContext const& context) {
    assert(!started() && "Task is already started");

    auto loop = context.get_event_loop();
    if (event_loop_ == nullptr) {
        event_loop_ = loop;
    } else if (event_loop_ != loop) {
        throw std::runtime_error("Task is already bound to another EventLoop");
    }

    if (global_task_manager_ == nullptr) {
        global_task_manager_ = context.get_global_task_manager();
    }

    state_ = Scheduled{this};
    event_loop_->acquire().schedule_back(&std::get<TaskState<>::Scheduled>(state_));
}

inline void TaskState<>::on_awaited(TaskContext const& context) {
    if (!started()) {
        start_soon(context);
    }
}

template <typename S>
template <typename PromiseType>
void detail::AwaitableAwaiter<S>::await_suspend(std::coroutine_handle<PromiseType> h) {
    parent_ = h;
    auto context = h.promise().get_context();
    // Keep the checked Task pointer local until await registration succeeds.
    auto task = context.get_task();
    awaitable_->on_awaited(context);
    task->on_awaiting(this);
    task_ = task;
    awaitable_->register_waiter(this);
}

template <typename S>
typename detail::AwaitableAwaiter<S>::value_type detail::AwaitableAwaiter<
    S>::await_resume() {
    // task_ is only set if await_suspend ran. If await_ready short-circuited on an
    // already-done awaitable, we skip the cancellation check as there was no possible
    // suspension point where the Task could become cancelled. The one caveat is a
    // self-cancellation, which is handled by throwing in a call to cancel();
    if (task_ != nullptr && task_->cancelling()) {
        throw coconext::CancelledError{};
    }
    return awaitable_->result();
}

template <typename S>
void detail::AwaitableAwaiter<S>::event_run() noexcept {
    assert(parent_ != nullptr);
    assert(task_ != nullptr);
    auto task = not_null{task_};
    task->inc_ref();
    auto& current_task = detail::current_task;
    auto previous_task = current_task;
    current_task = task;
    task->on_resume();
    parent_.resume();
    current_task = previous_task;
    task->dec_ref();
}

}  // namespace coconext

#endif  // COCONEXT_TASK_HPP

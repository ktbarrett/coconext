#ifndef COCONEXT_SCHEDULER_HPP
#define COCONEXT_SCHEDULER_HPP

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

}  // namespace detail

template <typename T>
class AbstractFutureState;

template <typename T>
class FutureState;

template <typename T = detail::Erased>
class TaskState;

template <typename StateT>
class AbstractFuture;

template <typename T>
class Task;

template <typename T>
class Coro;

class TaskManager;

class TaskContext;

[[nodiscard]] TaskContext current_context();

namespace detail {

// inline thread_local means this variable is included in the user's library, and lookups
// are fast (Local-Exec mode).
inline thread_local TaskState<>* current_task = nullptr;

template <typename AwaitableStateT>
class AwaitableAwaiter : private Event {
  public:
    using value_type = typename AwaitableStateT::value_type;

    ~AwaitableAwaiter() { event_unschedule(); }

    [[nodiscard]] bool await_ready() const noexcept { return awaitable_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h);
    [[nodiscard]] value_type await_resume();

  private:
    template <typename>
    friend class ::coconext::AbstractFuture;
    template <typename>
    friend class ::coconext::Task;

    [[nodiscard]] explicit AwaitableAwaiter(not_null<AwaitableStateT*> awaitable)
        : awaitable_(awaitable) {}

    void event_run() noexcept override;

    not_null<AwaitableStateT*> awaitable_;
    std::coroutine_handle<> parent_ = nullptr;
    TaskState<>* task_ = nullptr;
};

}  // namespace detail

template <typename T>
class AbstractFutureState {
    template <typename>
    friend class AbstractFuture;
    template <typename>
    friend class detail::AwaitableAwaiter;

  public:
    using value_type = T;

    // States are deleted through AbstractFutureState<T> when the final AbstractFuture
    // reference is released. Derived states commonly prime an external trigger in
    // their constructor and unprime it in their destructor when !done().
    virtual ~AbstractFutureState() = default;

    [[nodiscard]] bool done() const noexcept {
        return !std::holds_alternative<std::monostate>(result_);
    }

    [[nodiscard]] std::exception_ptr exception() const {
        if (!done()) {
            throw std::runtime_error("Not done");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            return std::get<std::exception_ptr>(result_);
        }
        return nullptr;
    }

    [[nodiscard]] T result() const {
        if (!done()) {
            throw std::runtime_error("Not done");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        if constexpr (!std::is_void_v<T>) {
            return std::get<detail::Value<T>>(result_).value;
        }
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

  protected:
    template <typename U>
        requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
    void set_result(U&& value) noexcept {
        result_ = detail::Value<T>{std::forward<U>(value)};
        on_done();
    }

    void set_void() noexcept
        requires std::is_void_v<T>
    {
        result_ = detail::Value<void>{};
        on_done();
    }

    void set_exception(std::exception_ptr exc) {
        if (!exc) {
            throw std::invalid_argument("exc must not be null");
        }
        result_ = exc;
        on_done();
    }

  private:
    void on_awaited(not_null<TaskState<>*> task);

    void register_waiter(not_null<detail::Event*> awaiter) noexcept {
        waiters_.push_back(awaiter);
    }

    void on_done() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
        if (!waiters_.empty()) {
            assert(event_loop_ != nullptr);
            event_loop_->acquire().schedule_all_back(std::move(waiters_));
        }
    }

    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    detail::IntrusiveDeque<detail::Event> waiters_;
    std::vector<std::function<void()>> callbacks_;
    std::variant<std::monostate, std::exception_ptr, detail::Value<T>> result_;
    detail::EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
};

// Single-shot, multiple-consumer awaitable object.
template <typename StateT>
class AbstractFuture {
    static_assert(
        std::is_base_of_v<AbstractFutureState<typename StateT::value_type>, StateT>,
        "AbstractFuture's StateT must derive from AbstractFutureState<T>"
    );

  public:
    using value_type = typename StateT::value_type;

    [[nodiscard]] AbstractFuture() noexcept : handle_(new StateT{}) { handle_->inc_ref(); }
    [[nodiscard]] AbstractFuture(AbstractFuture const& other) noexcept
        : handle_(other.handle_) {
        handle_->inc_ref();
    }

    AbstractFuture& operator=(AbstractFuture const& other) noexcept {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }

    ~AbstractFuture() noexcept { handle_->dec_ref(); }

    [[nodiscard]] bool done() const noexcept { return handle_->done(); }
    [[nodiscard]] value_type result() const { return handle_->result(); }
    [[nodiscard]] std::exception_ptr exception() const { return handle_->exception(); }

    template <typename F>
    void add_done_callback(F&& callback) {
        handle_->add_done_callback(std::forward<F>(callback));
    }

    [[nodiscard]] auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<StateT>(handle_);
    }

    [[nodiscard]] not_null<StateT*> get_state() const noexcept { return handle_; }
    [[nodiscard]] explicit AbstractFuture(not_null<StateT*> state) noexcept
        : handle_(state) {
        handle_->inc_ref();
    }

  private:
    not_null<StateT*> handle_;
};

// State for the concrete Future<T>. Unlike trigger-backed AbstractFutureState
// subclasses, this exposes its completion API publicly for ad-hoc inter-Task
// communication.
template <typename T>
class FutureState : public AbstractFutureState<T> {
  public:
    using AbstractFutureState<T>::set_exception;
    using AbstractFutureState<T>::set_result;
    using AbstractFutureState<T>::set_void;
};

template <typename T>
class Future : public AbstractFuture<FutureState<T>> {
  public:
    template <typename U>
        requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
    void set_result(U&& value) noexcept {
        this->get_state()->set_result(std::forward<U>(value));
    }

    void set_void() noexcept
        requires std::is_void_v<T>
    {
        this->get_state()->set_void();
    }

    void set_exception(std::exception_ptr exc) { this->get_state()->set_exception(exc); }
};

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
    template <typename>
    friend class AbstractFutureState;
    friend class TaskContext;

    struct Scheduled : detail::Event {
        [[nodiscard]] explicit Scheduled(not_null<TaskState<>*> task) : task_(task) {}

        void event_run() noexcept override {
            // on_resume() re-emplaces state_, which destroys *this. Copy the fields
            // we need onto the stack before that happens.
            auto task = task_;
            task->inc_ref();
            auto previous_task = detail::current_task;
            task->on_resume();
            task->handle_.resume();
            detail::current_task = previous_task;
            task->dec_ref();
        }

        not_null<TaskState<>*> task_;
    };

    struct Pending {
        [[nodiscard]] explicit Pending(not_null<detail::Event*> event) : event(event) {}

        not_null<detail::Event*> event;
    };

    struct Running {};
    struct CancelledOutcome {};

  public:
    using value_type = detail::Erased;

    [[nodiscard]] bool started() const noexcept { return started_; }
    [[nodiscard]] bool running() const noexcept {
        return std::holds_alternative<Running>(state_);
    }
    [[nodiscard]] size_t cancelling() const noexcept { return cancellation_requests_; }
    [[nodiscard]] bool cancelled() const noexcept {
        return std::holds_alternative<CancelledOutcome>(result_);
    }

    [[nodiscard]] bool done() const noexcept {
        return !std::holds_alternative<std::monostate>(result_);
    }

    [[nodiscard]] std::exception_ptr exception() const {
        if (!done()) {
            throw std::runtime_error("Not done");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            return std::get<std::exception_ptr>(result_);
        }
        if (cancelled()) {
            return std::make_exception_ptr(Cancelled{});
        }
        return nullptr;
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

    void cancel() {
        if (done()) {
            return;
        }
        cancellation_requests_++;
        if (running()) {
            throw Cancelled{};
        }
        if (!started()) {
            mark_cancelled();
            return;
        } else if (std::holds_alternative<Scheduled>(state_)) {
            inc_ref();
            std::get<Scheduled>(state_).event_unschedule();
            mark_cancelled();
            dec_ref();
            return;
        } else if (std::holds_alternative<Pending>(state_)) {
            auto& pending = std::get<Pending>(state_);
            pending.event->event_unschedule();
            event_loop_->acquire().schedule_back(pending.event);
        }
    }

    void uncancel() {
        if (done()) {
            return;
        }
        if (cancellation_requests_ == 0) {
            throw std::runtime_error("Task is not cancelled");
        }
        cancellation_requests_--;
    }

  protected:
    void set_exception(std::exception_ptr exc) noexcept {
        assert(exc);
        result_ = exc;
        on_done();
    }

    void mark_value() noexcept {
        if (cancellation_requests_ != 0) {
            mark_cancellation_ignored();
            return;
        }
        result_ = detail::Value<detail::Erased>{};
        on_done();
    }

    void mark_cancelled() noexcept {
        result_ = CancelledOutcome{};
        on_done();
    }

    void mark_cancellation_ignored() noexcept {
        try {
            throw_cancellation_ignored();
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

    [[nodiscard]] size_t take_cancellations() noexcept {
        return std::exchange(cancellation_requests_, 0);
    }

    void restore_cancellations(size_t count) noexcept { cancellation_requests_ += count; }

    [[noreturn]] static void throw_cancellation_ignored() {
        throw std::runtime_error("Task ignored cancellation without calling uncancel()");
    }

    [[nodiscard]] not_null<TaskState<>*> get_task() noexcept { return this; }

    [[nodiscard]] detail::EventLoop* get_event_loop() const noexcept { return event_loop_; }
    [[nodiscard]] TaskManager* get_task_manager() const noexcept { return task_manager_; }
    [[nodiscard]] TaskManager* get_global_task_manager() const noexcept {
        return global_task_manager_;
    }

    void start_soon(
        not_null<detail::EventLoop*> loop, not_null<TaskManager*> task_manager
    ) {
        assert(!started() && "Task is already started");

        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Task is already bound to another EventLoop");
        }

        if (global_task_manager_ == nullptr) {
            global_task_manager_ = task_manager;
        }

        // Keep the coroutine frame alive independently of Task handles and its manager
        // until the coroutine reaches completion.
        inc_ref();
        scheduler_owned_ = true;
        started_ = true;
        state_ = Scheduled{this};
        event_loop_->acquire().schedule_back(&std::get<TaskState<>::Scheduled>(state_));
    }

    void register_waiter(not_null<detail::Event*> awaiter) noexcept {
        waiters_.push_back(awaiter);
    }

    void on_awaited(not_null<TaskState<>*> awaiter) {
        if (!started()) {
            throw std::runtime_error("Cannot await an unstarted Task");
        }
        if (event_loop_ != awaiter->get_event_loop()) {
            throw std::runtime_error("Task is already bound to another EventLoop");
        }
    }

    void on_done() noexcept;

    void on_resume() noexcept {
        detail::current_task = this;
        state_ = Running{};
    }

    void on_awaiting(not_null<detail::Event*> awaiter) {
        if (cancellation_requests_ != 0) {
            throw_cancellation_ignored();
        }
        state_ = Pending{awaiter};
    }

  private:
    std::variant<std::monostate, Scheduled, Pending, Running> state_;
    std::variant<
        std::monostate,
        std::exception_ptr,
        detail::Value<detail::Erased>,
        CancelledOutcome>
        result_;
    detail::IntrusiveDeque<detail::Event> waiters_;
    std::vector<std::function<void()>> callbacks_;
    detail::EventLoop* event_loop_ = nullptr;
    TaskManager* task_manager_ = nullptr;
    void (*internal_done_callback_)(not_null<TaskState<>*>) noexcept = nullptr;
    TaskManager* global_task_manager_ = nullptr;
    std::coroutine_handle<> handle_;
    size_t ref_count_{0};
    size_t cancellation_requests_{0};
    bool started_ = false;
    bool scheduler_owned_ = false;
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
        } catch (Cancelled const&) {
            if (this->cancelling() != 0) {
                this->mark_cancelled();
            } else {
                this->set_exception(std::current_exception());
            }
        } catch (...) {
            if (this->cancelling() != 0) {
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
            throw Cancelled{};
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
            throw Cancelled{};
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
    [[nodiscard]] size_t cancelling() const noexcept { return handle_->cancelling(); }
    [[nodiscard]] bool cancelled() const noexcept { return handle_->cancelled(); }
    [[nodiscard]] std::exception_ptr exception() const { return handle_->exception(); }
    [[nodiscard]] T result() const { return handle_->result(); }

    void cancel() { handle_->cancel(); }
    void uncancel() { handle_->uncancel(); }

    [[nodiscard]] not_null<TaskState<T>*> get_state() const noexcept { return handle_; }

    [[nodiscard]] auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<TaskState<T>>(handle_);
    }

  private:
    [[nodiscard]] explicit Task(not_null<TaskState<T>*> state) noexcept : handle_(state) {
        handle_->inc_ref();
    }

    [[nodiscard]] Task(Coro<T> coro) : Task(std::move(wrap_impl(std::move(coro)))) {}

    [[nodiscard]] static Task<T> wrap_impl(Coro<T> coro) {
        co_return co_await std::move(coro);
    }

    not_null<TaskState<T>*> handle_;
};

[[nodiscard]] inline not_null<TaskState<>*> current_task() {
    if (detail::current_task == nullptr) {
        throw std::runtime_error("No current task");
    }
    return detail::current_task;
}

class TaskContext final {
    friend TaskContext get_context() noexcept;
    friend TaskContext current_context();

  public:
    [[nodiscard]] not_null<TaskState<>*> get_task() const {
        if (task_ == nullptr) {
            throw std::runtime_error("Did not await TaskContext before using it");
        }
        return task_;
    }
    [[nodiscard]] not_null<TaskManager*> get_global_task_manager() const {
        auto gtm = get_task()->get_global_task_manager();
        assert(gtm != nullptr && "Running Task must have a global TaskManager bound");
        return gtm;
    }
    [[nodiscard]] not_null<detail::EventLoop*> get_event_loop() const {
        auto loop = get_task()->get_event_loop();
        assert(loop != nullptr && "Running Task must have an EventLoop bound");
        return loop;
    }

    class Awaiter {
        friend class TaskContext;

      public:
        [[nodiscard]] bool await_ready() const noexcept { return false; }
        template <typename PromiseType>
        bool await_suspend(std::coroutine_handle<PromiseType> parent) const noexcept {
            ctxt_.task_ = parent.promise().get_task();
            return false;  // don't suspend the caller, just capture the context
        }
        [[nodiscard]] TaskContext await_resume() const noexcept { return ctxt_; }

      private:
        explicit Awaiter(TaskContext& ctxt) noexcept : ctxt_(ctxt) {}

        TaskContext& ctxt_;
    };

    [[nodiscard]] Awaiter operator co_await() noexcept { return Awaiter{*this}; }

  private:
    explicit TaskContext(TaskState<>* task) noexcept : task_(task) {}
    TaskContext() noexcept = default;

    TaskState<>* task_ = nullptr;
};

[[nodiscard]] inline TaskContext get_context() noexcept { return TaskContext{}; }

[[nodiscard]] inline TaskContext current_context() { return TaskContext{current_task()}; }

template <typename S>
template <typename PromiseType>
void detail::AwaitableAwaiter<S>::await_suspend(std::coroutine_handle<PromiseType> h) {
    parent_ = h;
    // This is non-null, so we want to use this instead of directly assigning to task_.
    auto task = h.promise().get_task();
    awaitable_->on_awaited(task);
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
    if (task_ != nullptr && task_->cancelling() != 0) {
        throw coconext::Cancelled{};
    }
    return awaitable_->result();
}

template <typename S>
void detail::AwaitableAwaiter<S>::event_run() noexcept {
    assert(parent_ != nullptr);
    assert(task_ != nullptr);
    auto task = not_null{task_};
    task->inc_ref();
    auto previous_task = detail::current_task;
    task->on_resume();
    parent_.resume();
    detail::current_task = previous_task;
    task->dec_ref();
}

template <typename T>
void AbstractFutureState<T>::on_awaited(not_null<TaskState<>*> task) {
    if (event_loop_ == nullptr) {
        event_loop_ = task->get_event_loop();
    } else if (event_loop_ != task->get_event_loop()) {
        throw std::runtime_error("Future is already bound to another EventLoop");
    }
}

inline void TaskState<>::on_done() noexcept {
    state_ = std::monostate{};
    for (auto& callback : callbacks_) {
        callback();
    }
    if (!waiters_.empty()) {
        assert(event_loop_ != nullptr);
        event_loop_->acquire().schedule_all_back(std::move(waiters_));
    }
    if (internal_done_callback_ != nullptr) {
        internal_done_callback_(this);
    }
    if (scheduler_owned_) {
        scheduler_owned_ = false;
        dec_ref();
    }
}

}  // namespace coconext

#endif  // COCONEXT_SCHEDULER_HPP

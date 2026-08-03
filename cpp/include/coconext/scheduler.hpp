#ifndef COCONEXT_SCHEDULER_HPP
#define COCONEXT_SCHEDULER_HPP

#include <coconext/event_loop.hpp>
#include <coconext/intrusive_deque.hpp>
#include <coconext/outcome.hpp>

#include <coroutine>
#include <cstddef>
#include <cstdint>
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

}  // namespace detail

template <typename T>
class FutureState;

template <typename T = detail::Erased>
class TaskManagerState;

template <typename T = detail::Erased>
class TaskState;

template <typename T, typename StateT = FutureState<T>>
class Future;

template <typename T>
class Task;

template <typename T>
class Coro;

template <typename StateT>
class TaskManager;

namespace detail {

// inline thread_local means this variable is included in the user's library, and lookups
// are fast (Local-Exec mode).
inline thread_local TaskState<>* current_task = nullptr;

template <typename T = Erased>
class AwaitableState;

template <>
class AwaitableState<detail::Erased> : public detail::IntrusiveDequeNode {
    template <typename>
    friend class AwaitableState;
    // This is an implementation detail only used for the following classes, so we avoid
    // protected and make friends.
    template <typename>
    friend class ::coconext::FutureState;
    template <typename>
    friend class ::coconext::TaskState;
    template <typename>
    friend class ::coconext::TaskManagerState;

    // This is a placeholder meaning the result is a value, but we can't store it here
    // because we don't know the type. The actual value is stored in AwaitableState<T>.
    struct Value {};

  public:
    bool done() const noexcept { return !std::holds_alternative<std::monostate>(result_); }

    std::exception_ptr exception() const {
        if (!done()) {
            throw std::runtime_error("Not done");
        } else if (std::holds_alternative<std::exception_ptr>(result_)) {
            return std::get<std::exception_ptr>(result_);
        }
        return nullptr;
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

  private:
    void set_exception(std::exception_ptr exc) noexcept {
        result_ = exc;
        on_done();
    }

    void set_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Awaitable is already bound to another EventLoop");
        }
    }

    // This is virtual to prevent a type from forgetting to define this, it's actually
    // dispatched to directly since we know AwaitableStateT.
    virtual void bind(EventLoop* loop, TaskManagerState<>* global_tm) = 0;

    void register_waiter(Event* awaiter) { deque_.push_back(awaiter); }

    void on_done() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
        event_loop_->acquire().schedule_all_back(std::move(deque_));
    }

  private:
    IntrusiveDeque<Event> deque_;
    std::vector<std::function<void()>> callbacks_;
    std::variant<std::monostate, std::exception_ptr, Value> result_;
    EventLoop* event_loop_ = nullptr;
};

template <typename T>
class AwaitableState : public AwaitableState<detail::Erased> {
    // This is an implementation detail only used for the following classes, so we avoid
    // protected and make friends.
    template <typename>
    friend class ::coconext::FutureState;
    template <typename>
    friend class ::coconext::TaskState;
    template <typename>
    friend class ::coconext::TaskManagerState;

  public:
    using value_type = T;

    T result() const {
        if (!AwaitableState<>::done()) {
            throw std::runtime_error("Not done");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        } else {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return result_value_->value;
            }
        }
    }

  private:
    void set_result(Result<T> value) noexcept {
        result_value_ = value;
        AwaitableState<>::on_done();
    }

  private:
    std::optional<Result<T>> result_value_;
};

template <typename AwaitableStateT>
class AwaitableAwaiter : Event {
  public:
    using value_type = typename AwaitableStateT::value_type;

    ~AwaitableAwaiter() { event_unschedule(); }

    bool await_ready() const noexcept { return awaitable_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h);
    value_type await_resume();

  private:
    explicit AwaitableAwaiter(AwaitableStateT* awaitable) : awaitable_(awaitable) {}

    void event_run() override;

    AwaitableStateT* awaitable_;
    std::coroutine_handle<> parent_;
    TaskState<>* task_;
};

template <typename T>
class FutureStateBase : public detail::AwaitableState<T> {
    template <typename, typename>
    friend class Future;

  public:
    using value_type = T;

    virtual ~FutureStateBase() = default;

  protected:
    // Override to unprime any underlying awaitable if refcount drops to zero.
    virtual void unprime() noexcept {}

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            unprime();
            delete this;
        }
    }

  private:
    size_t ref_count_{0};
};

}  // namespace detail

template <typename T>
class FutureState : public detail::FutureStateBase<T> {
  public:
    void set_result(T&& value) noexcept {
        detail::AwaitableState<T>::set_result(detail::Result<T>{std::move(value)});
    }
    void set_result(T const& value) noexcept {
        detail::AwaitableState<T>::set_result(detail::Result<T>{value});
    }
};

template <>
class FutureState<void> : public detail::FutureStateBase<void> {
  public:
    void set_void() noexcept {
        detail::AwaitableState<void>::set_result(detail::Result<void>{});
    }
};

// Single-shot, multiple-consumer awaitable object.
template <typename T, typename StateT>
class Future {
    static_assert(
        std::is_base_of_v<FutureState<T>, StateT>,
        "Future's StateT must derive from FutureStateBase<T>"
    );

  public:
    using value_type = T;

    Future() : handle_(new StateT{}) { handle_->inc_ref(); }
    ~Future() { handle_->dec_ref(); }
    Future(Future const& other) : handle_(other.handle_) { handle_->inc_ref(); }
    Future& operator=(Future const& other) {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }

    bool done() const noexcept { return handle_->done(); }
    T result() const { return handle_->result(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }

    template <typename F>
    void add_done_callback(F&& callback) {
        handle_->add_done_callback(std::forward<F>(callback));
    }

    auto operator co_await() { return detail::AwaitableAwaiter<StateT>(handle_); }

    static Future from_state(StateT& state) { return Future{state}; }
    StateT& get_state() const noexcept { return handle_; }

  private:
    explicit Future(StateT& state) : handle_(state) { handle_.inc_ref(); }

    StateT& handle_;
};

template <>
class TaskState<detail::Erased> : public detail::AwaitableState<> {
    template <typename>
    friend class Task;
    template <typename>
    friend class TaskState;
    template <typename>
    friend class TaskManagerState;
    friend class detail::AwaitableAwaiter<TaskState<>>;

    struct Scheduled : detail::Event {
        explicit Scheduled(TaskState<>* task) : task_(task) {}

        void event_run() override {
            task_->on_resume();
            auto handle = std::coroutine_handle<TaskState<>>::from_promise(*task_);
            handle.resume();
        }

        TaskState<>* task_;
    };

    struct Pending {
        detail::Event* event;
    };

    struct Running {};

  public:
    bool unstarted() const noexcept {
        return std::holds_alternative<std::monostate>(state_);
    }
    bool cancelled() const noexcept { return cancelled_ > 0; }

    void cancel() noexcept {
        if (this->done()) {
            return;
        }
        cancelled_++;
        if (unstarted()) {
            // We will never get the chance to check cancelled/uncancelled count, so we
            // force it done now.
            this->set_exception(std::make_exception_ptr(Cancelled{}));
            on_done();
            return;
        } else if (std::holds_alternative<Pending>(state_)) {
            auto& pending = std::get<Pending>(state_);
            pending.event->event_unschedule();
            state_ = Scheduled{this};
            this->get_event_loop()->acquire().schedule_back(&std::get<Scheduled>(state_));
        }
        // Not done, unstarted, or pending? Already scheduled, we will steal its place in
        // line.
    }
    void uncancel() {
        if (this->done()) {
            return;
        }
        if (cancelled_ == 0) {
            throw std::runtime_error("Task is not cancelled");
        }
        cancelled_--;
    }

    void start_soon(TaskManagerState<>& tm);

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            auto handle = std::coroutine_handle<TaskState<>>::from_promise(*this);
            handle.destroy();
        }
    }

    TaskState<>& get_task() noexcept { return *this; }
    TaskManagerState<>* get_task_manager() noexcept { return task_manager_; }
    TaskManagerState<>* get_global_task_manager() noexcept { return global_task_manager_; }
    detail::EventLoop* get_event_loop() noexcept { return event_loop_; }

    void bind(detail::EventLoop* loop, TaskManagerState<>* global_tm) {
        detail::AwaitableState<>::set_event_loop(loop);
        global_task_manager_ = global_tm;
    }

    void on_done() noexcept;

    void on_resume() {
        detail::current_task = this;
        state_ = Running{};
    }

    void set_pending(detail::Event* awaiter) { state_ = Pending{awaiter}; }

  private:
    std::variant<std::monostate, Scheduled, Pending, Running> state_;
    TaskManagerState<>* task_manager_ = nullptr;
    TaskManagerState<>* global_task_manager_ = nullptr;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

namespace detail {

template <typename T>
class TaskStateBase : public TaskState<>, public detail::AwaitableState<T> {
    Task<T> get_return_object() const noexcept { return Task<T>{this}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
};

}  // namespace detail

template <typename T>
class TaskState : public detail::TaskStateBase<T> {
  public:
    void unhandled_exception() noexcept {
        TaskState<>::set_exception(std::current_exception());
        TaskState<>::on_done();
    }
    void return_value(T value) noexcept {
        detail::AwaitableState<T>::set_result(detail::Result<T>{std::move(value)});
        TaskState<>::on_done();
    }
};

template <>
class TaskState<void> : public detail::TaskStateBase<void> {
  public:
    void unhandled_exception() noexcept {
        TaskState<>::set_exception(std::current_exception());
        TaskState<>::on_done();
    }
    void return_void() noexcept {
        detail::AwaitableState<void>::set_result(detail::Result<void>{});
        TaskState<>::on_done();
    }
};

template <typename T>
class Task {
  public:
    using value_type = T;

    ~Task() { handle_->dec_ref(); }

    Task(Task const& other) : handle_(other.handle_) { handle_->inc_ref(); }

    Task(Coro<T> coro) : Task(std::move(wrap_impl(std::move(coro)))) {}

    Task& operator=(Task const& other) {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        handle_.add_done_callback(std::forward<F>(callback));
    }

    bool unstarted() const noexcept { return handle_.unstarted(); }
    bool done() const noexcept { return handle_.done(); }
    bool cancelled() const noexcept { return handle_.cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_.exception(); }
    T result() { return handle_.result(); }

    void start_soon();
    void start_soon(TaskManagerState<>& tm);

    void cancel() noexcept { handle_.cancel(); }
    void uncancel() { handle_.uncancel(); }

    TaskState<T>& get_state() const noexcept { return handle_; }
    static Task<T> from_state(TaskState<T>& state) { return Task<T>{state}; }

    auto operator co_await() { return detail::AwaitableAwaiter<TaskState<T>>(handle_); }

  private:
    explicit Task(TaskState<T>& s) : handle_(s) { handle_.inc_ref(); }

    static Task<T> wrap_impl(Coro<T>&& coro) { co_return co_await std::move(coro); }

    TaskState<T>& handle_;
};

template <>
class TaskManagerState<detail::Erased> : public detail::AwaitableState<> {
    friend class TaskState<>;
    template <typename>
    friend class TaskManagerState;
    template <typename>
    friend class TaskManager;

  public:
    // TaskManagers have a couple states:
    // - open: tasks can be added.
    // - closed: no more tasks can be added, existing tasks are being waited until they
    //   complete.
    // - done: all tasks have completed, the manager has a result or exception.
    //
    // Managers can be cancelled in either the open or closed state. Cancellation implies
    // closed(), and like Tasks having to run again to throw CancelledError, a cancelled
    // TaskManager does not have a result until later.

    virtual ~TaskManagerState() = default;

    void add(TaskState<>& task) {
        if (detail::AwaitableState<>::done()) {
            throw std::runtime_error("Cannot add task to done TaskManager");
        }
        if (closed()) {
            throw std::runtime_error("Cannot add task to closed TaskManager");
        }
        if (task.unstarted()) {
            task.start_soon(*this);
        }
        task.inc_ref();
        tasks_.push_back(&task);
        on_add(&task);
    }

    void close() noexcept {
        if (detail::AwaitableState<>::done() || closed()) {
            return;
        }
        closed_ = true;
    }
    bool closed() const noexcept { return closed_; }
    void reopen() noexcept {
        if (detail::AwaitableState<>::done()) {
            return;
        }
        closed_ = false;
    }

    void cancel() noexcept {
        if (detail::AwaitableState<>::done() || cancelled()) {
            return;
        }
        cancelled_ = true;
        for (auto& task : tasks_) {
            task.cancel();
        }
    }

    bool cancelled() const noexcept { return cancelled_; }

  protected:
    // Hook 1: called after a task has been added to tasks_.
    virtual void on_add(TaskState<>* task) = 0;

    // Hook 2: called after each child completes and has been removed from tasks_.
    // Override to decide whether to call close(), inspect the completed task's outcome,
    // etc.
    virtual void on_child_done(TaskState<>* task) = 0;

    // Hook 3: called exactly once after tasks_ drains. Users call set_result() or
    // set_exception() to set the result of the TaskManager.
    virtual void on_drain_complete() = 0;

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    detail::EventLoop* get_event_loop() const noexcept {
        return detail::AwaitableState<>::event_loop_;
    }

    TaskManagerState<>* get_global_task_manager() const noexcept {
        return global_task_manager_;
    }

    void bind(detail::EventLoop* loop, TaskManagerState<>* global_tm) {
        detail::AwaitableState<>::set_event_loop(loop);
        global_task_manager_ = global_tm;
    }

    void internal_child_done(TaskState<>& task) {
        task.deque_remove();
        task.dec_ref();
        on_child_done(&task);
        if (!closed() && tasks_.empty()) {
            close();
        }
        if (closed() && tasks_.empty() && !detail::AwaitableState<>::done()) {
            on_drain_complete();
        }
    }

  protected:
    detail::IntrusiveDeque<TaskState<>> tasks_;

  private:
    TaskManagerState<>* global_task_manager_ = nullptr;
    size_t ref_count_{0};
    bool cancelled_ = false;
    bool closed_ = false;
};

template <typename T>
class TaskManagerState : public TaskManagerState<>, public detail::AwaitableState<T> {
  private:
    // prevent this from further subclassing.
    using detail::IntrusiveDequeNode::deque_remove;
};

template <typename StateT>
class TaskManager {
    static_assert(
        std::is_base_of_v<TaskManagerState<>, StateT>,
        "TaskManager's StateT must derive from TaskManagerState<>"
    );

  public:
    using value_type = typename StateT::value_type;

    TaskManager() : state_(new StateT{}) { state_->inc_ref(); }
    ~TaskManager() { state_->dec_ref(); }

    TaskManager(TaskManager const& other) : state_(other.state_) { state_->inc_ref(); }
    TaskManager& operator=(TaskManager const& other) {
        if (this != &other) {
            state_->dec_ref();
            state_ = other.state_;
            state_->inc_ref();
        }
        return *this;
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        state_.add_done_callback(std::forward<F>(callback));
    }

    bool done() const noexcept { return state_.done(); }
    bool cancelled() const noexcept { return state_.cancelled(); }
    value_type result() const { return state_.result(); }
    std::exception_ptr exception() const noexcept { return state_.exception(); }

    void add(TaskState<>& task) { state_.add(task); }

    void cancel() noexcept { state_.cancel(); }

    auto operator co_await() { return detail::AwaitableAwaiter<StateT>(state_); }

    StateT& get_state() const noexcept { return state_; }
    static TaskManager from_state(StateT& state) { return TaskManager{state}; }

  private:
    explicit TaskManager(StateT& state) : state_(state) { state_.inc_ref(); }

    StateT& state_;
};

template <typename T>
template <typename PromiseType>
void detail::AwaitableAwaiter<T>::await_suspend(std::coroutine_handle<PromiseType> h) {
    parent_ = h;
    task_ = h.promise().get_task();
    task_->set_pending(this);
    awaitable_->bind(task_->get_event_loop(), task_->get_global_task_manager());
    awaitable_->register_waiter(this);
}

template <typename T>
typename detail::AwaitableAwaiter<T>::value_type detail::AwaitableAwaiter<
    T>::await_resume() {
    if (task_->cancelled()) {
        throw coconext::Cancelled{};
    }
    return awaitable_->result();
}

void TaskState<>::start_soon(TaskManagerState<>& tm) {
    if (!unstarted()) {
        throw std::runtime_error("Task already started");
    }
    task_manager_ = &tm;
    bind(tm.get_event_loop(), tm.get_global_task_manager());
    state_ = TaskState<>::Scheduled{this};
    this->get_event_loop()->acquire().schedule_back(
        &std::get<TaskState<>::Scheduled>(state_)
    );
}

void TaskState<>::on_done() noexcept {
    state_ = std::monostate{};
    task_manager_->internal_child_done(*this);
}

template <typename AwaitableStateT>
void detail::AwaitableAwaiter<AwaitableStateT>::event_run() {
    task_->on_resume();
    parent_.resume();
}

template <typename T>
void Task<T>::start_soon(TaskManagerState<>& tm) {
    tm.add(handle_);
}

template <typename T>
void Task<T>::start_soon() {
    if (!detail::current_task) {
        throw std::runtime_error("No current Task");
    }
    detail::current_task->get_global_task_manager()->add(handle_);
}

template <typename T>
T run(Task<T> task);

template <typename T>
Task<T> start_soon(Task<T> task) {
    task.start_soon();
    return task;
}

template <typename T>
Task<T> start_soon(Coro<T> coro) {
    return start_soon(Task<T>{std::move(coro)});
}

}  // namespace coconext

#endif  // COCONEXT_SCHEDULER_HPP

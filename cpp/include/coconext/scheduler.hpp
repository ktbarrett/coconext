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

template <typename T = void, typename StateT = TaskManagerState<T>>
class TaskManager;

namespace detail {

// inline thread_local means this variable is included in the user's library, and lookups
// are fast (Local-Exec mode).
inline thread_local TaskState<>* current_task = nullptr;

template <typename T = Erased>
class AwaitableState;

template <typename T>
class FutureStateBase;

template <>
class AwaitableState<detail::Erased> : public detail::IntrusiveDequeNode {
    template <typename>
    friend class AwaitableState;
    // This is an implementation detail only used for the following classes, so we avoid
    // protected and make friends.
    template <typename>
    friend class ::coconext::detail::FutureStateBase;
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
    [[nodiscard]] bool done() const noexcept {
        return !std::holds_alternative<std::monostate>(result_);
    }

    [[nodiscard]] std::exception_ptr exception() const {
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

    [[nodiscard]] EventLoop* get_event_loop() const noexcept { return event_loop_; }

  private:
    void set_exception(std::exception_ptr exc) noexcept {
        assert(exc);  // no null exceptions
        result_ = exc;
        on_done();
    }

    void bind_event_loop(EventLoop& loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = &loop;
        } else if (event_loop_ != &loop) {
            throw std::runtime_error("Awaitable is already bound to another EventLoop");
        }
    }

    // This method is virtual to prevent a type from forgetting to define this, it's
    // actually dispatched to directly since we know AwaitableStateT.
    //
    // This is the custom behavior for each Awaitable when it's awaited, it's given the
    // TaskState<> which is either directly or indirectly (through Coros) awaiting it.
    virtual void on_awaited(TaskState<>& task) = 0;

    void register_waiter(Event& awaiter) noexcept { deque_.push_back(&awaiter); }

    void on_done() {
        for (auto& callback : callbacks_) {
            callback();
        }
        assert(event_loop_ != nullptr);
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

    [[nodiscard]] value_type result() const {
        if (!AwaitableState<>::done()) {
            throw std::runtime_error("Not done");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        } else {
            assert(
                result_value_.has_value()
                && "AwaitableState is done but result_value_ is not set"
            );
            return *result_value_;
        }
    }

  private:
    void set_result(T&& value) noexcept {
        result_value_ = value;
        result_ = Value{};
    }
    void set_result(T const& value) noexcept {
        result_value_ = value;
        result_ = Value{};
    }

  private:
    std::optional<T> result_value_;
};

template <>
class AwaitableState<void> : public AwaitableState<detail::Erased> {
    // This is an implementation detail only used for the following classes, so we avoid
    // protected and make friends.
    template <typename>
    friend class ::coconext::FutureState;
    template <typename>
    friend class ::coconext::TaskState;
    template <typename>
    friend class ::coconext::TaskManagerState;

  public:
    using value_type = void;

    void result() const {
        if (!AwaitableState<>::done()) {
            throw std::runtime_error("Not done");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
    }

  private:
    void set_void() noexcept { result_ = Value{}; }
};

template <typename AwaitableStateT>
class AwaitableAwaiter : private Event {
  public:
    using value_type = typename AwaitableStateT::value_type;

    ~AwaitableAwaiter() { event_unschedule(); }

    [[nodiscard]] bool await_ready() const noexcept { return awaitable_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) noexcept;
    [[nodiscard]] value_type await_resume();

  private:
    [[nodiscard]] explicit AwaitableAwaiter(AwaitableStateT& awaitable)
        : awaitable_(awaitable) {}

    void event_run() noexcept override;

  private:
    AwaitableStateT& awaitable_;
    std::coroutine_handle<> parent_ = nullptr;
    TaskState<>* task_ = nullptr;
};

template <typename T>
class FutureStateBase : public detail::AwaitableState<T> {
    template <typename, typename>
    friend class Future;

  public:
    virtual ~FutureStateBase() {
        if (!detail::AwaitableState<>::done()) {
            unprime();
        }
    };

  protected:
    // Override to unprime any underlying awaitable if refcount drops to zero.
    virtual void unprime() noexcept {}

    using detail::AwaitableState<>::set_exception;

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            unprime();
            delete this;
        }
    }

    void on_awaited(TaskState<>& task) final;

  private:
    size_t ref_count_{0};
};

}  // namespace detail

template <typename T>
class FutureState : public detail::FutureStateBase<T> {
  protected:
    using detail::AwaitableState<T>::set_result;
};

template <>
class FutureState<void> : public detail::FutureStateBase<void> {
  protected:
    using detail::AwaitableState<void>::set_void;
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

    [[nodiscard]] Future() noexcept : handle_(new StateT{}) { handle_->inc_ref(); }
    [[nodiscard]] Future(Future const& other) noexcept : handle_(other.handle_) {
        handle_->inc_ref();
    }

    Future& operator=(Future const& other) noexcept {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }

    ~Future() noexcept { handle_->dec_ref(); }

    [[nodiscard]] bool done() const noexcept { return handle_->done(); }
    [[nodiscard]] T result() const { return handle_->result(); }
    [[nodiscard]] std::exception_ptr exception() const { return handle_->exception(); }

    template <typename F>
    void add_done_callback(F&& callback) {
        handle_->add_done_callback(std::forward<F>(callback));
    }

    [[nodiscard]] auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<StateT>(*handle_);
    }

    [[nodiscard]] static Future from_state(StateT& state) noexcept { return Future{state}; }
    [[nodiscard]] StateT& get_state() const noexcept { return *handle_; }

  private:
    [[nodiscard]] explicit Future(StateT& state) noexcept : handle_(&state) {
        handle_->inc_ref();
    }

    StateT* handle_;
};

template <>
class TaskState<detail::Erased> : public detail::AwaitableState<> {
    template <typename>
    friend class Task;
    template <typename>
    friend class TaskState;
    template <typename>
    friend class detail::FutureStateBase;
    template <typename>
    friend class TaskManagerState;
    friend class detail::AwaitableAwaiter<TaskState<>>;

    struct Scheduled : detail::Event {
        [[nodiscard]] explicit Scheduled(TaskState<>* task) : task_(task) {}

        void event_run() noexcept override {
            task_->on_resume();
            auto handle = std::coroutine_handle<TaskState<>>::from_promise(*task_);
            // Technically resume() is not noexcept and can throw, but not in our
            // implementation.
            handle.resume();
        }

        // TODO: non-null pointer, we can't use ref since this needs to be assignable.
        TaskState<>* task_;
    };

    struct Pending {
        [[nodiscard]] explicit Pending(detail::Event* event) : event(event) {}

        // TODO: non-null pointer, we can't use ref since this needs to be assignable.
        detail::Event* event;
    };

    struct Running {};

  public:
    [[nodiscard]] bool unstarted() const noexcept {
        return std::holds_alternative<std::monostate>(state_);
    }
    [[nodiscard]] bool cancelled() const noexcept { return cancelled_ > 0; }

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

    void start_soon();

    [[nodiscard]] TaskManagerState<>* get_task_manager() const noexcept {
        return task_manager_;
    }
    [[nodiscard]] TaskManagerState<>* get_global_task_manager() const noexcept {
        return global_task_manager_;
    }

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            auto handle = std::coroutine_handle<TaskState<>>::from_promise(*this);
            handle.destroy();
        }
    }

    // This is an abstraction point between Coro and Task to get the current Task from the
    // awaiting coroutine's promise instead of a TLS lookup.
    [[nodiscard]] TaskState<>& get_task() noexcept { return *this; }

    void on_awaited(TaskState<>& task) final {
        // If we are running already, we can assume the necessary bits are already bound.
        if (!task.unstarted()) {
            return;
        }
        // Bind the event loop.
        auto event_loop = task.get_event_loop();
        assert(event_loop != nullptr && "Running Task must have an EventLoop bound");
        bind_event_loop(*event_loop);

        // Don't bind task_manager_ when awaiting directly, there is no need to manage its
        // lifetime since we are holding reference to it until it finishes and we don't want
        // adoptive siblings be cancelled if it fails.

        // Bind the global task manager.
        auto global_task_manager = task.get_global_task_manager();
        assert(
            global_task_manager != nullptr
            && "Running Task must have a global TaskManager bound"
        );
        bind_global_task_manager(*global_task_manager);

        event_loop_->acquire().schedule_back(&std::get<Scheduled>(state_));
    }

    void bind_task_manager(TaskManagerState<>& task_manager) {
        if (task_manager_ != nullptr && task_manager_ != &task_manager) {
            throw std::runtime_error("Task is already bound to a TaskManager");
        }
        task_manager_ = &task_manager;
    }

    void bind_global_task_manager(TaskManagerState<>& task_manager) noexcept {
        if (global_task_manager_ != nullptr) {
            return;
        }
        global_task_manager_ = &task_manager;
    }

    void on_done() noexcept;

    void on_resume() noexcept {
        detail::current_task = this;
        state_ = Running{};
    }

    void on_awaiting(detail::Event& awaiter) noexcept { state_ = Pending{&awaiter}; }

  private:
    std::variant<std::monostate, Scheduled, Pending, Running> state_;
    TaskManagerState<>* task_manager_ = nullptr;
    TaskManagerState<>* global_task_manager_ = nullptr;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

template <typename T>
class TaskState : public TaskState<>, public detail::AwaitableState<T> {
  public:
    [[nodiscard]] Task<T> get_return_object() noexcept {
        return Task<T>::from_state(*this);
    }
    [[nodiscard]] std::suspend_always initial_suspend() noexcept { return {}; }
    [[nodiscard]] std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() noexcept {
        TaskState<>::set_exception(std::current_exception());
        TaskState<>::on_done();
    }
    void return_value(T value) noexcept {
        set_result(std::move(value));
        TaskState<>::on_done();
    }
};

template <>
class TaskState<void> : public TaskState<>, public detail::AwaitableState<void> {
  public:
    [[nodiscard]] Task<void> get_return_object() noexcept;
    [[nodiscard]] std::suspend_always initial_suspend() noexcept { return {}; }
    [[nodiscard]] std::suspend_always final_suspend() noexcept { return {}; }

    void unhandled_exception() noexcept {
        TaskState<>::set_exception(std::current_exception());
        TaskState<>::on_done();
    }
    void return_void() noexcept {
        set_void();
        TaskState<>::on_done();
    }
};

template <typename T>
class Task {
  public:
    using value_type = T;

    [[nodiscard]] Task(Task const& other) noexcept : handle_(other.handle_) {
        handle_->inc_ref();
    }
    [[nodiscard]] Task(Coro<T> coro) noexcept
        : Task(std::move(wrap_impl(std::move(coro)))) {}

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

    [[nodiscard]] bool unstarted() const noexcept { return handle_->unstarted(); }
    [[nodiscard]] bool done() const noexcept { return handle_->done(); }
    [[nodiscard]] bool cancelled() const noexcept { return handle_->cancelled(); }
    [[nodiscard]] std::exception_ptr exception() const { return handle_->exception(); }
    [[nodiscard]] T result() const { return handle_->result(); }

    void start_soon();

    void cancel() noexcept { handle_->cancel(); }
    void uncancel() { handle_->uncancel(); }

    [[nodiscard]] TaskState<T>& get_state() const noexcept { return *handle_; }
    [[nodiscard]] static Task<T> from_state(TaskState<T>& state) noexcept {
        return Task<T>{state};
    }

    [[nodiscard]] auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<TaskState<T>>(*handle_);
    }

  private:
    [[nodiscard]] explicit Task(TaskState<T>& s) noexcept : handle_(&s) {
        handle_->inc_ref();
    }

    [[nodiscard]] static Task<T> wrap_impl(Coro<T>&& coro) noexcept {
        co_return co_await std::move(coro);
    }

    TaskState<T>* handle_;
};

template <>
class TaskManagerState<detail::Erased> : public detail::AwaitableState<> {
    friend class TaskState<>;
    template <typename>
    friend class TaskManagerState;
    template <typename, typename>
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
            task.start_soon();
        }
        task.bind_task_manager(*this);
        task.inc_ref();
        tasks_.push_back(&task);
        on_add(task);
    }

    void close() noexcept {
        if (detail::AwaitableState<>::done() || closed()) {
            return;
        }
        closed_ = true;
    }
    [[nodiscard]] bool closed() const noexcept { return closed_; }
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

    [[nodiscard]] bool cancelled() const noexcept { return cancelled_; }

  protected:
    // Hook 1: called after a task has been added to tasks_.
    virtual void on_add(TaskState<>& task) noexcept = 0;

    // Hook 2: called after each child completes and has been removed from tasks_.
    // Override to decide whether to call close(), inspect the completed task's outcome,
    // etc.
    virtual void on_child_done(TaskState<>& task) noexcept = 0;

    // Hook 3: called exactly once after tasks_ drains. Users call set_result() or
    // set_exception() to set the result of the TaskManager.
    virtual void on_drain_complete() noexcept = 0;

    using detail::AwaitableState<>::set_exception;

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    void internal_child_done(TaskState<>& task) noexcept {
        task.deque_remove();
        task.dec_ref();
        on_child_done(task);
        if (!closed() && tasks_.empty()) {
            close();
        }
        if (closed() && tasks_.empty() && !detail::AwaitableState<>::done()) {
            on_drain_complete();
        }
    }

    void on_awaited(TaskState<>& task) final {
        // bind event loop
        auto event_loop = task.get_event_loop();
        assert(event_loop != nullptr && "Running Task must have an EventLoop bound");
        bind_event_loop(*event_loop);
    }

    // prevent this from further subclassing.
    using detail::IntrusiveDequeNode::deque_remove;

  protected:
    detail::IntrusiveDeque<TaskState<>> tasks_;

  private:
    size_t ref_count_{0};
    bool cancelled_ = false;
    bool closed_ = false;
};

template <typename T>
class TaskManagerState : public TaskManagerState<>, public detail::AwaitableState<T> {
  protected:
    using detail::AwaitableState<T>::set_result;
};

template <>
class TaskManagerState<void> : public TaskManagerState<>,
                               public detail::AwaitableState<void> {
  protected:
    using detail::AwaitableState<void>::set_void;
};

template <typename T, typename StateT>
class TaskManager {
    static_assert(
        std::is_base_of_v<TaskManagerState<>, StateT>,
        "TaskManager's StateT must derive from TaskManagerState<>"
    );

  public:
    using value_type = T;

    [[nodiscard]] TaskManager() noexcept : state_(new StateT{}) { state_->inc_ref(); }
    [[nodiscard]] TaskManager(TaskManager const& other) noexcept : state_(other.state_) {
        state_->inc_ref();
    }

    TaskManager& operator=(TaskManager const& other) noexcept {
        if (this != &other) {
            state_->dec_ref();
            state_ = other.state_;
            state_->inc_ref();
        }
        return *this;
    }

    ~TaskManager() noexcept { state_->dec_ref(); }

    template <typename F>
    void add_done_callback(F&& callback) {
        state_->add_done_callback(std::forward<F>(callback));
    }

    [[nodiscard]] bool done() const noexcept { return state_->done(); }
    [[nodiscard]] bool cancelled() const noexcept { return state_->cancelled(); }
    [[nodiscard]] T result() const { return state_->result(); }
    [[nodiscard]] std::exception_ptr exception() const { return state_->exception(); }

    void add(TaskState<>& task) { state_->add(task); }

    void cancel() noexcept { state_->cancel(); }

    [[nodiscard]] auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<StateT>(*state_);
    }

    [[nodiscard]] StateT& get_state() const noexcept { return *state_; }
    [[nodiscard]] static TaskManager from_state(StateT& state) noexcept {
        return TaskManager{state};
    }

  private:
    [[nodiscard]] explicit TaskManager(StateT& state) noexcept : state_(&state) {
        state_->inc_ref();
    }

    StateT* state_;
};

[[nodiscard]] inline TaskState<>& current_task() {
    if (detail::current_task == nullptr) {
        throw std::runtime_error("No current task");
    }
    return *detail::current_task;
}

[[nodiscard]] inline TaskManagerState<>& current_global_task_manager() {
    auto& task = current_task();
    auto global_task_manager = task.get_global_task_manager();
    assert(
        global_task_manager != nullptr
        && "Running Task must have a global TaskManager bound"
    );
    return *global_task_manager;
}

[[nodiscard]] inline detail::EventLoop& current_event_loop() {
    auto& task = current_task();
    auto event_loop = task.get_event_loop();
    assert(event_loop != nullptr && "Running Task must have an EventLoop bound");
    return *event_loop;
}

template <typename T>
template <typename PromiseType>
void detail::AwaitableAwaiter<T>::await_suspend(
    std::coroutine_handle<PromiseType> h
) noexcept {
    parent_ = h;
    auto& task = h.promise().get_task();
    task.on_awaiting(*this);
    task_ = &task;
    awaitable_->on_awaited(task);
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

template <typename AwaitableStateT>
void detail::AwaitableAwaiter<AwaitableStateT>::event_run() noexcept {
    assert(parent_ != nullptr);
    assert(task_ != nullptr);
    task_->on_resume();
    // technically resume() is not noexcept and can throw, but not in our implementation.
    parent_.resume();
}

template <typename T>
void detail::FutureStateBase<T>::on_awaited(TaskState<>& task) {
    auto event_loop = task.get_event_loop();
    assert(event_loop != nullptr && "Running Task must have an EventLoop bound");
    this->bind_event_loop(*event_loop);
}

inline void TaskState<>::on_done() noexcept {
    state_ = std::monostate{};
    // We may not have a manager for this Task if it was directly awaited.
    if (task_manager_) {
        task_manager_->internal_child_done(*this);
    }
}

inline void TaskState<>::start_soon() {
    if (!unstarted()) {
        throw std::runtime_error("Task is already started");
    }
    if (global_task_manager_ == nullptr) {
        bind_global_task_manager(current_global_task_manager());
    }
    if (event_loop_ == nullptr) {
        bind_event_loop(current_event_loop());
    }
    event_loop_->acquire().schedule_back(&std::get<TaskState<>::Scheduled>(state_));
}

[[nodiscard]] Task<void> TaskState<void>::get_return_object() noexcept {
    return Task<void>::from_state(*this);
}

template <typename T>
void Task<T>::start_soon() {
    handle_->start_soon();
}

template <typename T>
T run(Task<T> task);

template <typename T>
Task<T> start_soon(Task<T> task) {
    task.start_soon();
    current_global_task_manager().add(task.get_state());
    return task;
}

template <typename T>
Task<T> start_soon(Coro<T> coro) {
    return start_soon(Task<T>{std::move(coro)});
}

}  // namespace coconext

#endif  // COCONEXT_SCHEDULER_HPP

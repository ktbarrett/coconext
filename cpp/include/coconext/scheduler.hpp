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

template <typename T, typename StateT = TaskManagerState<T>>
class TaskManager;

namespace detail {

// inline thread_local means this variable is included in the user's library, and lookups
// are fast (Local-Exec mode).
inline thread_local TaskState<>* current_task = nullptr;

template <typename T>
class AwaitableStateBase {
    // This is an implementation detail only used for the following classes, so we avoid
    // protected and make friends.
    friend class ::coconext::FutureState<T>;
    friend class ::coconext::TaskState<T>;
    friend class ::coconext::TaskManagerState<T>;

  public:
    using value_type = T;

    bool done() const noexcept { return !std::holds_alternative<std::monostate>(result_); }

    T result() const {
        if (std::holds_alternative<detail::Exception>(result_)) {
            std::rethrow_exception(std::get<detail::Exception>(result_).exception);
        }
        if (std::holds_alternative<detail::Result<T>>(result_)) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::get<detail::Result<T>>(result_).value;
            }
        }
        throw std::runtime_error("Not done");
    }
    std::exception_ptr exception() const noexcept {
        if (std::holds_alternative<detail::Exception>(result_)) {
            return std::get<detail::Exception>(result_).exception;
        }
        return nullptr;
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

  private:
    void set_result(Result<T> value) noexcept {
        result_ = detail::Result{std::move(value)};
        on_done();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        result_ = detail::Exception{exc};
        on_done();
    }
    EventLoop* get_event_loop() noexcept { return event_loop_; }
    void set_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Awaitable is already bound to another EventLoop");
        }
    }

    void bind(EventLoop* loop, TaskManagerState<>* /*global_tm*/) { set_event_loop(loop); }

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
    std::variant<std::monostate, Result<T>, Exception> result_;
    EventLoop* event_loop_ = nullptr;
};

template <typename AwaitableStateT>
class AwaitableAwaiter : Event {
  public:
    ~AwaitableAwaiter() { event_unschedule(); }

    bool await_ready() const noexcept { return awaitable_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) {
        parent_ = h;
        task_ = h.promise().get_task_for_awaitable();
        task_->set_pending(this);
        awaitable_->bind(task_->get_event_loop(), task_->get_global_task_manager());
        awaitable_->register_waiter(this);
    }
    typename AwaitableStateT::value_type await_resume() {
        if (task_->cancelled()) {
            throw coconext::Cancelled{};
        }
        return awaitable_->result();
    }

  private:
    explicit AwaitableAwaiter(AwaitableStateT* awaitable) : awaitable_(awaitable) {}

    void event_run() override {
        task_->on_resume();
        parent_.resume();
    }

    AwaitableStateT* awaitable_;
    std::coroutine_handle<> parent_;
    TaskState<>* task_;
};

}  // namespace detail

template <typename T>
class FutureStateBase : public detail::AwaitableStateBase<T> {
    template <typename, typename>
    friend class Future;

  public:
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

template <typename T>
class FutureState : public FutureStateBase<T> {
  public:
    void set_result(T&& value) noexcept {
        detail::AwaitableStateBase<T>::set_result(detail::Result<T>{std::move(value)});
    }
    void set_result(T const& value) noexcept {
        detail::AwaitableStateBase<T>::set_result(detail::Result<T>{value});
    }
};

template <>
class FutureState<void> : public FutureStateBase<void> {
  public:
    void set_void() noexcept {
        detail::AwaitableStateBase<void>::set_result(detail::Result<void>{});
    }
};

// Single-shot, multiple-consumer awaitable object.
template <typename T, typename StateT>
class Future {
    static_assert(
        std::is_base_of_v<FutureStateBase<T>, StateT>,
        "Future's StateT must derive from FutureStateBase<T>"
    );

  public:
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

template <typename T>
class TaskStateBase : public detail::AwaitableStateBase<T> {
    friend class Task<T>;
    template <typename>
    friend class TaskManagerState;

    struct Scheduled : detail::Event {
        explicit Scheduled(TaskStateBase<T>& task) : task_(&task) {}

        void event_run() override {
            task_->on_resume();
            auto handle = std::coroutine_handle<TaskState<T>>::from_promise(
                *static_cast<TaskState<T>*>(task_)
            );
            handle.resume();
        }

        TaskStateBase<T>* task_;
    };

    struct Pending {
        detail::Event* event;
    };

    struct Running {};

  public:
    Task<T> get_return_object() { return Task<T>{static_cast<TaskState<T>*>(this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() {
        this->set_exception(std::current_exception());
        on_done();
    }

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
            state_ = Scheduled{*this};
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

    void start_soon(TaskManagerState<>* tm);

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            auto handle = std::coroutine_handle<TaskState<T>>::from_promise(
                *static_cast<TaskState<T>*>(this)
            );
            handle.destroy();
        }
    }

    TaskState<>* get_task() noexcept { return this; }
    TaskManagerState<>* get_task_manager() noexcept { return task_manager_; }
    TaskManagerState<>* get_global_task_manager() noexcept { return global_task_manager_; }
    void bind(detail::EventLoop* loop, TaskManagerState<>* global_tm) {
        detail::AwaitableStateBase<T>::set_event_loop(loop);
        global_task_manager_ = global_tm;
    }

    void on_done() noexcept {
        state_ = std::monostate{};
        if (task_manager_) {
            task_manager_->internal_child_done(static_cast<TaskState<T>*>(this));
        }
    }

    void on_resume() {
        current_task = this;
        state_ = Running{};
    }

    void set_pending(detail::Event* future_awaiter) { state_ = Pending{future_awaiter}; }

    std::variant<std::monostate, Scheduled, Pending, Running> state_;
    TaskManagerState<>* task_manager_ = nullptr;
    TaskManagerState<>* global_task_manager_ = nullptr;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

template <typename T>
class TaskState : public TaskStateBase<T> {
  public:
    void return_value(T value) {
        this->set_result(Result<T>{std::move(value)});
        this->on_done();
    }
};

template <>
class TaskState<void> : public TaskStateBase<void> {
  public:
    void return_void() {
        this->set_result(Result<void>{});
        this->on_done();
    }
};

template <typename T>
class Task {
  public:
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
    void start_soon(TaskManager<>& tm);

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

template <typename T>
class TaskManagerState : public detail::AwaitableStateBase<T> {
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

    void add(TaskState<>* task) {
        if (this->done()) {
            throw std::runtime_error("Cannot add task to done TaskManager");
        }
        if (closed()) {
            throw std::runtime_error("Cannot add task to closed TaskManager");
        }
        if (task->unstarted()) {
            task->start_soon(this);
        }
        task->inc_ref();
        tasks_.push_back(task);
        on_add(task);
    }

    void close() noexcept {
        if (this->done() || closed()) {
            return;
        }
        closed_ = true;
    }
    bool closed() const noexcept { return closed_; }
    void reopen() noexcept {
        if (this->done()) {
            return;
        }
        closed_ = false;
    }

    void cancel() noexcept {
        if (this->done() || cancelled()) {
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

    // Hook 3: called exactly once after tasks_ drains. Return the manager's outcome.
    virtual Outcome<T> on_drain_complete() = 0;

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    detail::EventLoop* get_event_loop() noexcept { return this->get_event_loop(); }
    TaskManagerState<>* get_global_task_manager() noexcept { return global_task_manager_; }
    void bind(detail::EventLoop* loop, TaskManagerState<>* global_tm) {
        detail::AwaitableStateBase<T>::set_event_loop(loop);
        global_task_manager_ = global_tm;
    }

    void internal_child_done(TaskState<>* task) {
        task->remove_child();
        task->dec_ref();
        on_child_done(task);
        if (!closed() && tasks_.empty()) {
            close();
        }
        if (closed() && tasks_.empty() && !this->done()) {
            Outcome<T> outcome = on_drain_complete();
            if (outcome.has_exception()) {
                this->set_exception(outcome.exception());
            } else {
                if constexpr (std::is_void_v<T>) {
                    this->set_result(detail::Result<void>{});
                } else {
                    this->set_result(detail::Result<T>{outcome.value()});
                }
            }
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

template <typename T, typename StateT>
class TaskManager {
    static_assert(
        std::is_base_of_v<TaskManagerState<>, StateT>,
        "TaskManager's StateT must derive from TaskManagerState<>"
    );

  public:
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
    T result() const { return state_.result(); }
    std::exception_ptr exception() const noexcept { return state_.exception(); }

    void add(Task<>& task) { state_.add(task); }

    void cancel() noexcept { state_.cancel(); }

    auto operator co_await() { return detail::AwaitableAwaiter<StateT>(state_); }

    StateT& get_state() const noexcept { return state_; }
    static TaskManager from_state(StateT& state) { return TaskManager{state}; }

  private:
    explicit TaskManager(StateT& state) : state_(state) { state_.inc_ref(); }

    StateT& state_;
};

template <typename T>
void TaskStateBase<T>::start_soon(TaskManagerState<>* tm) {
    if (!unstarted()) {
        throw std::runtime_error("Task already started");
    }
    task_manager_ = tm;
    bind(tm->get_event_loop(), tm->get_global_task_manager());
    state_ = Scheduled{*this};
    this->get_event_loop()->acquire().schedule_back(&std::get<Scheduled>(state_));
}

template <typename T>
void Task<T>::start_soon(TaskManager<>& tm) {
    Task<> erased = Task<>::from_state(handle_);
    tm.add(erased);
}

template <typename T>
void Task<T>::start_soon() {
    if (!detail::current_task) {
        throw std::runtime_error("No current Task");
    }
    Task<> erased = Task<>::from_state(handle_);
    detail::current_task->get_global_task_manager()->add(erased);
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

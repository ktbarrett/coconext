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

template <typename T>
class FutureState;

template <typename T = Erased>
class TaskManagerState;

template <typename T = Erased>
class TaskState;

}  // namespace detail

template <typename T, typename StateT = detail::FutureState<T>>
class Future;

template <typename T = detail::Erased>
class Task;

template <typename T>
class Coro;

template <typename StateT = detail::TaskManagerState<>>
class TaskManager;

namespace detail {

template <typename StateT>
class AwaitableAwaiter;

template <>
class TaskState<Erased> : public IntrusiveDequeNode {
    friend class TaskManagerState<>;
    template <typename>
    friend class AwaitableAwaiter;

  public:
    virtual void inc_ref() noexcept = 0;
    virtual void dec_ref() noexcept = 0;

    virtual TaskState<>* get_task() noexcept = 0;
    virtual TaskManagerState<>* get_task_manager() noexcept = 0;
    virtual EventLoop* get_event_loop() noexcept = 0;

    virtual bool unstarted() const noexcept = 0;
    virtual bool cancelled() const noexcept = 0;
    virtual bool done() const noexcept = 0;
    virtual std::exception_ptr exception() const noexcept = 0;

    virtual void start_soon(TaskManagerState<>* loop) = 0;
    virtual void cancel() noexcept = 0;
    virtual void uncancel() = 0;

  private:
    void remove_child() { deque_remove(); }
    using IntrusiveDequeNode::deque_remove;

    virtual void set_pending(Event* future_awaiter) = 0;
    virtual void on_resume() = 0;
};

template <>
class TaskManagerState<Erased> {
    template <typename>
    friend class TaskStateBase;

  public:
    virtual ~TaskManagerState() = default;

    virtual void inc_ref() noexcept = 0;
    virtual void dec_ref() noexcept = 0;

    virtual void add(Task<>& task) = 0;
    virtual void close() noexcept = 0;
    virtual bool closed() const noexcept = 0;
    virtual void reopen() noexcept = 0;

    virtual void cancel() noexcept = 0;

    virtual EventLoop* get_event_loop() noexcept = 0;
    virtual void set_event_loop(EventLoop* loop) = 0;

    virtual bool done() const noexcept = 0;
    virtual bool cancelled() const noexcept = 0;

  private:
    virtual void internal_child_done(TaskState<>* task) = 0;
};

// Shared machinery for anything Future-awaitable: an outcome slot, a waiter deque, done
// callbacks, and an EventLoop binding. No virtuals. Callbacks are void() -- users capture
// whatever handle they need.
template <typename T>
class AwaitableStateBase {
    template <typename>
    friend class FutureAwaiter;

  public:
    using value_type = T;

    bool done() const noexcept { return !std::holds_alternative<std::monostate>(result_); }

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
        throw std::runtime_error("Not done");
    }
    std::exception_ptr exception() const noexcept {
        if (std::holds_alternative<Exception>(result_)) {
            return std::get<Exception>(result_).exception;
        }
        return nullptr;
    }

    void set_result(Result<T> value) noexcept {
        result_ = std::move(value);
        on_done();
    }
    void set_exception(std::exception_ptr exc) noexcept {
        result_ = Exception{exc};
        on_done();
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

    EventLoop* get_event_loop() noexcept { return event_loop_; }
    void set_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Awaitable is already bound to another EventLoop");
        }
    }

    void register_waiter(Event* awaiter) { deque_.push_back(awaiter); }

  private:
    void on_done() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
        event_loop_->acquire().schedule_all_back(std::move(deque_));
    }

    IntrusiveDeque<Event> deque_;
    std::vector<std::function<void()>> callbacks_;
    std::variant<std::monostate, Result<T>, Exception> result_;
    EventLoop* event_loop_ = nullptr;
};

template <typename StateT>
class AwaitableAwaiter : Event {
    template <typename, typename>
    friend class ::coconext::Future;
    template <typename>
    friend class Task;
    template <typename>
    friend class ::coconext::TaskManager;

  public:
    bool await_ready() const noexcept { return awaitable_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) {
        parent_ = h;
        task_ = h.promise().get_task();
        task_->set_pending(this);
        awaitable_->set_event_loop(task_->get_event_loop());
        awaitable_->register_waiter(this);
    }
    typename StateT::value_type await_resume() {
        if (task_->cancelled()) {
            throw coconext::Cancelled{};
        }
        return awaitable_->result();
    }

    ~AwaitableAwaiter() { event_unschedule(); }

  private:
    explicit AwaitableAwaiter(StateT* awaitable) : awaitable_(awaitable) {}

    void event_run() override {
        task_->on_resume();
        parent_.resume();
    }

    StateT* awaitable_;
    std::coroutine_handle<> parent_;
    TaskState<>* task_;
};

template <typename T>
class FutureStateBase : public AwaitableStateBase<T> {
    friend class FutureState<T>;

  public:
    virtual ~FutureStateBase() = default;

    bool cancelled() const noexcept { return cancelled_; }
    void cancel() noexcept {
        if (cancelled_) {
            return;
        }
        cancelled_ = true;
        unprime();
        if (!this->done()) {
            this->set_exception(std::make_exception_ptr(Cancelled{}));
        }
    }

    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            cancel();
            delete this;
        }
    }

  protected:
    // Called from cancel() exactly once (guarded by cancelled_). Subclasses that arm
    // external callbacks in their ctor should override to disarm them here -- this fires
    // both on explicit cancel() and when the last handle drops, so the disarm always
    // runs while the state is still alive.
    virtual void unprime() noexcept {}

  private:
    size_t ref_count_{0};
    bool cancelled_ = false;
};

template <typename T>
class FutureState : public FutureStateBase<T> {
  public:
    void set_result(T&& value) noexcept {
        AwaitableStateBase<T>::set_result(Result<T>{std::move(value)});
    }
    void set_result(T const& value) noexcept {
        AwaitableStateBase<T>::set_result(Result<T>{value});
    }
};

template <>
class FutureState<void> : public FutureStateBase<void> {
  public:
    void set_void() noexcept { AwaitableStateBase<void>::set_result(Result<void>{}); }
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T, typename StateT>
class Future {
    static_assert(
        std::is_base_of_v<detail::FutureStateBase<T>, StateT>,
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
    T result() const { return handle_->result(); }

    bool done() const noexcept { return handle_->done(); }
    bool cancelled() const noexcept { return handle_->cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }

    template <typename F>
    void add_done_callback(F&& callback) {
        handle_->add_done_callback(std::forward<F>(callback));
    }

    auto operator co_await() { return detail::AwaitableAwaiter<StateT>(handle_); }

    static Future from_state(StateT* state) { return Future{state}; }
    StateT* get_state() const noexcept { return handle_; }

  private:
    explicit Future(StateT* state) : handle_(state) { handle_->inc_ref(); }

    StateT* handle_;
};

namespace detail {

// inline thread_local means this variable is included in the user's library, and lookups
// are fast (Local-Exec mode).
inline thread_local TaskState<>* current_task = nullptr;

template <typename T>
class TaskStateBase : public TaskState<Erased>, public AwaitableStateBase<T> {
    friend class TaskState<T>;

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
        Event* event;
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

    TaskState<>* get_task() noexcept final { return this; }
    EventLoop* get_event_loop() noexcept final {
        return AwaitableStateBase<T>::get_event_loop();
    }
    TaskManagerState<>* get_task_manager() noexcept final { return task_manager_; }

    bool unstarted() const noexcept final {
        return std::holds_alternative<std::monostate>(state_);
    }
    bool cancelled() const noexcept final { return cancelled_ > 0; }
    bool done() const noexcept final { return AwaitableStateBase<T>::done(); }
    std::exception_ptr exception() const noexcept final {
        return AwaitableStateBase<T>::exception();
    }

    void cancel() noexcept final {
        if (done()) {
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
            AwaitableStateBase<T>::get_event_loop()->acquire().schedule_back(
                &std::get<Scheduled>(state_)
            );
        }
        // Not done, unstarted, or pending? Already scheduled, we will steal its place in
        // line.
    }
    void uncancel() final {
        if (done()) {
            return;
        }
        if (cancelled_ == 0) {
            throw std::runtime_error("Task is not cancelled");
        }
        cancelled_--;
    }

    void start_soon(detail::TaskManagerState<>* tm) final;

    void inc_ref() noexcept final { ++ref_count_; }
    void dec_ref() noexcept final {
        if (--ref_count_ == 0) {
            auto handle = std::coroutine_handle<TaskState<T>>::from_promise(
                *static_cast<TaskState<T>*>(this)
            );
            handle.destroy();
        }
    }

  private:
    void on_done() noexcept {
        state_ = std::monostate{};
        if (task_manager_) {
            task_manager_->internal_child_done(static_cast<TaskState<T>*>(this));
        }
    }

    void on_resume() final {
        current_task = this;
        state_ = Running{};
    }

    void set_pending(Event* future_awaiter) final { state_ = Pending{future_awaiter}; }

    std::variant<std::monostate, Scheduled, Pending, Running> state_;
    detail::TaskManagerState<>* task_manager_ = nullptr;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

template <typename T>
class TaskState final : public TaskStateBase<T> {
  public:
    void return_value(T value) {
        this->set_result(Result<T>{std::move(value)});
        this->on_done();
    }
};

template <>
class TaskState<void> final : public TaskStateBase<void> {
  public:
    void return_void() {
        this->set_result(Result<void>{});
        this->on_done();
    }
};

template <typename T>
Task<T> wrap_impl(Coro<T>&& coro) {
    co_return co_await std::move(coro);
}

}  // namespace detail

template <typename T>
class Task {
  public:
    ~Task() { handle_->dec_ref(); }

    Task(Task const& other) : handle_(other.handle_) { handle_->inc_ref(); }

    Task(Coro<T> coro) : Task(std::move(detail::wrap_impl(std::move(coro)))) {}

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
        handle_->add_done_callback(std::forward<F>(callback));
    }

    bool unstarted() const noexcept { return handle_->unstarted(); }
    bool done() const noexcept { return handle_->done(); }
    bool cancelled() const noexcept { return handle_->cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }
    T result() { return handle_->result(); }

    void start_soon();
    void start_soon(TaskManager<>& tm);

    void cancel() noexcept { handle_->cancel(); }
    void uncancel() { handle_->uncancel(); }

    detail::TaskState<T>* get_state() const noexcept { return handle_; }
    static Task<T> from_state(detail::TaskState<T>* state) { return Task<T>{state}; }

    auto operator co_await() {
        return detail::AwaitableAwaiter<detail::TaskState<T>>(handle_);
    }

  private:
    explicit Task(detail::TaskState<T>* s) : handle_(s) { handle_->inc_ref(); }

    detail::TaskState<T>* handle_;
};

namespace detail {

template <typename T>
class TaskManagerState : public TaskManagerState<Erased>, public AwaitableStateBase<T> {
    // TaskManagers have a couple states:
    // - open: tasks can be added.
    // - closed: no more tasks can be added, existing tasks are being waited until they
    //   complete.
    // - done: all tasks have completed, the manager has a result or exception.
    //
    // Managers can be cancelled in either the open or closed state. Cancellation implies
    // closed(), and like Tasks having to run again to throw CancelledError, a cancelled
    // TaskManager does not have a result until later.
  public:
    void inc_ref() noexcept final { ++ref_count_; }
    void dec_ref() noexcept final {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    void add(Task<>& task) final {
        if (done()) {
            throw std::runtime_error("Cannot add task to done TaskManager");
        }
        if (closed()) {
            throw std::runtime_error("Cannot add task to closed TaskManager");
        }
        if (task.unstarted()) {
            task.get_state()->start_soon(this);
        }
        task.get_state()->inc_ref();
        tasks_.push_back(task.get_state());
        on_add(task.get_state());
    }

    void close() noexcept final {
        if (done() || closed()) {
            return;
        }
        closed_ = true;
        for (auto& task : tasks_) {
            task.cancel();
        }
    }
    bool closed() const noexcept final { return closed_; }
    void reopen() noexcept final {
        if (done()) {
            return;
        }
        closed_ = false;
    }

    void cancel() noexcept final {
        if (done() || cancelled()) {
            return;
        }
        cancelled_ = true;
        for (auto& task : tasks_) {
            task.cancel();
        }
    }

    EventLoop* get_event_loop() noexcept final {
        return AwaitableStateBase<T>::get_event_loop();
    }
    void set_event_loop(EventLoop* loop) final {
        AwaitableStateBase<T>::set_event_loop(loop);
    }

    bool done() const noexcept final { return AwaitableStateBase<T>::done(); }
    bool cancelled() const noexcept final { return cancelled_; }

  protected:
    // Hook 1: called after a task has been added to tasks_.
    virtual void on_add(TaskState<>* task) = 0;

    // Hook 2: called after each child completes and has been removed from tasks_.
    // Override to decide whether to call close(), inspect the completed task's outcome,
    // etc.
    virtual void on_child_done(TaskState<>* task) = 0;

    // Hook 3: called exactly once after tasks_ drains. Return the manager's outcome.
    virtual Outcome<T> on_drain_complete() = 0;

    IntrusiveDeque<TaskState<>> tasks_;

  private:
    void internal_child_done(TaskState<>* task) final {
        task->remove_child();
        task->dec_ref();
        on_child_done(task);
        if (!closed() && tasks_.empty()) {
            close();
        }
        if (closed() && tasks_.empty() && !done()) {
            Outcome<T> outcome = on_drain_complete();
            if (outcome.has_exception()) {
                this->set_exception(outcome.exception());
            } else {
                if constexpr (std::is_void_v<T>) {
                    this->set_result(Result<void>{});
                } else {
                    this->set_result(Result<T>{outcome.value()});
                }
            }
        }
    }

    size_t ref_count_{0};
    bool cancelled_ = false;
    bool closed_ = false;
};

}  // namespace detail

template <typename StateT>
class TaskManager {
    static_assert(
        std::is_base_of_v<detail::TaskManagerState<>, StateT>,
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
        state_->add_done_callback(std::forward<F>(callback));
    }

    void add(Task<>& task) { state_->add(task); }

    bool done() const noexcept { return state_->done(); }
    bool cancelled() const noexcept { return state_->cancelled(); }
    auto result() const { return state_->result(); }
    std::exception_ptr exception() const noexcept { return state_->exception(); }

    void cancel() noexcept { state_->cancel(); }

    auto operator co_await() { return detail::AwaitableAwaiter<StateT>(state_); }

    StateT* get_state() const noexcept { return state_; }
    static TaskManager from_state(StateT* state) { return TaskManager{state}; }

  private:
    explicit TaskManager(StateT* state) : state_(state) { state_->inc_ref(); }

    StateT* state_;
};

namespace detail {

template <typename T>
void TaskStateBase<T>::start_soon(TaskManagerState<>* tm) {
    if (!unstarted()) {
        throw std::runtime_error("Task already started");
    }
    task_manager_ = tm;
    auto event_loop = tm->get_event_loop();
    AwaitableStateBase<T>::set_event_loop(event_loop);
    state_ = Scheduled{*this};
    event_loop->acquire().schedule_back(&std::get<Scheduled>(state_));
}

}  // namespace detail

Task<> current_task() {
    if (!detail::current_task) {
        throw std::runtime_error("No current task");
    }
    return Task<>::from_state(detail::current_task);
}

template <typename T>
void Task<T>::start_soon() {
    if (!detail::current_task) {
        throw std::runtime_error("No current task");
    }
    handle_->start_soon(detail::current_task->get_task_manager());
}

template <typename T>
void Task<T>::start_soon(TaskManager<>& tm) {
    handle_->start_soon(tm.get_state());
}

template <typename T>
T run(Task<T> task);

}  // namespace coconext

#endif  // COCONEXT_SCHEDULER_HPP

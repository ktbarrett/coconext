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
class FutureStateBase;

template <typename T>
class FutureState;

}  // namespace detail

template <typename T, typename StateT = detail::FutureState<T>>
class Future;

template <typename T = detail::Erased>
class Task;

template <typename T>
class Coro;

template <typename StateT>
class TaskManager;

namespace detail {

template <typename T = Erased>
class TaskManagerState;

template <typename T = Erased>
class TaskState;

template <typename T, typename StateT>
class FutureAwaiter;

class TaskManagerSharedState : public IntrusiveDequeNode {
    friend class TaskManagerState<>;

  private:
    void remove_child() { deque_remove(); }
    using IntrusiveDequeNode::deque_remove;
};

template <>
class TaskState<Erased> : public TaskManagerSharedState {
    template <typename, typename>
    friend class FutureAwaiter;

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
    virtual void on_await(FutureState<Erased>* future_awaiter) = 0;
    virtual void on_resume() = 0;
};

// inline thread_local means this variable is included in the user's code, and lookups are
// fast (Local-Exec mode).
inline thread_local TaskState<>* current_task = nullptr;

template <typename T>
class FutureStateBase {
    friend class FutureState<T>;
    template <typename, typename>
    friend class FutureAwaiter;

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
    void add_done_callback(F&& callback) {
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

    void bind_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Future is already bound to another EventLoop");
        }
    }

  protected:
    // This method is intended to be overridden by subclasses for behavior when the Future
    // is awaited.
    virtual void on_await(TaskState<>* task) {}

  private:
    void on_done() noexcept {
        Future<T> future =
            Future<T>::from_state(static_cast<detail::FutureState<T>*>(this));
        for (auto& callback : callbacks_) {
            callback(future);
        }
        event_loop_->acquire().schedule_all_back(std::move(deque_));
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
    void set_result(T&& value) noexcept {
        FutureStateBase<T>::set_result(Result<T>{std::move(value)});
    }
    void set_result(T const& value) noexcept {
        FutureStateBase<T>::set_result(Result<T>{value});
    }
};

template <>
class FutureState<void> : public FutureStateBase<void> {
  public:
    void set_void() noexcept { set_result(Result<void>{}); }
};

template <typename T, typename StateT>
class FutureAwaiter : Event {
    friend class Future<T, StateT>;

  public:
    bool await_ready() const noexcept { return future_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) {
        parent_ = h;
        task_ = h.promise().get_task();
        task_->on_await(this);
        future_->on_await(task_);
        future_->bind_event_loop(task_->get_event_loop());
        future_->deque_.push_back(this);
    }
    T await_resume() {
        if (task_->cancelled()) {
            throw coconext::Cancelled{};
        }
        return future_->result();
    }

    ~FutureAwaiter() { event_unschedule(); }

  private:
    explicit FutureAwaiter(StateT* future) : future_(future) {}

    void event_run() override {
        task_->on_resume();
        parent_.resume();
    }

    StateT* future_;
    std::coroutine_handle<> parent_;
    TaskState<>* task_;
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

    auto operator co_await() { return detail::FutureAwaiter<T, StateT>(handle_); }

    static Future from_state(StateT* state) { return Future{state}; }
    StateT* get_state() const noexcept { return handle_; }

  private:
    explicit Future(StateT* state) : handle_(state) { handle_->inc_ref(); }

    StateT* handle_;
};

namespace detail {

template <typename T>
class TaskStateBase : public TaskState<Erased> {
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

        TaskStateBase<Erased>* task_;
    };

    struct Pending {
        Event* event;
    };

    struct Running {};

  public:
    class DoneFutureState : public FutureState<void> {
        friend class TaskStateBase<T>;

      public:
        void on_await(TaskState<>* task) override {
            owner_->bind_event_loop(task->get_event_loop());
        }

      private:
        explicit DoneFutureState(TaskStateBase<T>* owner) noexcept : owner_(owner) {}

        TaskStateBase<T>* owner_;
    };

    using DoneFuture = Future<void, DoneFutureState>;

    Task<T> get_return_object() { return Task<T>{static_cast<TaskState<T>*>(this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { set_outcome(Exception{std::current_exception()}); }

    TaskState<>* get_task() noexcept override { return this; }
    EventLoop* get_event_loop() noexcept override { return event_loop_; }
    TaskManagerState<>* get_task_manager() noexcept override { return task_manager_; }

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
        return std::holds_alternative<Result<T>>(state_)
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

    void start_soon(detail::TaskManagerState<>* tm) override;

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

    ~TaskStateBase() {
        if (wait_complete_future_) {
            wait_complete_future_->owner_ = nullptr;
            wait_complete_future_->dec_ref();
        }
    }

    void bind_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Task is already bound to another EventLoop");
        }
    }

  private:
    template <typename V>
    void set_outcome(V&& value) noexcept {
        state_ = std::forward<V>(value);
        // TODO: handle cancelled_ > 0
        on_done();
    }

    void on_done();

    void on_resume() override {
        current_task = this;
        state_ = Running{};
    }

    DoneFuture wait_complete() const noexcept {
        if (!wait_complete_future_) {
            wait_complete_future_ =
                new DoneFutureState{const_cast<TaskStateBase<T>*>(this)};
            wait_complete_future_->inc_ref();
            if (done()) {
                wait_complete_future_->set_void();
            }
        }
        return DoneFuture::from_state(wait_complete_future_);
    }

    void on_await(FutureState<Erased>* future_awaiter) override {
        state_ = Pending{future_awaiter};
    }

    std::variant<std::monostate, Scheduled, Pending, Running, Result<T>, Exception> state_;
    std::vector<std::function<void(Task<T>&)>> callbacks_;
    mutable DoneFutureState* wait_complete_future_ = nullptr;
    detail::TaskManagerState<>* task_manager_ = nullptr;
    EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

template <typename T>
class TaskState : public TaskStateBase<T> {
  public:
    void return_value(T value) { set_outcome(Result<T>{std::move(value)}); }
};

template <>
class TaskState<void> : public TaskStateBase<void> {
  public:
    void return_void() { set_outcome(Result<void>{}); }
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
    template <typename StateT>
    void start_soon(TaskManager<StateT>& loop) {
        handle_->start_soon(loop.get_state());
    }

    void cancel() noexcept { handle_->cancel(); }

    detail::TaskState<T>* get_state() const noexcept { return handle_; }
    static Task<T> from_state(detail::TaskState<T>* state) { return Task<T>{state}; }

    typename detail::TaskStateBase<T>::DoneFuture wait_complete() const noexcept;

    Coro<T> wait_result() {
        co_await wait_complete();
        co_return result();
    }

  private:
    explicit Task(detail::TaskState<T>* s) : handle_(s) { handle_->inc_ref(); }

    detail::TaskState<T>* handle_;
};

template <typename T>
T run(Task<T> task);

namespace detail {

// Pure-virtual untyped interface. All state and impl live in TaskManagerState<T>.
// Mirrors the TaskState<Erased> / TaskStateBase<T> split.
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
    virtual void bind_event_loop(EventLoop* loop) = 0;

    virtual bool done() const noexcept = 0;
    virtual bool cancelled() const noexcept = 0;

  private:
    virtual void internal_child_done(TaskState<>* task) = 0;
};

template <typename T>
class TaskManagerState : public TaskManagerState<Erased> {
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
    class DoneFutureState : public FutureState<T> {
        friend class TaskManagerState<T>;

      public:
        void on_await(TaskState<>* task) override {
            owner_->bind_event_loop(task->get_event_loop());
        }

      private:
        explicit DoneFutureState(TaskManagerState<T>* owner) noexcept : owner_(owner) {}

        TaskManagerState<T>* owner_;
    };

    using DoneFuture = Future<T, DoneFutureState>;

    TaskManagerState() : wait_complete_state_(new DoneFutureState{this}) {
        wait_complete_state_->inc_ref();
    }

    ~TaskManagerState() override {
        wait_complete_state_->owner_ = nullptr;
        wait_complete_state_->dec_ref();
    }

    void inc_ref() noexcept override { ++ref_count_; }
    void dec_ref() noexcept override {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    void add(Task<>& task) override {
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
    }

    void close() noexcept override {
        if (done() || closed()) {
            return;
        }
        closed_ = true;
        // cancel remaining tasks
        for (auto& task : tasks_) {
            task.cancel();
        }
    }
    bool closed() const noexcept override { return closed_; }

    void cancel() noexcept override {
        if (done() || cancelled()) {
            return;
        }
        cancelled_ = true;
        for (auto& task : tasks_) {
            task.cancel();
        }
    }

    EventLoop* get_event_loop() noexcept override { return event_loop_; }
    void bind_event_loop(EventLoop* loop) override {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("TaskManager is already bound to another EventLoop");
        }
    }

    bool done() const noexcept override { return done_; }
    bool cancelled() const noexcept override { return cancelled_; }

    DoneFuture wait_complete() const noexcept {
        return DoneFuture::from_state(wait_complete_state_);
    }

    T result() const { return wait_complete_state_->result(); }
    std::exception_ptr exception() const noexcept {
        return wait_complete_state_->exception();
    }

    void add_done_callback(std::function<void(TaskManager<T>&)> callback) {
        if (done()) {
            throw std::runtime_error("TaskManager is already done, cannot add callback");
        }
        callbacks_.push_back(std::move(callback));
    }

  protected:
    // Hook 1: called after each child completes and has been removed from tasks_. Override
    // to decide whether to call close(), inspect the completed task's outcome, etc.
    virtual void on_child_done(TaskState<>* task) = 0;

    // Hook 2: called exactly once after tasks_ drains. Return the manager's outcome.
    virtual Outcome<T> on_drain_complete() = 0;

  private:
    void internal_child_done(TaskState<>* task) override {
        task->remove_child();
        task->dec_ref();
        on_child_done(task);
        if (!closed() && tasks_.empty()) {
            close();
        }
        if (closed() && tasks_.empty() && !done()) {
            done_ = true;
            Outcome<T> outcome = on_drain_complete();
            if (outcome.has_exception()) {
                wait_complete_state_->set_exception(outcome.exception());
            } else {
                if constexpr (std::is_void_v<T>) {
                    wait_complete_state_->set_void();
                } else {
                    wait_complete_state_->set_result(outcome.value());
                }
            }
            auto tm_handle = TaskManager<T>::from_state(this);
            for (auto& callback : callbacks_) {
                callback(tm_handle);
            }
        }
    }

  protected:
    IntrusiveDeque<TaskState<>> tasks_;

  private:
    std::vector<std::function<void(TaskManager<T>&)>> callbacks_;
    DoneFutureState* wait_complete_state_;
    EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
    bool cancelled_ = false;
    bool closed_ = false;
    bool done_ = false;
};

}  // namespace detail

template <typename StateT = detail::TaskManagerState<>>
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

    void cancel() noexcept { state_->cancel(); }

    auto result() const { return state_->result(); }
    std::exception_ptr exception() const noexcept { return state_->exception(); }

    auto wait_complete() const noexcept { return state_->wait_complete(); }
    auto operator co_await() { return state_->wait_complete().operator co_await(); }

    StateT* get_state() const noexcept { return state_; }
    static TaskManager from_state(StateT* state) { return TaskManager{state}; }

  private:
    explicit TaskManager(StateT* state) : state_(state) { state_->inc_ref(); }

    StateT* state_;
};

namespace detail {

template <typename T>
void TaskStateBase<T>::on_done() {
    auto task = Task<T>::from_state(static_cast<TaskState<T>*>(this));
    for (auto& callback : callbacks_) {
        callback(task);
    }
    task_manager_->internal_child_done(static_cast<TaskState<T>*>(this));
    if (wait_complete_future_) {
        wait_complete_future_->set_void();
    }
}

template <typename T>
void TaskStateBase<T>::start_soon(TaskManagerState<>* tm) {
    if (!unstarted()) {
        throw std::runtime_error("Task already started");
    }
    task_manager_ = tm;
    bind_event_loop(tm->get_event_loop());
    state_ = Scheduled{*this};
    event_loop_->acquire().schedule_back(&std::get<Scheduled>(state_));
}

}  // namespace detail

template <typename T>
typename detail::TaskStateBase<T>::DoneFuture Task<T>::wait_complete() const noexcept {
    return handle_->wait_complete();
}

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

}  // namespace coconext

#endif  // COCONEXT_SCHEDULER_HPP

#ifndef COCONEXT_SCHEDULER_HPP
#define COCONEXT_SCHEDULER_HPP

#include <coconext/event_loop.hpp>
#include <coconext/intrusive_deque.hpp>
#include <coconext/not_null.hpp>
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

template <typename T>
class RunTaskManager;

}  // namespace detail

template <typename T>
class FutureState;

template <typename T = detail::Erased>
class TaskManagerState;

template <typename T = detail::Erased>
class TaskState;

template <typename StateT>
class Future;

template <typename T>
class Task;

template <typename T>
class Coro;

template <typename StateT>
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
    friend class ::coconext::Future;
    template <typename>
    friend class ::coconext::Task;
    template <typename>
    friend class ::coconext::TaskManager;

    [[nodiscard]] explicit AwaitableAwaiter(not_null<AwaitableStateT*> awaitable)
        : awaitable_(awaitable) {}

    void event_run() noexcept override;

    not_null<AwaitableStateT*> awaitable_;
    std::coroutine_handle<> parent_ = nullptr;
    TaskState<>* task_ = nullptr;
};

}  // namespace detail

template <typename T>
class FutureState {
    template <typename>
    friend class Future;
    template <typename>
    friend class detail::AwaitableAwaiter;

  public:
    using value_type = T;

    virtual ~FutureState() {
        if (!done()) {
            unprime();
        }
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

    [[nodiscard]] detail::EventLoop* get_event_loop() const noexcept { return event_loop_; }

  protected:
    // Override to unprime any underlying awaitable if refcount drops to zero.
    virtual void unprime() noexcept {}

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

    void set_exception(std::exception_ptr exc) noexcept {
        assert(exc);
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
        assert(event_loop_ != nullptr);
        event_loop_->acquire().schedule_all_back(std::move(waiters_));
    }

    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            unprime();
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
class Future {
    static_assert(
        std::is_base_of_v<FutureState<typename StateT::value_type>, StateT>,
        "Future's StateT must derive from FutureState<T>"
    );

  public:
    using value_type = typename StateT::value_type;

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
    [[nodiscard]] explicit Future(not_null<StateT*> state) noexcept : handle_(state) {
        handle_->inc_ref();
    }

  private:
    not_null<StateT*> handle_;
};

template <typename T>
T run(Task<T> task);

namespace detail {

template <typename T>
class TaskStateBase;

}

template <>
class TaskState<detail::Erased> : public detail::IntrusiveDequeNode {
    template <typename>
    friend class Task;
    template <typename>
    friend class TaskManagerState;
    template <typename>
    friend class detail::AwaitableAwaiter;
    template <typename U>
    friend U run(Task<U>);
    template <typename>
    friend class Coro;
    template <typename>
    friend class detail::TaskStateBase;
    template <typename>
    friend class FutureState;
    friend class TaskContext;

    struct Scheduled : detail::Event {
        [[nodiscard]] explicit Scheduled(not_null<TaskState<>*> task) : task_(task) {}

        void event_run() noexcept override {
            // on_resume() re-emplaces state_, which destroys *this. Copy the fields
            // we need onto the stack before that happens.
            auto task = task_;
            task->on_resume();
            task->handle_.resume();
        }

        not_null<TaskState<>*> task_;
    };

    struct Pending {
        [[nodiscard]] explicit Pending(not_null<detail::Event*> event) : event(event) {}

        not_null<detail::Event*> event;
    };

    struct Running {};

  public:
    using value_type = detail::Erased;

    [[nodiscard]] bool started() const noexcept {
        return !std::holds_alternative<std::monostate>(state_);
    }
    [[nodiscard]] bool running() const noexcept {
        return std::holds_alternative<Running>(state_);
    }
    [[nodiscard]] bool cancelled() const noexcept { return cancelled_ > 0; }

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

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

    void cancel() {
        if (done()) {
            return;
        }
        if (running()) {
            throw Cancelled{};
        }
        cancelled_++;
        if (!started()) {
            set_exception(std::make_exception_ptr(Cancelled{}));
            return;
        } else if (std::holds_alternative<Pending>(state_)) {
            auto& pending = std::get<Pending>(state_);
            pending.event->event_unschedule();
            state_ = Scheduled{this};
            event_loop_->acquire().schedule_back(&std::get<Scheduled>(state_));
        }
    }

    void uncancel() {
        if (done()) {
            return;
        }
        if (cancelled_ == 0) {
            throw std::runtime_error("Task is not cancelled");
        }
        cancelled_--;
    }

    void start_soon();
    void start_soon(TaskContext const& ctxt);

  protected:
    void set_exception(std::exception_ptr exc) noexcept {
        assert(exc);
        result_ = exc;
        on_done();
    }

    void mark_value() noexcept {
        result_ = detail::Value<detail::Erased>{};
        on_done();
    }

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            handle_.destroy();
        }
    }

    [[nodiscard]] not_null<TaskState<>*> get_task() noexcept { return this; }

    [[nodiscard]] detail::EventLoop* get_event_loop() const noexcept { return event_loop_; }
    [[nodiscard]] TaskManagerState<>* get_task_manager() const noexcept {
        return task_manager_;
    }
    [[nodiscard]] TaskManagerState<>* get_global_task_manager() const noexcept {
        return global_task_manager_;
    }

    void start_soon(
        not_null<detail::EventLoop*> loop, not_null<TaskManagerState<>*> task_manager
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

        state_ = Scheduled{this};
        event_loop_->acquire().schedule_back(&std::get<TaskState<>::Scheduled>(state_));
    }

    void register_waiter(not_null<detail::Event*> awaiter) noexcept {
        waiters_.push_back(awaiter);
    }

    // Callback called when a Task/Coro awaits on this object, we bind to the caller's
    // EventLoop and global TaskManager if not already bound.
    void on_awaited(not_null<TaskState<>*> awaiter) {
        if (started()) {
            return;
        }
        start_soon(awaiter->get_event_loop(), awaiter->get_global_task_manager());
    }

    void on_done() noexcept;

    void on_resume() noexcept {
        detail::current_task = this;
        state_ = Running{};
    }

    void on_awaiting(not_null<detail::Event*> awaiter) noexcept {
        state_ = Pending{awaiter};
    }

  private:
    std::variant<std::monostate, Scheduled, Pending, Running> state_;
    std::variant<std::monostate, std::exception_ptr, detail::Value<detail::Erased>> result_;
    detail::IntrusiveDeque<detail::Event> waiters_;
    std::vector<std::function<void()>> callbacks_;
    detail::EventLoop* event_loop_ = nullptr;
    TaskManagerState<>* task_manager_ = nullptr;
    TaskManagerState<>* global_task_manager_ = nullptr;
    std::coroutine_handle<> handle_;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
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
        TaskState<>::set_exception(std::current_exception());
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
        if (auto exc = TaskState<>::exception()) {
            std::rethrow_exception(exc);
        }
    }
};

template <typename T>
class Task {
  public:
    using value_type = T;
    using promise_type = TaskState<T>;

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

    [[nodiscard]] bool started() const noexcept { return handle_->started(); }
    [[nodiscard]] bool done() const noexcept { return handle_->done(); }
    [[nodiscard]] bool cancelled() const noexcept { return handle_->cancelled(); }
    [[nodiscard]] std::exception_ptr exception() const { return handle_->exception(); }
    [[nodiscard]] T result() const { return handle_->result(); }

    void start_soon() { handle_->start_soon(); }
    void start_soon(TaskContext const& ctxt) { handle_->start_soon(ctxt); }

    void cancel() noexcept { handle_->cancel(); }
    void uncancel() { handle_->uncancel(); }

    [[nodiscard]] not_null<TaskState<T>*> get_state() const noexcept { return handle_; }
    [[nodiscard]] explicit Task(not_null<TaskState<T>*> s) noexcept : handle_(s) {
        handle_->inc_ref();
    }

    [[nodiscard]] auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<TaskState<T>>(handle_);
    }

  private:
    [[nodiscard]] static Task<T> wrap_impl(Coro<T> coro) {
        co_return co_await std::move(coro);
    }

    not_null<TaskState<T>*> handle_;
};

template <>
class TaskManagerState<detail::Erased> {
    friend class TaskState<>;
    template <typename>
    friend class TaskManager;
    template <typename>
    friend class detail::AwaitableAwaiter;
    template <typename>
    friend class detail::RunTaskManager;

  public:
    using value_type = detail::Erased;

    virtual ~TaskManagerState() = default;

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

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

    [[nodiscard]] detail::EventLoop* get_event_loop() const noexcept { return event_loop_; }

    void add(not_null<TaskState<>*> task) {
        if (done()) {
            throw std::runtime_error("Cannot add task to done TaskManager");
        }
        if (closed()) {
            throw std::runtime_error("Cannot add task to closed TaskManager");
        }
        if (task->task_manager_) {
            throw std::runtime_error("Task is already bound to a TaskManager");
        }
        // Loop affinity: manager, task body, and all sibling tasks must share one
        // EventLoop. Bind whichever side is already bound to the other; throws on
        // mismatch. If neither is bound, start_soon() below binds all the children later
        if (event_loop_ == nullptr && task->started()) {
            // We know the child task is started and can use its bindings.
            auto event_loop = task->get_event_loop();
            assert(event_loop != nullptr && "Running Task must have an EventLoop bound");
            if (event_loop_ == nullptr) {
                event_loop_ = event_loop;
            } else if (event_loop_ != event_loop) {
                throw std::runtime_error(
                    "TaskManager is already bound to another EventLoop"
                );
            }
        }
        if (global_task_manager_ == nullptr && task->started()) {
            // We know the child task is started and can use its bindings.
            auto global_task_manager = task->get_global_task_manager();
            assert(
                global_task_manager != nullptr
                && "Running Task must have a global TaskManager bound"
            );
            if (global_task_manager_ == nullptr) {
                global_task_manager_ = global_task_manager;
            }
        }
        if (started() && !task->started()) {
            assert(
                event_loop_ != nullptr
                && "Running TaskManager must be bound to an EventLoop"
            );
            assert(
                global_task_manager_ != nullptr
                && "Running TaskManager must be bound to a global TaskManager"
            );
            task->start_soon(event_loop_, global_task_manager_);
        }
        task->task_manager_ = this;
        task->inc_ref();
        tasks_.push_back(task);
        on_add(task);
    }

    void start_soon();
    void start_soon(TaskContext const& ctxt);

    [[nodiscard]] TaskManagerState<>* get_global_task_manager() const noexcept {
        return global_task_manager_;
    }

    [[nodiscard]] bool started() const noexcept { return started_; }

    void close() noexcept {
        if (done() || closed()) {
            return;
        }
        closed_ = true;
    }
    [[nodiscard]] bool closed() const noexcept { return closed_; }

    void cancel() noexcept {
        if (done() || cancelled()) {
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
    virtual void on_add(not_null<TaskState<>*> task) noexcept = 0;

    // Hook 2: called after each child completes and has been removed from tasks_.
    virtual void on_child_done(not_null<TaskState<>*> task) noexcept = 0;

    // Hook 3: called exactly once after tasks_ drains. Users call set_result() or
    // set_exception() to set the result of the TaskManager.
    virtual void on_drain_complete() noexcept = 0;

    void set_exception(std::exception_ptr exc) noexcept {
        assert(exc);
        result_ = exc;
        on_done();
    }

    void mark_value() noexcept {
        result_ = detail::Value<detail::Erased>{};
        on_done();
    }

  private:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    void start_soon(
        not_null<detail::EventLoop*> loop, not_null<TaskManagerState<>*> global_task_manager
    ) {
        assert(!started() && "TaskManager is already started");
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("TaskManager is already bound to another EventLoop");
        }
        if (global_task_manager_ == nullptr) {
            global_task_manager_ = global_task_manager;
        }
        started_ = true;
        for (auto it = tasks_.begin(); it != tasks_.end();) {
            auto& task = *it++;
            task.start_soon(event_loop_, global_task_manager_);
        }
    }

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
        // If there is are no awaiters, we don't care if there is no event loop.
    }

    // Called when a child Task completes. It removes the child from tasks_, decrements its
    // refcount, and calls the customizable on_child_done(). Then after all children are
    // done if we are closed, calls on_drain_complete().
    void internal_child_done(not_null<TaskState<>*> task) noexcept {
        task->deque_remove();
        task->dec_ref();
        on_child_done(task);
        if (closed() && tasks_.empty()) {
            assert(!done());
            on_drain_complete();
            assert(
                done() && "on_drain_complete() must call set_result() or set_exception()"
            );
        }
    }

    // Callback called when a Task/Coro awaits on this object, we bind to the caller's
    // EventLoop and global TaskManager if not already bound.
    void on_awaited(not_null<TaskState<>*> task) {
        if (started()) {
            return;
        }
        start_soon(task->get_event_loop(), task->get_global_task_manager());
    }

  protected:
    detail::IntrusiveDeque<TaskState<>> tasks_;

  private:
    detail::IntrusiveDeque<detail::Event> waiters_;
    std::vector<std::function<void()>> callbacks_;
    detail::EventLoop* event_loop_ = nullptr;
    TaskManagerState<>* global_task_manager_ = nullptr;
    std::variant<std::monostate, std::exception_ptr, detail::Value<detail::Erased>> result_;
    size_t ref_count_{0};
    bool started_ = false;
    bool cancelled_ = false;
    bool closed_ = false;
};

template <typename T>
class TaskManagerState : public TaskManagerState<> {
  public:
    using value_type = T;

    [[nodiscard]] T result() const {
        if (!TaskManagerState<>::done()) {
            throw std::runtime_error("Not done");
        }
        if (auto exc = TaskManagerState<>::exception()) {
            std::rethrow_exception(exc);
        }
        if constexpr (!std::is_void_v<T>) {
            return *value_;
        }
    }

  protected:
    template <typename U>
        requires(std::is_convertible_v<U, T>)
    void set_result(U&& value) noexcept {
        value_ = std::forward<U>(value);
        TaskManagerState<>::mark_value();
    }

  private:
    std::optional<T> value_;
};

template <>
class TaskManagerState<void> : public TaskManagerState<> {
  public:
    using value_type = void;

    void result() const {
        if (!TaskManagerState<>::done()) {
            throw std::runtime_error("Not done");
        }
        if (auto exc = TaskManagerState<>::exception()) {
            std::rethrow_exception(exc);
        }
    }

    void set_void() noexcept { TaskManagerState<>::mark_value(); }
};

template <typename StateT>
class TaskManager {
    static_assert(
        std::is_base_of_v<TaskManagerState<typename StateT::value_type>, StateT>,
        "TaskManager's StateT must derive from TaskManagerState<T>"
    );

  public:
    using value_type = typename StateT::value_type;

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
    [[nodiscard]] value_type result() const { return state_->result(); }
    [[nodiscard]] std::exception_ptr exception() const { return state_->exception(); }

    void add(not_null<TaskState<>*> task) { state_->add(task); }

    void cancel() noexcept { state_->cancel(); }

    [[nodiscard]] auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<StateT>(state_);
    }

    [[nodiscard]] not_null<StateT*> get_state() const noexcept { return state_; }
    [[nodiscard]] explicit TaskManager(not_null<StateT*> state) noexcept : state_(state) {
        state_->inc_ref();
    }

  private:
    not_null<StateT*> state_;
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
    [[nodiscard]] not_null<TaskManagerState<>*> get_global_task_manager() const {
        auto gtm = get_task()->get_global_task_manager();
        assert(gtm != nullptr && "Running Task must have a global TaskManager bound");
        return gtm;
    }
    [[nodiscard]] not_null<detail::EventLoop*> get_event_loop() const {
        auto loop = get_task()->get_event_loop();
        assert(loop != nullptr && "Running Task must have an EventLoop bound");
        return loop;
    }
    [[nodiscard]] TaskManagerState<>* get_task_manager() const {
        return get_task()->get_task_manager();
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
    task->on_awaiting(this);
    task_ = task;
    awaitable_->on_awaited(task);
    awaitable_->register_waiter(this);
}

template <typename S>
typename detail::AwaitableAwaiter<S>::value_type detail::AwaitableAwaiter<
    S>::await_resume() {
    // task_ is only set if await_suspend ran. If await_ready short-circuited on an
    // already-done awaitable, we skip the cancellation check as there was no possible
    // suspension point where the Task could become cancelled. The one caveat is a
    // self-cancellation, which is handled by throwing in a call to cancel();
    if (task_ != nullptr && task_->cancelled()) {
        throw coconext::Cancelled{};
    }
    return awaitable_->result();
}

template <typename S>
void detail::AwaitableAwaiter<S>::event_run() noexcept {
    assert(parent_ != nullptr);
    assert(task_ != nullptr);
    task_->on_resume();
    parent_.resume();
}

template <typename T>
void FutureState<T>::on_awaited(not_null<TaskState<>*> task) {
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
    assert(event_loop_ != nullptr);
    event_loop_->acquire().schedule_all_back(std::move(waiters_));
    if (task_manager_) {
        task_manager_->internal_child_done(this);
    }
}

inline void TaskState<>::start_soon(TaskContext const& ctxt) {
    if (started()) {
        throw std::runtime_error("TaskManager is already started");
    }
    start_soon(ctxt.get_event_loop(), ctxt.get_global_task_manager());
}

inline void TaskState<>::start_soon() {
    if (started()) {
        throw std::runtime_error("TaskManager is already started");
    }
    if (event_loop_ == nullptr || global_task_manager_ == nullptr) {
        start_soon(current_context());
    }
}

inline void TaskManagerState<>::start_soon(TaskContext const& ctxt) {
    if (started()) {
        throw std::runtime_error("TaskManager is already started");
    }
    start_soon(ctxt.get_event_loop(), ctxt.get_global_task_manager());
}

inline void TaskManagerState<>::start_soon() {
    if (started()) {
        throw std::runtime_error("TaskManager is already started");
    }
    if (event_loop_ == nullptr || global_task_manager_ == nullptr) {
        start_soon(current_context());
    }
}

template <typename T>
T run(Task<T> task);

template <typename T>
Task<T> start_soon(Task<T> task) {
    auto ctxt = current_context();
    ctxt.get_global_task_manager()->add(task.get_state());
    task.start_soon(ctxt);
    return task;
}

template <typename T>
Task<T> start_soon(Coro<T> coro) {
    return start_soon(Task<T>{std::move(coro)});
}

}  // namespace coconext

#endif  // COCONEXT_SCHEDULER_HPP

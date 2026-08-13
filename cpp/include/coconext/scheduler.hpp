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

  public:
    using value_type = detail::Erased;

    [[nodiscard]] bool started() const noexcept { return started_; }
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
        } else if (std::holds_alternative<Scheduled>(state_)) {
            inc_ref();
            std::get<Scheduled>(state_).event_unschedule();
            set_exception(std::make_exception_ptr(Cancelled{}));
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
        if (cancelled_ == 0) {
            throw std::runtime_error("Task is not cancelled");
        }
        cancelled_--;
    }

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

    void on_awaiting(not_null<detail::Event*> awaiter) noexcept {
        state_ = Pending{awaiter};
    }

  private:
    std::variant<std::monostate, Scheduled, Pending, Running> state_;
    std::variant<std::monostate, std::exception_ptr, detail::Value<detail::Erased>> result_;
    detail::IntrusiveDeque<detail::Event> waiters_;
    std::vector<std::function<void()>> callbacks_;
    detail::EventLoop* event_loop_ = nullptr;
    TaskManager* task_manager_ = nullptr;
    TaskManager* global_task_manager_ = nullptr;
    std::coroutine_handle<> handle_;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
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

class TaskManager {
    friend class TaskState<>;
    friend class TaskContext;
    template <typename>
    friend class detail::RunTaskManager;

    enum class State {
        Created,
        Open,
        Closed,
        Done,
        Ended
    };

  public:
    class StartAwaiter {
        friend class TaskManager;

      public:
        [[nodiscard]] bool await_ready() const noexcept { return false; }

        template <typename PromiseType>
        bool await_suspend(std::coroutine_handle<PromiseType> parent) {
            auto task = parent.promise().get_task();
            manager_.start_internal(
                task->get_event_loop(), task->get_global_task_manager()
            );
            return false;
        }

        void await_resume() const noexcept {}

      private:
        explicit StartAwaiter(TaskManager& manager) noexcept : manager_(manager) {}

        TaskManager& manager_;
    };

    class JoinAwaiter : private detail::Event {
        friend class TaskManager;

      public:
        ~JoinAwaiter() {
            event_unschedule();
            if (manager_.join_waiter_ == this) {
                manager_.join_waiter_ = nullptr;
                manager_.join_started_ = false;
            }
        }

        [[nodiscard]] bool await_ready() {
            manager_.begin_join();
            return manager_.done();
        }

        template <typename PromiseType>
        void await_suspend(std::coroutine_handle<PromiseType> parent) {
            auto task = parent.promise().get_task();
            if (manager_.event_loop_ != task->get_event_loop()) {
                manager_.join_started_ = false;
                throw std::runtime_error(
                    "TaskManager is already bound to another EventLoop"
                );
            }
            parent_ = parent;
            task->on_awaiting(this);
            task_ = task;
            manager_.join_waiter_ = this;
        }

        void await_resume() {
            manager_.finish_join();
            if (task_ != nullptr && task_->cancelled()) {
                throw Cancelled{};
            }
            if (manager_.exception_) {
                std::rethrow_exception(manager_.exception_);
            }
        }

      private:
        explicit JoinAwaiter(TaskManager& manager) noexcept : manager_(manager) {}

        void event_run() noexcept override {
            assert(task_ != nullptr);
            if (!manager_.done()) {
                assert(task_->cancelled());
                try {
                    manager_.cancel();
                } catch (...) {
                    manager_.set_exception(std::current_exception());
                    manager_.close();
                }
                return;
            }
            auto task = not_null{task_};
            task->inc_ref();
            auto previous_task = detail::current_task;
            task->on_resume();
            parent_.resume();
            detail::current_task = previous_task;
            task->dec_ref();
        }

        TaskManager& manager_;
        std::coroutine_handle<> parent_ = nullptr;
        TaskState<>* task_ = nullptr;
    };

    TaskManager() noexcept = default;
    TaskManager(TaskManager const&) = delete;
    TaskManager& operator=(TaskManager const&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    virtual ~TaskManager() noexcept(false) {
        if (state_ != State::Created && state_ != State::Ended) {
            abandon_children();
            throw std::logic_error("TaskManager destroyed before join() completed");
        }
    }

    [[nodiscard]] StartAwaiter start() & {
        if (state_ != State::Created) {
            throw std::logic_error("TaskManager is already started");
        }
        return StartAwaiter{*this};
    }

    [[nodiscard]] JoinAwaiter join() & { return JoinAwaiter{*this}; }

    template <typename T>
    [[nodiscard]] Task<T> start_soon(Coro<T> coro);

    void close() {
        if (state_ == State::Created) {
            throw std::logic_error("Cannot close an unstarted TaskManager");
        }
        if (state_ != State::Open) {
            return;
        }
        state_ = State::Closed;
        complete_if_ready();
    }

    void cancel() {
        close();
        if (cancelling_) {
            return;
        }
        cancelling_ = true;
        try {
            for (auto it = tasks_.begin(); it != tasks_.end();) {
                auto& task = *it++;
                task.cancel();
            }
        } catch (...) {
            cancelling_ = false;
            throw;
        }
        cancelling_ = false;
    }

    [[nodiscard]] bool started() const noexcept { return state_ != State::Created; }
    [[nodiscard]] bool closed() const noexcept { return state_ == State::Closed || done(); }
    [[nodiscard]] bool done() const noexcept {
        return state_ == State::Done || state_ == State::Ended;
    }
    [[nodiscard]] bool ended() const noexcept { return state_ == State::Ended; }
    [[nodiscard]] bool empty() const noexcept { return tasks_.empty(); }

  protected:
    // Called after a child is linked and scheduled.
    virtual void on_add(not_null<TaskState<>*>) noexcept {}

    // Called after a completed child is unlinked. The default policy closes once no
    // child remains, but specialized managers can close or cancel earlier.
    virtual void on_child_done(not_null<TaskState<>*>) noexcept {
        if (empty()) {
            close();
        }
    }

    // Called exactly once when a closed manager finishes draining.
    virtual void on_done() noexcept {}

    void set_exception(std::exception_ptr exc) noexcept {
        assert(exc);
        if (!exception_) {
            exception_ = exc;
        }
    }

  private:
    void start_internal(
        not_null<detail::EventLoop*> event_loop, not_null<TaskManager*> global_task_manager
    ) {
        if (state_ != State::Created) {
            throw std::logic_error("TaskManager is already started");
        }
        event_loop_ = event_loop;
        global_task_manager_ = global_task_manager;
        state_ = State::Open;
    }

    void begin_join() {
        if (state_ == State::Created) {
            throw std::logic_error("Cannot join an unstarted TaskManager");
        }
        if (join_started_) {
            throw std::logic_error("TaskManager is already being joined");
        }
        join_started_ = true;
    }

    void finish_join() {
        assert(state_ == State::Done);
        state_ = State::Ended;
        join_waiter_ = nullptr;
    }

    void add_and_start(not_null<TaskState<>*> task) {
        if (state_ != State::Open) {
            throw std::logic_error("Cannot start a Task on a non-open TaskManager");
        }
        assert(event_loop_ != nullptr);
        assert(global_task_manager_ != nullptr);
        assert(task->task_manager_ == nullptr);

        task->task_manager_ = this;
        task->inc_ref();
        tasks_.push_back(task);
        task->start_soon(event_loop_, global_task_manager_);
        on_add(task);
    }

    void internal_child_done(not_null<TaskState<>*> task) noexcept {
        task->deque_remove();
        task->task_manager_ = nullptr;
        on_child_done(task);
        complete_if_ready();
        task->dec_ref();
    }

    void complete_if_ready() noexcept {
        if (state_ != State::Closed || !tasks_.empty()) {
            return;
        }
        state_ = State::Done;
        on_done();
        if (join_waiter_ != nullptr) {
            assert(event_loop_ != nullptr);
            event_loop_->acquire().schedule_back(join_waiter_);
        }
    }

    void abandon_children() noexcept {
        while (auto task = tasks_.pop_front()) {
            task->task_manager_ = nullptr;
            if (task->global_task_manager_ == this) {
                task->global_task_manager_ = nullptr;
            }
            try {
                task->cancel();
            } catch (...) {}
            task->dec_ref();
        }
    }

    detail::IntrusiveDeque<TaskState<>> tasks_;
    detail::EventLoop* event_loop_ = nullptr;
    TaskManager* global_task_manager_ = nullptr;
    JoinAwaiter* join_waiter_ = nullptr;
    std::exception_ptr exception_;
    State state_ = State::Created;
    bool join_started_ = false;
    bool cancelling_ = false;
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
    if (task_ != nullptr && task_->cancelled()) {
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
    if (task_manager_) {
        task_manager_->internal_child_done(this);
    }
    if (scheduler_owned_) {
        scheduler_owned_ = false;
        dec_ref();
    }
}

template <typename T>
Task<T> TaskManager::start_soon(Coro<T> coro) {
    Task<T> task{std::move(coro)};
    add_and_start(task.get_state());
    return task;
}

template <typename T>
[[nodiscard]] Task<T> start_soon(Coro<T> coro) {
    return current_context().get_global_task_manager()->start_soon(std::move(coro));
}

}  // namespace coconext

#endif  // COCONEXT_SCHEDULER_HPP

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

template <typename T = detail::Erased>
class Task;

template <typename T>
class Coro;

template <typename T>
class Future;

class TaskManager;

namespace detail {

class TaskManagerState;

template <typename T = Erased>
class TaskState;

template <typename T>
class FutureState;

template <typename T>
class FutureAwaiter;

class TaskManagerSharedState : public IntrusiveDequeNode {
    friend class TaskManagerState;

  private:
    void remove_child() { deque_remove(); }
    using IntrusiveDequeNode::deque_remove;
};

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
class FutureStateBase {
    friend class FutureState<T>;

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
    void add_callback(F&& callback) {
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

  private:
    void on_done() noexcept {
        Future<T> future =
            Future<T>::from_state(static_cast<detail::FutureState<T>*>(this));
        for (auto& callback : callbacks_) {
            callback(future);
        }
        event_loop_->acquire().schedule_all_back(std::move(deque_));
    }

    void bind_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Future is already bound to another EventLoop");
        }
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
    void set_result(T&& value) noexcept { set_result(Result<T>{std::move(value)}); }
    void set_result(T const& value) noexcept { set_result(Result<T>{value}); }
};

template <>
class FutureState<void> : public FutureStateBase<void> {
  public:
    void set_void() noexcept { set_result(Result<void>{}); }
};

template <typename T>
class FutureAwaiter : Event {
    friend class Future<T>;

  public:
    bool await_ready() const noexcept { return future_->done(); }
    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> h) {
        parent_ = h;
        task_ = h.promise().get_task();
        task_->set_pending(this);
        future_->bind_event_loop(task_->get_event_loop());
    }
    T await_resume() {
        if (task_->cancelled()) {
            throw coconext::Cancelled{};
        }
        return future_->result();
    }

  private:
    explicit FutureAwaiter(FutureState<T>* future) : future_(future) {}

    void event_run() override { parent_.resume(); }

    FutureState<T>* future_;
    std::coroutine_handle<> parent_;
    TaskState<>* task_;
};

}  // namespace detail

// Single-shot, multiple-consumer awaitable object.
template <typename T>
class Future {
  public:
    Future() : handle_(new detail::FutureState<T>{}) {}
    ~Future() { handle_->dec_ref(); }
    Future(Future const& other) : handle_(other.handle_) { handle_->inc_ref(); }
    Future(Future&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Future& operator=(Future const& other) {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }
    Future& operator=(Future&& other) noexcept {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    T result() const { return handle_->result(); }

    bool done() const noexcept { return handle_->done(); }
    bool cancelled() const noexcept { return handle_->cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }

    template <typename F>
    void add_callback(F&& callback) {
        handle_->add_callback(std::forward<F>(callback));
    }

    auto operator co_await() { return detail::FutureAwaiter(handle_); }

    static Future from_state(detail::FutureState<T>* state) { return Future{state}; }
    detail::FutureState<T>* get_state() const noexcept { return handle_; }

  private:
    explicit Future(detail::FutureState<T>* state) : handle_(state) { handle_->inc_ref(); }

    detail::FutureState<T>* handle_;
};

namespace detail {

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

    ~TaskStateBase() {
        if (wait_complete_future_) {
            wait_complete_future_->dec_ref();
        }
    }

  private:
    void set_result(Result<T>&& value) noexcept {
        state_ = std::move(value);
        // TODO: handle cancelled_ > 0
        on_done();
    }

    void on_done();

    void bind_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Task is already bound to another EventLoop");
        }
    }

    Future<void> wait_complete() const noexcept {
        if (!wait_complete_future_) {
            wait_complete_future_ = new FutureState<void>{};
            wait_complete_future_->inc_ref();
            if (done()) {
                wait_complete_future_->set_void();
            }
        }
        return Future<void>::from_state(wait_complete_future_);
    }

    void set_pending(Event* future_awaiter) override { state_ = Pending{future_awaiter}; }

    std::variant<std::monostate, Scheduled, Pending, Result<T>, Exception> state_;
    std::vector<std::function<void(Task<T>&)>> callbacks_;
    mutable FutureState<void>* wait_complete_future_ = nullptr;
    detail::TaskManagerState* task_manager_ = nullptr;
    EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

template <typename T>
class TaskState : public TaskStateBase<T> {
  public:
    void return_value(T value) { this->set_result(Result<T>{std::move(value)}); }
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

    Future<void> wait_complete() const noexcept;

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

class TaskManagerState {
    friend class ::coconext::TaskManager;
    template <typename T>
    friend T(::coconext::run)(Task<T> task);

  public:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    void add(Task<>& task) {
        if (task.unstarted()) {
            task.get_state()->start_soon(this);
        }
        if (cancelled_ > 0) {
            throw std::runtime_error("Cannot add task to cancelled TaskManager");
        }
        task.get_state()->inc_ref();
        tasks_.push_back(task.get_state());
    }

    EventLoop* get_event_loop() noexcept { return event_loop_; }

    bool done() const noexcept;
    bool cancelled() const noexcept;
    void result() const {}
    std::exception_ptr exception() const noexcept { return nullptr; }

    void cancel() noexcept {
        if (done()) {
            return;
        }
        if (!cancelled_) {
            for (auto& task : tasks_) {
                task.cancel();
            }
        }
        cancelled_++;
    }
    void uncancel() {
        if (done()) {
            return;
        }
        if (cancelled_ == 0) {
            throw std::runtime_error("TaskManager is not cancelled");
        }
        cancelled_--;
    }

    Future<void> result_future() { return result_future_; }

    void child_done(TaskState<>* task) {
        task->remove_child();
        task->dec_ref();
        if (cancelled_ > 0 && tasks_.empty()) {
            on_done();
        }
    }

  private:
    void on_done() noexcept { result_future_.get_state()->set_void(); }

    void bind_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("TaskManager is already bound to another EventLoop");
        }
    }

    IntrusiveDeque<TaskState<>> tasks_;
    Future<void> result_future_;
    EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

}  // namespace detail

class TaskManager {
  public:
    TaskManager() : state_(new detail::TaskManagerState{}) { state_->inc_ref(); }
    ~TaskManager() { state_->dec_ref(); }

    void add(Task<>& task) { state_->add(task); }

    bool done() const noexcept { return state_->done(); }
    bool cancelled() const noexcept { return state_->cancelled(); }
    void result() const { state_->result(); }
    std::exception_ptr exception() const noexcept { return state_->exception(); }

    void cancel() noexcept { state_->cancel(); }
    void uncancel() noexcept { state_->uncancel(); }

    auto operator co_await() { return state_->result_future().operator co_await(); }

    detail::TaskManagerState* get_state() const noexcept { return state_; }
    static TaskManager from_state(detail::TaskManagerState* state) {
        return TaskManager{state};
    }

  private:
    explicit TaskManager(detail::TaskManagerState* state) : state_(state) {
        state_->inc_ref();
    }

    detail::TaskManagerState* state_;
};

namespace detail {

template <typename T>
void TaskStateBase<T>::on_done() {
    auto task = Task<T>::from_state(static_cast<TaskState<T>*>(this));
    for (auto& callback : callbacks_) {
        callback(task);
    }
    task_manager_->child_done(static_cast<TaskState<T>*>(this));
    if (wait_complete_future_) {
        wait_complete_future_->set_void();
    }
}

template <typename T>
void TaskStateBase<T>::start_soon(TaskManagerState* tm) {
    if (!unstarted()) {
        throw std::runtime_error("Task already started");
    }
    bind_event_loop(tm->get_event_loop());
    state_ = Scheduled{*this};
    event_loop_->acquire().schedule_back(&std::get<Scheduled>(state_));
}

}  // namespace detail

template <typename T>
Future<void> Task<T>::wait_complete() const noexcept {
    return handle_->wait_complete();
}

template <typename T>
void Task<T>::start_soon(TaskManager& loop) {
    handle_->start_soon(loop.get_state());
}

}  // namespace coconext

#endif  // COCONEXT_SCHEDULER_HPP

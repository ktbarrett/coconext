#ifndef COCONEXT_TASK_HPP
#define COCONEXT_TASK_HPP

#include <coroutine>
#include <exception>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <coconext/cancelled.hpp>
#include <coconext/event_loop.hpp>
#include <vector>

namespace coconext {

namespace detail {

class ManagedObject : public IntrusiveDequeNode {
  public:
    virtual void inc_ref() noexcept = 0;
    virtual void dec_ref() noexcept = 0;

    virtual EventLoop* get_event_loop() noexcept = 0;
    virtual ManagedObject* get_parent() noexcept = 0;

    virtual bool cancelled() const noexcept = 0;
    virtual bool done() const noexcept = 0;
    virtual std::exception_ptr exception() const noexcept = 0;

    virtual void cancel() noexcept = 0;
    virtual void uncancel() noexcept = 0;
};

class Erased {};

}  // namespace detail

template <typename T = detail::Erased>
class Task;

namespace detail {

template <typename T = Erased>
class TaskState;

template <>
class TaskState<Erased> : public detail::ManagedObject {
  public:
    // While all of these methods are virtual, the compiler will almost certainly
    // devirtualize them in practice since there is only one implementing class.

    virtual TaskState<>* get_task() noexcept = 0;
    virtual bool unstarted() const noexcept = 0;
    virtual void start_soon(EventLoop* loop) = 0;
    virtual void bind_event_loop(EventLoop* loop) = 0;
    virtual void set_pending(Event* future_awaiter) = 0;
};

// Result and Exception are defined here because FutureState uses it as well.

template <typename T>
struct Result {
    T value;
};

template <>
struct Result<void> {};

struct Exception {
    std::exception_ptr exception;
};

template <typename T>
class TaskStateBase : public TaskState<Erased> {
    struct Scheduled : detail::Event {
        explicit Scheduled(TaskStateBase<T>& task) : task_(&task) {}
        void event_run() override;
        TaskStateBase<Erased>* task_;
    };

    struct Pending {
        Event* event;
    };

  public:
    Task<T> get_return_object() { return Task<T>{static_cast<TaskState<T>*>(this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept {
        // TODO: handle cancelled_ > 0
        on_done();
        return {};
    }
    void unhandled_exception() {
        // TODO: handle cancelled_ > 0
        on_done();
        state_ = Exception{std::current_exception()};
    }
    void on_done() {
        Task<T> task{static_cast<TaskState<T>*>(this)};
        for (auto& callback : callbacks_) {
            callback(&task);
        }
    }

    TaskState<>* get_task() noexcept override { return this; }
    EventLoop* get_event_loop() noexcept override { return event_loop_; }
    ManagedObject* get_parent() noexcept override { return parent_; }

    bool unstarted() const noexcept override {
        return std::holds_alternative<std::monostate>(state_);
    }
    bool cancelled() const noexcept override { return cancelled_; }
    bool done() const noexcept override {
        return std::holds_alternative<Result>(state_)
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
            event_loop_->acquire().schedule_back(&std::get<Scheduled>(state_));
        }
        // Not done, unstarted, or pending? Already scheduled, we will steal its place in
        // line.
    }
    void uncancel() noexcept override {
        if (done()) {
            return;
        }
        cancelled_--;
    }
    void start_soon(EventLoop* loop) override {
        if (!unstarted()) {
            throw std::runtime_error("Task already started");
        }
        bind_event_loop(loop);
        state_ = Scheduled{*this};
        event_loop_->acquire().schedule_back(&std::get<Scheduled>(state_));
    }

    void inc_ref() noexcept override { ref_count_++; }
    void dec_ref() noexcept override {
        ref_count_--;
        if (ref_count_ == 0) {
            auto& handle = std::coroutine_handle<TaskState<T>>::from_promise(
                *static_cast<TaskState<T>*>(this)
            );
            // This is basically a "delete this", so no more code should follow this line.
            handle.destroy();
        }
    }

    void bind_event_loop(EventLoop* loop) override {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("Task is already bound to another EventLoop");
        }
    }

    void set_pending(Event* future_awaiter) override { state_ = Pending{future_awaiter}; }

  private:
    friend class TaskState<T>;
    void set_result(Result<T>&& value) noexcept { state_ = std::move(value); }

    std::variant<std::monostate, Scheduled, Pending, Result<T>, Exception> state_;
    std::vector<std::function<void(Task<T>&)>> callbacks_;
    ManagedObject* parent_ = nullptr;
    EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
    int16_t cancelled_{0};
};

template <typename T>
class TaskState : public TaskStateBase<T> {
  public:
    void return_value(T value) { set_result(Result<T>{std::move(value)}); }
};

template <>
class TaskState<void> : public TaskStateBase<void> {
  public:
    void return_void() { set_result(Result<void>{}); }
};

}  // namespace detail

template <>
class Task<detail::Erased> {
  public:
    ~Task() {
        if (handle_) {
            handle_->dec_ref();
        }
    }
    Task(Task const& other) : handle_(other.handle_) { handle_->inc_ref(); }
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
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

    bool unstarted() const noexcept { return handle_->unstarted(); }
    bool done() const noexcept { return handle_->done(); }
    bool cancelled() const noexcept { return handle_->cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }

    void start_soon();
    void cancel() noexcept { handle_->cancel(); }

  private:
    template <typename>
    friend class Task;

    explicit Task(detail::TaskState<>* s) : handle_(s) { handle_->inc_ref(); }
    detail::TaskState<>* handle_;
};

template <typename T>
class Task : public Task<detail::Erased> {
  public:
    T result() {
        return static_cast<detail::TaskState<T>*>(Task<detail::Erased>::handle_)->result();
    }
};

namespace detail {

// Sticking the current task variable in the header as inline thread_local allows us to
// stick global dynamic lookup. The initialization cost is also not a problem since this is
// a simple pointer.
inline thread_local TaskState<>* current_task_ = nullptr;

template <typename T>
void TaskStateBase<T>::Scheduled::event_run() {
    detail::current_task_ = task_;
    auto& handle = std::coroutine_handle<TaskState<T>>::from_promise(
        *static_cast<TaskState<T>*>(task_)
    );
    handle.resume();
}

}  // namespace detail

void Task<detail::Erased>::start_soon() {
    if (detail::current_task_ == nullptr) {
        throw std::runtime_error("Task::start_soon() called outside of a Task");
    }
    handle_->start_soon(detail::current_task_->get_event_loop());
}

}  // namespace coconext

#endif  // COCONEXT_TASK_HPP

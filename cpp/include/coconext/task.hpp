#ifndef COCONEXT_TASKS_HPP
#define COCONEXT_TASKS_HPP

#include <atomic>
#include <coroutine>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include <coconext/cancelled.hpp>
#include <coconext/event_loop.hpp>

namespace coconext {

template <typename T>
class Task;

namespace detail {

class TaskStateBase {
  public:
    // While all of these methods are virtual, the compiler will almost certainly
    // devirtualize them in practice since there is only one implementing class.

    virtual TaskStateBase* get_task() noexcept = 0;
    virtual coconext::EventLoop* get_event_loop() noexcept = 0;

    virtual bool cancelled() const noexcept = 0;
    virtual bool done() const noexcept = 0;
    virtual std::exception_ptr exception() const noexcept = 0;

    virtual void cancel() noexcept = 0;

    virtual void inc_ref() noexcept = 0;
    virtual void dec_ref() noexcept = 0;
};

template <typename T>
class TaskState : public TaskStateBase {
    struct Value {
        T value;
    };

  public:
    Task<T> get_return_object() { return Task<T>{this}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_value(T value) { result_ = Value{std::move(value)}; }
    void unhandled_exception() { result_ = std::current_exception(); }

    TaskStateBase* get_task() noexcept override { return this; }
    coconext::EventLoop* get_event_loop() noexcept override { return event_loop_; }

    bool cancelled() const noexcept override {
        return std::holds_alternative<Cancelled>(result_);
    }
    bool done() const noexcept override {
        return !std::holds_alternative<std::monostate>(result_);
    }
    T result() {
        if (!done()) {
            throw std::runtime_error("Task not completed yet.");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        if (std::holds_alternative<Value>(result_)) {
            return std::get<Value>(result_).value;
        }
        throw std::runtime_error("Task does not have a result");
    }
    std::exception_ptr exception() const noexcept override {
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            return std::get<std::exception_ptr>(result_);
        }
        return nullptr;
    }

    void inc_ref() noexcept override { ref_count_.fetch_add(1, std::memory_order_relaxed); }
    void dec_ref() noexcept override {
        auto prev = ref_count_.fetch_sub(1, std::memory_order_relaxed);
        if (prev == 1) {
            std::coroutine_handle<TaskState>::from_promise(*this).destroy();
            // The above is basically a "delete this", so no more code should follow this
            // line.
        }
    }

  private:
    std::atomic<size_t> ref_count_{0};
    std::variant<std::monostate, Value, std::exception_ptr, Cancelled> result_;
    coconext::EventLoop* event_loop_ = nullptr;
};

class Erased {};

template <typename HandleType>
    requires std::is_base_of_v<TaskStateBase, HandleType>
class TaskBase {
  public:
    ~TaskBase() { handle_->dec_ref(); }

    bool done() const noexcept { return handle_->done(); }
    bool cancelled() const noexcept { return handle_->cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }

    void cancel() noexcept { handle_->cancel(); }

  protected:
    explicit TaskBase(HandleType* h) : handle_(h) {}
    HandleType* handle_;
};

}  // namespace detail

template <typename T = detail::Erased>
class Task : public detail::TaskBase<detail::TaskState<T>> {
    using Base = detail::TaskBase<detail::TaskState<T>>;
    friend class detail::TaskState<T>;

  public:
    Task(Task const& other) : Base(other.handle_) { Base::handle_->inc_ref(); }
    Task(Task&& other) noexcept : Base(other.handle_) { other.handle_ = nullptr; }
    Task& operator=(Task const& other) {
        if (this != &other) {
            Base::handle_->dec_ref();
            Base::handle_ = other.handle_;
            Base::handle_->inc_ref();
        }
        return *this;
    }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            Base::handle_->dec_ref();
            Base::handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

  private:
    explicit Task(detail::TaskState<T>* h) : Base(h) { Base::handle_->inc_ref(); }
};

namespace detail {

template <>
class TaskState<void> : public TaskStateBase {
    class Done {};

  public:
    Task<void> get_return_object() { return Task<void>{this}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() { result_ = Done{}; }
    void unhandled_exception() { result_ = std::current_exception(); }

    TaskStateBase* get_task() noexcept override { return this; }
    coconext::EventLoop* get_event_loop() noexcept override { return event_loop_; }

    bool cancelled() const noexcept override {
        return std::holds_alternative<Cancelled>(result_);
    }
    bool done() const noexcept override {
        return !std::holds_alternative<std::monostate>(result_);
    }
    void result() {
        if (!done()) {
            throw std::runtime_error("Task not completed yet.");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        if (std::holds_alternative<Done>(result_)) {
            return;
        }
        throw std::runtime_error("Task does not have a result");
    }
    std::exception_ptr exception() const noexcept override {
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            return std::get<std::exception_ptr>(result_);
        }
        return nullptr;
    }

    void inc_ref() noexcept override { ref_count_.fetch_add(1, std::memory_order_relaxed); }
    void dec_ref() noexcept override {
        auto prev = ref_count_.fetch_sub(1, std::memory_order_relaxed);
        if (prev == 1) {
            std::coroutine_handle<TaskState<void>>::from_promise(*this).destroy();
            // The above is basically a "delete this", so no more code should follow this
            // line.
        }
    }

  private:
    std::atomic<size_t> ref_count_{0};
    std::variant<std::monostate, Done, std::exception_ptr, Cancelled> result_;
    coconext::EventLoop* event_loop_ = nullptr;
};

}  // namespace detail

template <>
class Task<detail::Erased> : public detail::TaskBase<detail::TaskStateBase> {
    using Base = detail::TaskBase<detail::TaskStateBase>;

  public:
    template <typename T>
    Task(Task<T> const& other) : Base(other) {
        Base::handle_->inc_ref();
    }
    template <typename T>
    Task(Task<T>&& other) noexcept : Base(other.handle_) {
#ifndef NDEBUG
        other.handle_ = nullptr;
#endif
    }
    template <typename T>
    Task& operator=(Task<T> const& other) {
        if (this != &other) {
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }
    template <typename T>
    Task& operator=(Task<T>&& other) noexcept {
        if (this != &other) {
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
};

}  // namespace coconext

#endif  // COCONEXT_TASKS_HPP

#ifndef COCONEXT_TASK_HPP
#define COCONEXT_TASK_HPP

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <coconext/cancelled.hpp>
#include <coconext/event_loop.hpp>

namespace coconext {

namespace detail {

class Erased {};

}  // namespace detail

template <typename T = detail::Erased>
class Task;

namespace detail {

class TaskStateTypeErased {
  public:
    // While all of these methods are virtual, the compiler will almost certainly
    // devirtualize them in practice since there is only one implementing class.

    virtual TaskStateTypeErased* get_task() noexcept = 0;
    virtual detail::EventLoop* get_event_loop() noexcept = 0;

    virtual bool cancelled() const noexcept = 0;
    virtual bool done() const noexcept = 0;
    virtual std::exception_ptr exception() const noexcept = 0;

    virtual void cancel() noexcept = 0;

    virtual void inc_ref() noexcept = 0;
    virtual void dec_ref() noexcept = 0;
};

template <typename T>
struct ResultValue {
    T value;
};

template <>
struct ResultValue<void> {};

template <typename T>
class TaskState;

template <typename T>
class TaskStateBase : public TaskStateTypeErased {
  public:
    Task<T> get_return_object() { return Task<T>{static_cast<TaskState<T>*>(this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() { result_ = std::current_exception(); }

    TaskStateTypeErased* get_task() noexcept override { return this; }
    detail::EventLoop* get_event_loop() noexcept override { return event_loop_; }

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
        if (std::holds_alternative<ResultValue<T>>(result_)) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::get<ResultValue<T>>(result_).value;
            }
        }
        throw std::runtime_error("Task does not have a result");
    }
    std::exception_ptr exception() const noexcept override {
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            return std::get<std::exception_ptr>(result_);
        }
        return nullptr;
    }

    void inc_ref() noexcept override { ref_count_++; }
    void dec_ref() noexcept override {
        ref_count_--;
        if (ref_count_ == 0) {
            std::coroutine_handle<TaskState<T>>::from_promise(
                *static_cast<TaskState<T>*>(this)
            )
                .destroy();
            // The above is basically a "delete this", so no more code should follow this
            // line.
        }
    }

  private:
    friend class TaskState<T>;
    void set_result(ResultValue<T> value) noexcept { result_ = std::move(value); }

    std::variant<std::monostate, ResultValue<T>, std::exception_ptr, Cancelled> result_;
    detail::EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
};

template <typename T>
class TaskState : public TaskStateBase<T> {
  public:
    void return_value(T value) { set_result(ResultValue<T>{std::move(value)}); }
};

template <>
class TaskState<void> : public TaskStateBase<void> {
  public:
    void return_void() { set_result(ResultValue<void>{}); }
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

    detail::EventLoop* get_event_loop() noexcept { return handle_->get_event_loop(); }
    bool done() const noexcept { return handle_->done(); }
    bool cancelled() const noexcept { return handle_->cancelled(); }
    std::exception_ptr exception() const noexcept { return handle_->exception(); }
    void cancel() noexcept { handle_->cancel(); }

  private:
    template <typename>
    friend class Task;

    explicit Task(detail::TaskStateTypeErased* s) : handle_(s) { handle_->inc_ref(); }
    detail::TaskStateTypeErased* handle_;
};

template <typename T>
class Task : public Task<detail::Erased> {
  public:
    T result() {
        return static_cast<detail::TaskState<T>*>(Task<detail::Erased>::handle_)->result();
    }
};

namespace detail {

extern thread_local coconext::Task<>* current_task_;

}  // namespace detail

}  // namespace coconext

#endif  // COCONEXT_TASK_HPP

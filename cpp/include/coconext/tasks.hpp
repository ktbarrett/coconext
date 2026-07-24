#ifndef COCONEXT_TASKS_HPP
#define COCONEXT_TASKS_HPP

#include <atomic>
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>

#include <coconext/event_loop.hpp>

namespace coconext {

namespace detail {
// These are interfaces we want Awaiters to have access to. We can't expose the full Promise
// since the types are different between Coro::Promise, Task::Promise, and of course the
// various return types. Each Awaiter's await_suspend is templated and upcasts each concrete
// type to this base class for storage.
class TaskContext {
  public:
    bool cancelled() const noexcept { return cancelled_; }
    coconext::EventLoop* get_event_loop() noexcept { return event_loop_; }

  protected:
    bool cancelled_ = false;
    coconext::EventLoop* event_loop_ = nullptr;
};

}  // namespace detail

template <typename T>
class Task {
    class Promise : public detail::TaskContext {
        std::optional<T> value_ = std::nullopt;
        std::exception_ptr exception_ = nullptr;

        // This Promise is a shared ARC object. Task wraps std::coroutine_handle acting like
        // a shared pointer. TaskManagers hold at least one reference to running Tasks, so
        // we don't have to worry about fire-and-forget tasks being dangling pointers.
        std::atomic<size_t> ref_count_{0};
        void inc_ref() noexcept { ref_count_.fetch_add(1, std::memory_order_relaxed); }
        bool dec_ref() noexcept {
            auto prev = ref_count_.fetch_sub(1, std::memory_order_relaxed);
            // Return true if ref count is 0 after this function completes.
            return (prev == 1);
        }

      public:
        Task<T> get_return_object() {
            return Task<T>{std::coroutine_handle<Promise>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_value(T value) { value_ = value; }
        void unhandled_exception() { exception_ = std::current_exception(); }

      public:
        detail::TaskContext* get_context() noexcept { return this; }
    };

  public:
    explicit Task(std::coroutine_handle<Promise> h) : handle(h) {
        handle.promise().inc_ref();
    }
    ~Task() {
        if (handle.promise().dec_ref()) {
            handle.destroy();
        }
    }

  public:
    bool done() const noexcept { return handle.done(); }
    bool cancelled() const noexcept { return handle.promise().cancelled_; }
    T result() {
        if (!done()) {
            throw std::runtime_error("Task not completed yet.");
        }
        if (handle.promise().exception_) {
            std::rethrow_exception(handle.promise().exception_);
        }
        return *(handle.promise().value_);
    }
    std::exception_ptr exception() const noexcept { return handle.promise().exception_; }

  public:
    void cancel() noexcept { handle.promise().cancelled_ = true; }

  private:
    std::coroutine_handle<Promise> handle;
};

// Passthrough coroutine, keeps a reference to the owning Task's Promise
template <typename T>
class Coro {
    class Promise {
      public:
        Coro<T> get_return_object() {
            return Coro<T>{std::coroutine_handle<Promise>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            // This exists to "chain" coros together.
            class TransferAwaitable {
              public:
                explicit TransferAwaitable(std::coroutine_handle<> parent)
                    : parent_(parent) {}

              public:  // Awaitable API
                bool await_ready() noexcept { return false; }
                // Returning the coroutine_handle here is what transfers control back to the
                // parent coroutine.
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
                    return parent_;
                }
                void await_resume() noexcept {}

              private:
                std::coroutine_handle<> parent_;
            };
            return TransferAwaitable{parent_};
        }
        template <typename U>
        void return_value(U&& value) {
            value_ = std::forward<U>(value);
        }
        void unhandled_exception() { exception_ = std::current_exception(); }

        T result() {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
            if (value_.has_value()) {
                return std::move(*value_);
            }
            throw std::runtime_error("Coro does not have a result");
        }

      public:
        detail::TaskContext* get_context() noexcept { return task_context_; }

      private:
        std::optional<T> value_;
        std::exception_ptr exception_;
        std::coroutine_handle<> parent_;
        detail::TaskContext* task_context_;
    };

  public:
    class Awaiter {
      public:
        Awaiter(Coro& coro) : coro_(coro) {}

      public:
        bool await_ready() const noexcept { return false; }
        template <typename PromiseType>
        void await_suspend(std::coroutine_handle<PromiseType> h) noexcept {
            coro_.handle_.promise().task_promise_ = h.promise().get_context();
            coro_.handle_.promise().parent_ = h;
        }
        T await_resume() { return coro_.handle_.promise().result(); }

      private:
        Coro& coro_;
    };

  public:
    explicit Coro(std::coroutine_handle<Promise> h) : handle_(h) {}
    ~Coro() {}

    // Coro is only used once, so it's move-only
    Coro(Coro const&) = delete;
    Coro& operator=(Coro const&) = delete;
    Coro(Coro&&) = default;
    Coro& operator=(Coro&&) = default;

  public:
    auto operator co_await() { return Awaiter{*this}; }

  private:
    std::coroutine_handle<Promise> handle_;
};

}  // namespace coconext

#endif  // COCONEXT_TASKS_HPP

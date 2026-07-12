#ifndef COCONEXT_TASKS_HPP
#define COCONEXT_TASKS_HPP

#include <atomic>
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>

namespace coconext::tasks {

template <typename T>
class Task;

template <typename T>
class Task {
    class Promise {
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
    bool cancelled() const noexcept { return false; }
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
    void cancel() noexcept {
        // TODO We need to coordinate with the Future we are awaiting to inform it of the
        // CancelledError it must raise.
    }

  private:
    std::coroutine_handle<Promise> handle;
};

// Passthrough coroutine, keeps a reference to the owning Task's Promise
template <typename T>
class Coro {
    class Promise {
        Coro<T> get_return_object() {
            return Coro<T>{std::coroutine_handle<Promise>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_value(T value) { /* store the value */ }
        void unhandled_exception() { std::terminate(); }
    };

  public:
    explicit Coro(std::coroutine_handle<Promise> h) : handle(h) {}
    ~Coro() {}

    // Coro is only used once, so it's move-only
    Coro(Coro const&) = delete;
    Coro& operator=(Coro const&) = delete;
    Coro(Coro&&) = default;
    Coro& operator=(Coro&&) = default;

  public:
    void operator co_await() {
        // TODO
    }

  private:
    std::coroutine_handle<Promise> handle;
};

}  // namespace coconext::tasks

#endif  // COCONEXT_TASKS_HPP

#ifndef COCONEXT_CORO_HPP
#define COCONEXT_CORO_HPP

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <coconext/task.hpp>

namespace coconext {

template <typename T>
class Coro;

template <typename T>
class CoroState;

template <typename T>
class CoroStateBase {
    friend class CoroState<T>;

  public:
    Coro<T> get_return_object() {
        return Coro<T>{std::coroutine_handle<CoroState<T>>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept { return {}; }
    auto final_suspend() noexcept {
        // This exists to "chain" coros together.
        class TransferAwaitable {
          public:
            explicit TransferAwaitable(std::coroutine_handle<> parent) : parent_(parent) {}

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
    void unhandled_exception() { value_ = detail::Exception{std::current_exception()}; }

    T result() {
        if (std::holds_alternative<detail::Exception>(value_)) {
            std::rethrow_exception(std::get<detail::Exception>(value_).exception);
        }
        if (std::holds_alternative<detail::Result<T>>(value_)) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::move(std::get<detail::Result<T>>(value_).value);
            }
        }
        throw std::runtime_error("Coro does not have a result");
    }

    TaskState<>* get_task() noexcept { return task_; }

  private:
    void set_result(detail::Result<T> value) noexcept { value_ = std::move(value); }

    std::variant<std::monostate, detail::Result<T>, detail::Exception> value_;
    std::coroutine_handle<> parent_;
    TaskState<>* task_;
};

template <typename T>
class CoroState : public CoroStateBase<T> {
  public:
    template <typename U>
        requires std::is_convertible_v<U, T>
    void return_value(U&& value) {
        set_result(detail::Result<T>{std::forward<U>(value)});
    }
};

template <>
class CoroState<void> : public CoroStateBase<void> {
  public:
    void return_void() { this->set_result(detail::Result<void>{}); }
};

// Passthrough coroutine, keeps a reference to the owning Task's Promise
template <typename T>
class Coro {
  public:
    class Awaiter {
      public:
        Awaiter(Coro& coro) : coro_(coro) {}

      public:
        bool await_ready() const noexcept { return false; }
        template <typename PromiseType>
        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<PromiseType> h
        ) noexcept {
            auto p = coro_.handle_.promise();
            p.task_ = h.promise().get_task();
            p.parent_ = h;
            return coro_.handle_;
        }
        T await_resume() { return coro_.handle_.promise().result(); }

      private:
        Coro& coro_;
    };

  public:
    explicit Coro(std::coroutine_handle<CoroState<T>> h) : handle_(h) {}
    ~Coro() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Coro is only used once, so it's move-only
    Coro(Coro const&) = delete;
    Coro& operator=(Coro const&) = delete;
    Coro(Coro&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Coro& operator=(Coro&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    CoroState<T>& get_state() const noexcept { return handle_.promise(); }
    // No from_state() because Coro cannot be copied.

  public:
    auto operator co_await() { return Awaiter{*this}; }

  private:
    std::coroutine_handle<CoroState<T>> handle_;
};

}  // namespace coconext

#endif  // COCONEXT_CORO_HPP

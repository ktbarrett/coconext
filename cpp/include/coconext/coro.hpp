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

namespace detail {

template <typename T>
class CoroStateBase {
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
    void unhandled_exception() { value_ = Exception{std::current_exception()}; }

    T result() {
        if (std::holds_alternative<Exception>(value_)) {
            std::rethrow_exception(std::get<Exception>(value_).exception);
        }
        if (std::holds_alternative<Result<T>>(value_)) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::move(std::get<Result<T>>(value_).value);
            }
        }
        throw std::runtime_error("Coro does not have a result");
    }

    detail::TaskState<>* get_task() noexcept { return task_; }

  protected:
    void set_result(Result<T> value) noexcept { value_ = std::move(value); }

  private:
    std::variant<std::monostate, Result<T>, Exception> value_;
    std::coroutine_handle<> parent_;
    detail::TaskState<>* task_;
};

template <typename T>
class CoroState : public CoroStateBase<T> {
    using Base = CoroStateBase<T>;

  public:
    template <typename U>
        requires std::is_convertible_v<U, T>
    void return_value(U&& value) {
        Base::set_result(Result<T>{std::forward<U>(value)});
    }

  private:
    // This makes set_result private for subclasses.
    using Base::set_result;
};

template <>
class CoroState<void> : public CoroStateBase<void> {
    using Base = CoroStateBase<void>;

  public:
    void return_void() { Base::set_result(Result<void>{}); }

  private:
    // This makes set_result private for subclasses.
    using Base::set_result;
};

}  // namespace detail

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
        void await_suspend(std::coroutine_handle<PromiseType> h) noexcept {
            coro_.handle_.promise().task_ = h.promise().get_task();
            coro_.handle_.promise().parent_ = h;
        }
        T await_resume() { return coro_.handle_.promise().result(); }

      private:
        Coro& coro_;
    };

  public:
    explicit Coro(std::coroutine_handle<detail::CoroState<T>> h) : handle_(h) {}
    ~Coro() {}

    // Coro is only used once, so it's move-only
    Coro(Coro const&) = delete;
    Coro& operator=(Coro const&) = delete;
    Coro(Coro&&) = default;
    Coro& operator=(Coro&&) = default;

  public:
    auto operator co_await() { return Awaiter{*this}; }

  private:
    std::coroutine_handle<detail::CoroState<T>> handle_;
};

}  // namespace coconext

#endif  // COCONEXT_CORO_HPP

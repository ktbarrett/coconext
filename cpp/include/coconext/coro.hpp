#ifndef COCONEXT_CORO_HPP
#define COCONEXT_CORO_HPP

#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>
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
    friend class CoroState<T>;
    friend class Coro<T>;

  public:
    [[nodiscard]] Coro<T> get_return_object() noexcept {
        return Coro<T>{std::coroutine_handle<CoroState<T>>::from_promise(
            *static_cast<CoroState<T>*>(this)
        )};
    }
    [[nodiscard]] std::suspend_always initial_suspend() noexcept { return {}; }
    [[nodiscard]] auto final_suspend() noexcept {
        // This exists to "chain" coros together.
        class TransferAwaitable {
          public:
            explicit TransferAwaitable(std::coroutine_handle<> parent) noexcept
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
    void unhandled_exception() noexcept { value_ = std::current_exception(); }

    [[nodiscard]] T result() {
        if (std::holds_alternative<std::exception_ptr>(value_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(value_));
        }
        if (std::holds_alternative<detail::Value<T>>(value_)) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::move(std::get<detail::Value<T>>(value_).value);
            }
        }
        throw std::runtime_error("Coro does not have a result");
    }

    // This carries the enclosing Task's scheduler bindings through arbitrarily nested
    // Coros without depending on a TLS lookup.
    [[nodiscard]] TaskContext get_context() const noexcept {
        assert(context_.has_value());
        return *context_;
    }

  private:
    void set_result(detail::Value<T> value) noexcept { value_ = std::move(value); }

    std::variant<std::monostate, detail::Value<T>, std::exception_ptr> value_;
    std::coroutine_handle<> parent_;
    std::optional<TaskContext> context_;
};

}  // namespace detail

template <typename T>
class CoroState : public detail::CoroStateBase<T> {
  public:
    template <typename U>
        requires std::is_convertible_v<U, T>
    void return_value(U&& value) noexcept(std::is_nothrow_constructible_v<T, U>) {
        this->set_result(detail::Value<T>{std::forward<U>(value)});
    }
};

template <>
class CoroState<void> : public detail::CoroStateBase<void> {
  public:
    void return_void() noexcept { this->set_result(detail::Value<void>{}); }
};

// Passthrough coroutine, keeps a reference to the owning Task's Promise
template <typename T>
class Coro {
  public:
    class Awaiter {
      public:
        explicit Awaiter(Coro& coro) noexcept : coro_(coro) {}

      public:
        [[nodiscard]] bool await_ready() const noexcept { return false; }
        template <typename PromiseType>
        [[nodiscard]] std::coroutine_handle<> await_suspend(
            std::coroutine_handle<PromiseType> h
        ) noexcept {
            auto& p = coro_.handle_.promise();
            p.context_ = h.promise().get_context();
            p.parent_ = h;
            return coro_.handle_;
        }
        [[nodiscard]] T await_resume() { return coro_.handle_.promise().result(); }

      private:
        Coro& coro_;
    };

    using promise_type = CoroState<T>;

    [[nodiscard]] explicit Coro(std::coroutine_handle<CoroState<T>> h) noexcept
        : handle_(h) {}
    ~Coro() noexcept {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Coro is only used once, so it's move-only
    Coro(Coro const&) = delete;
    Coro& operator=(Coro const&) = delete;
    [[nodiscard]] Coro(Coro&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Coro& operator=(Coro&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    [[nodiscard]] CoroState<T>& get_state() const noexcept { return handle_.promise(); }
    // No from_state() because Coro cannot be copied.

    [[nodiscard]] auto operator co_await() noexcept { return Awaiter{*this}; }

  private:
    std::coroutine_handle<CoroState<T>> handle_;
};

}  // namespace coconext

#endif  // COCONEXT_CORO_HPP

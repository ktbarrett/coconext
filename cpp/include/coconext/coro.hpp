#ifndef COCONEXT_CORO_HPP
#define COCONEXT_CORO_HPP

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <utility>
#include <variant>

#include <coconext/task.hpp>

namespace coconext {

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
        void unhandled_exception() { value_ = std::current_exception(); }

        T result() {
            if (std::holds_alternative<std::exception_ptr>(value_)) {
                std::rethrow_exception(std::get<std::exception_ptr>(value_));
            }
            if (std::holds_alternative<T>(value_)) {
                return std::move(std::get<T>(value_));
            }
            throw std::runtime_error("Coro does not have a result");
        }

        Task<>* get_task() noexcept { return task_; }

      private:
        std::variant<std::monostate, T, std::exception_ptr> value_;
        std::coroutine_handle<> parent_;
        Task<>* task_;
    };

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

#endif  // COCONEXT_CORO_HPP

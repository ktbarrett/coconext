#ifndef COCONEXT_FUTURE_HPP
#define COCONEXT_FUTURE_HPP

#include <coconext/task.hpp>

#include <cassert>
#include <cstddef>
#include <exception>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace coconext {

template <typename T>
class AbstractFutureState {
    template <typename>
    friend class AbstractFuture;
    template <typename>
    friend class detail::AwaitableAwaiter;

  public:
    using value_type = T;

    // States are deleted through AbstractFutureState<T> when the final AbstractFuture
    // reference is released. Derived states commonly prime an external trigger in
    // their constructor and unprime it in their destructor when !done().
    virtual ~AbstractFutureState() = default;

    [[nodiscard]] bool done() const noexcept {
        return !std::holds_alternative<std::monostate>(result_);
    }

    [[nodiscard]] std::exception_ptr exception() const {
        if (!done()) {
            throw std::runtime_error("Not done");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            return std::get<std::exception_ptr>(result_);
        }
        return nullptr;
    }

    [[nodiscard]] T result() const {
        if (!done()) {
            throw std::runtime_error("Not done");
        }
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        if constexpr (!std::is_void_v<T>) {
            return std::get<detail::Value<T>>(result_).value;
        }
    }

    template <typename F>
    void add_done_callback(F&& callback) {
        callbacks_.emplace_back(std::forward<F>(callback));
    }

  protected:
    template <typename U>
        requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
    void set_result(U&& value) noexcept {
        result_ = detail::Value<T>{std::forward<U>(value)};
        on_done();
    }

    void set_void() noexcept
        requires std::is_void_v<T>
    {
        result_ = detail::Value<void>{};
        on_done();
    }

    void set_exception(std::exception_ptr exc) {
        if (!exc) {
            throw std::invalid_argument("exc must not be null");
        }
        result_ = exc;
        on_done();
    }

  private:
    void on_awaited(TaskContext const& context) {
        auto event_loop = context.get_event_loop();
        if (event_loop_ == nullptr) {
            event_loop_ = event_loop;
        } else if (event_loop_ != event_loop) {
            throw std::runtime_error("Future is already bound to another EventLoop");
        }
    }

    void register_waiter(not_null<detail::Event*> awaiter) noexcept {
        waiters_.push_back(awaiter);
    }

    void on_done() noexcept {
        for (auto& callback : callbacks_) {
            callback();
        }
        if (!waiters_.empty()) {
            assert(event_loop_ != nullptr);
            event_loop_->acquire().schedule_all_back(std::move(waiters_));
        }
    }

    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    detail::IntrusiveDeque<detail::Event> waiters_;
    std::vector<std::function<void()>> callbacks_;
    std::variant<std::monostate, std::exception_ptr, detail::Value<T>> result_;
    detail::EventLoop* event_loop_ = nullptr;
    size_t ref_count_{0};
};

// Single-shot, multiple-consumer awaitable object.
template <typename StateT>
class AbstractFuture {
    static_assert(
        std::is_base_of_v<AbstractFutureState<typename StateT::value_type>, StateT>,
        "AbstractFuture's StateT must derive from AbstractFutureState<T>"
    );

  public:
    using value_type = typename StateT::value_type;

    [[nodiscard]] AbstractFuture() noexcept : handle_(new StateT{}) { handle_->inc_ref(); }
    [[nodiscard]] AbstractFuture(AbstractFuture const& other) noexcept
        : handle_(other.handle_) {
        handle_->inc_ref();
    }

    AbstractFuture& operator=(AbstractFuture const& other) noexcept {
        if (this != &other) {
            handle_->dec_ref();
            handle_ = other.handle_;
            handle_->inc_ref();
        }
        return *this;
    }

    ~AbstractFuture() noexcept { handle_->dec_ref(); }

    [[nodiscard]] bool done() const noexcept { return handle_->done(); }
    [[nodiscard]] value_type result() const { return handle_->result(); }
    [[nodiscard]] std::exception_ptr exception() const { return handle_->exception(); }

    template <typename F>
    void add_done_callback(F&& callback) {
        handle_->add_done_callback(std::forward<F>(callback));
    }

    [[nodiscard]] CoconextAwaitable auto operator co_await() noexcept {
        return detail::AwaitableAwaiter<StateT>(handle_);
    }

    [[nodiscard]] not_null<StateT*> get_state() const noexcept { return handle_; }
    [[nodiscard]] explicit AbstractFuture(not_null<StateT*> state) noexcept
        : handle_(state) {
        handle_->inc_ref();
    }

  private:
    not_null<StateT*> handle_;
};

// State for the concrete Future<T>. Unlike trigger-backed AbstractFutureState
// subclasses, this exposes its completion API publicly for ad-hoc inter-Task
// communication.
template <typename T>
class FutureState : public AbstractFutureState<T> {
  public:
    using AbstractFutureState<T>::set_exception;
    using AbstractFutureState<T>::set_result;
    using AbstractFutureState<T>::set_void;
};

template <typename T>
class Future : public AbstractFuture<FutureState<T>> {
  public:
    template <typename U>
        requires(!std::is_void_v<T> && std::is_convertible_v<U, T>)
    void set_result(U&& value) noexcept {
        this->get_state()->set_result(std::forward<U>(value));
    }

    void set_void() noexcept
        requires std::is_void_v<T>
    {
        this->get_state()->set_void();
    }

    void set_exception(std::exception_ptr exc) { this->get_state()->set_exception(exc); }
};

}  // namespace coconext

#endif  // COCONEXT_FUTURE_HPP

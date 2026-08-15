#ifndef COCONEXT_AWAITABLE_HPP
#define COCONEXT_AWAITABLE_HPP

#include <concepts>
#include <coroutine>
#include <type_traits>
#include <utility>

namespace coconext {

namespace detail {

template <typename T>
concept HasMemberCoAwait =
    requires(T&& value) { std::forward<T>(value).operator co_await(); };

template <typename T>
concept HasFreeCoAwait = requires(T&& value) { operator co_await(std::forward<T>(value)); };

template <typename T>
struct IsCoroutineHandle : std::false_type {};

template <typename PromiseType>
struct IsCoroutineHandle<std::coroutine_handle<PromiseType>> : std::true_type {};

template <typename T>
inline constexpr bool valid_await_suspend_result =
    std::is_void_v<T> || std::same_as<T, bool> || IsCoroutineHandle<T>::value;

}  // namespace detail

// C++ coroutine protocol because the standard doesn't define these for some reason...

template <typename T>
decltype(auto) get_awaiter(T&& value) {
    if constexpr (detail::HasMemberCoAwait<T>) {
        return std::forward<T>(value).operator co_await();
    } else if constexpr (detail::HasFreeCoAwait<T>) {
        return operator co_await(std::forward<T>(value));
    } else {
        return std::forward<T>(value);
    }
}

// Awaitability is relative to the enclosing coroutine's promise type. The default
// recognizes Awaiters that accept a type-erased coroutine handle; callers can name a
// more specific promise type when needed.
template <typename T, typename PromiseType = void>
concept Awaiter =
    requires(T& awaiter, std::coroutine_handle<PromiseType> parent) {
        { !awaiter.await_ready() } -> std::same_as<bool>;
        awaiter.await_suspend(parent);
        awaiter.await_resume();
    }
    && detail::valid_await_suspend_result<decltype(std::declval<T&>().await_suspend(
        std::declval<std::coroutine_handle<PromiseType>>()
    ))>;

template <typename T, typename PromiseType = void>
concept Awaitable = requires(T&& value) { get_awaiter(std::forward<T>(value)); }
                 && Awaiter<decltype(get_awaiter(std::declval<T>())), PromiseType>;

template <typename T>
using await_result_t = decltype(get_awaiter(std::declval<T>()).await_resume());

// coconext scheduler compatibility

class TaskContext;

namespace detail {

// A stand-in for coconext coroutine promises. Awaiters use the promise's context to
// preserve scheduler bindings across suspension; CoconextAwaiter only needs that
// interface rather than any particular promise implementation.
struct CoconextPromise {
    [[nodiscard]] TaskContext get_context() const noexcept;
};

}  // namespace detail

template <typename T>
concept CoconextAwaiter = Awaiter<T, detail::CoconextPromise> && requires {
    // Scheduler compatibility is semantic and cannot be inferred structurally, so Awaiters
    // opt in with this public marker.
    typename std::remove_cvref_t<T>::coconext_awaiter;
};

template <typename T>
concept CoconextAwaitable = requires(T&& value) { get_awaiter(std::forward<T>(value)); }
                         && CoconextAwaiter<decltype(get_awaiter(std::declval<T>()))>;

}  // namespace coconext

#endif  // COCONEXT_AWAITABLE_HPP

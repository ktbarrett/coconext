#ifndef COCONEXT_TYPES_UTIL_HPP
#define COCONEXT_TYPES_UTIL_HPP

#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

namespace coconext::types {

template <typename T>
struct is_char : public std::false_type {};

template <typename T>
concept Character = is_char<std::remove_cv_t<T>>::value;

template <>
struct is_char<char> : public std::true_type {};
template <>
struct is_char<wchar_t> : public std::true_type {};
template <>
struct is_char<char8_t> : public std::true_type {};
template <>
struct is_char<char16_t> : public std::true_type {};
template <>
struct is_char<char32_t> : public std::true_type {};

template <typename T>
struct is_native_int : public std::false_type {};

template <typename T>
concept NativeInteger =
    is_native_int<std::remove_cv_t<T>>::value && std::numeric_limits<T>::is_integer;

template <>
struct is_native_int<signed char> : public std::true_type {};
template <>
struct is_native_int<unsigned char> : public std::true_type {};
template <>
struct is_native_int<short> : public std::true_type {};
template <>
struct is_native_int<unsigned short> : public std::true_type {};
template <>
struct is_native_int<int> : public std::true_type {};
template <>
struct is_native_int<unsigned int> : public std::true_type {};
template <>
struct is_native_int<long> : public std::true_type {};
template <>
struct is_native_int<unsigned long> : public std::true_type {};
template <>
struct is_native_int<long long> : public std::true_type {};
template <>
struct is_native_int<unsigned long long> : public std::true_type {};

#if defined(__SIZEOF_INT128__)
template <>
struct is_native_int<__int128_t> : public std::true_type {};
template <>
struct is_native_int<__uint128_t> : public std::true_type {};
#endif

template <typename T>
inline constexpr bool is_fixed = false;

namespace detail {

template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template <typename T>
inline constexpr bool is_coconext_unsigned_v = false;

template <typename T>
inline constexpr bool is_coconext_signed_v = false;

// Niebloid that reads a type's packed storage. Implementers declare
// `friend struct detail::storage_fn;` and keep `value_` private; ADL cannot find
// this call because `storage` is an object, so users can only reach it via the
// qualified `detail::storage(x)`.
struct storage_fn {
    template <typename T>
    constexpr auto operator()(T const& t) const noexcept -> decltype((t.value_)) {
        return t.value_;
    }

    template <typename T>
        requires(!std::is_lvalue_reference_v<T>)
    constexpr auto operator()(T&& t) const noexcept
        -> decltype((std::forward<T>(t).value_)) {
        return std::forward<T>(t).value_;
    }
};

inline constexpr storage_fn storage{};

template <typename T>
concept HasStorage = requires(T const& t) {
    { detail::storage(t) };
};

template <typename T>
concept Formattable = std::semiregular<std::formatter<std::remove_cvref_t<T>, char>>;

}  // namespace detail

}  // namespace coconext::types

#endif

#ifndef COCONEXT_TYPES_UTIL_HPP
#define COCONEXT_TYPES_UTIL_HPP

#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <type_traits>

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

namespace detail {

template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

template <typename T>
inline constexpr bool is_coconext_unsigned_v = false;

template <typename T>
inline constexpr bool is_coconext_signed_v = false;

// Niebloid that reads a HasBits type's packed storage. Implementers declare
// `friend struct detail::bits_fn;` and keep `value_` private; ADL cannot find
// this call because `bits` is an object, so users can only reach it via the
// qualified `detail::bits(x)`.
struct bits_fn {
    template <typename T>
    constexpr auto const& operator()(T const& t) const noexcept {
        return t.value_;
    }
};

inline constexpr bits_fn bits{};

template <typename T>
concept HasBits = requires(T const& t) {
    T::static_range;
    { detail::bits(t) };
};

template <typename T>
concept Formattable = std::semiregular<std::formatter<std::remove_cvref_t<T>, char>>;

}  // namespace detail

}  // namespace coconext::types

#endif

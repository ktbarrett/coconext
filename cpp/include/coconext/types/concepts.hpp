#ifndef COCONEXT_TYPES_UTIL_HPP
#define COCONEXT_TYPES_UTIL_HPP

#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
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
struct is_int : public std::false_type {};

template <typename T>
concept Integer = is_int<std::remove_cv_t<T>>::value;

template <>
struct is_int<signed char> : public std::true_type {};
template <>
struct is_int<unsigned char> : public std::true_type {};
template <>
struct is_int<short> : public std::true_type {};
template <>
struct is_int<unsigned short> : public std::true_type {};
template <>
struct is_int<int> : public std::true_type {};
template <>
struct is_int<unsigned int> : public std::true_type {};
template <>
struct is_int<long> : public std::true_type {};
template <>
struct is_int<unsigned long> : public std::true_type {};
template <>
struct is_int<long long> : public std::true_type {};
template <>
struct is_int<unsigned long long> : public std::true_type {};

namespace detail {

template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

// C++20 substitute for std::formattable (C++23). Strong enough to discriminate
// types with a usable std::formatter specialization from those that fall back
// to the disabled primary template.
template <typename T>
concept Formattable = std::semiregular<std::formatter<std::remove_cvref_t<T>, char>>;

}  // namespace detail

}  // namespace coconext::types

#endif

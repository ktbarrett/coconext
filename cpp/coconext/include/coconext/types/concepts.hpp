#ifndef COCONEXT_TYPES_UTIL_HPP
#define COCONEXT_TYPES_UTIL_HPP

#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <stdexcept>
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

template <typename ShiftType>
constexpr size_t normalize_shift_amount(ShiftType const& shift_amount, size_t limit) {
    using CleanType = std::remove_cvref_t<ShiftType>;

    static_assert(
        std::is_integral_v<CleanType> || is_coconext_unsigned_v<CleanType>
            || is_coconext_signed_v<CleanType>,
        "Shift amount can only be a native integer, Signed, or Unsigned"
    );

    if constexpr (std::is_integral_v<CleanType>) {
        if constexpr (std::is_signed_v<CleanType>) {
            if (shift_amount < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        if constexpr (
            std::numeric_limits<CleanType>::digits > std::numeric_limits<size_t>::digits
        )
        {
            if (shift_amount > static_cast<CleanType>(std::numeric_limits<size_t>::max())) {
                return limit;
            }
        }

        size_t const value = static_cast<size_t>(shift_amount);
        return value < limit ? value : limit;
    } else if constexpr (
        is_coconext_unsigned_v<CleanType> || is_coconext_signed_v<CleanType>
    )
    {
        constexpr size_t width = CleanType::size();
        if constexpr (is_coconext_signed_v<CleanType> && width > 0) {
            if (bits(shift_amount).get_bit(width - 1)) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        if constexpr (width == 0) {
            return 0;
        } else {
            // Accumulate only up to the operand width. Every larger value has
            // the same shift result, so the shift count never needs narrowing.
            size_t value = 0;
            for (size_t bit_pos = width; bit_pos > 0; --bit_pos) {
                if (value > limit / 2) {
                    return limit;
                }
                value *= 2;
                if (value >= limit) {
                    return limit;
                }
                if (bits(shift_amount).get_bit(bit_pos - 1)) {
                    ++value;
                    if (value >= limit) {
                        return limit;
                    }
                }
            }
            return value;
        }
    } else {
        return 0;  // The static assertion above reports the unsupported type.
    }
}

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

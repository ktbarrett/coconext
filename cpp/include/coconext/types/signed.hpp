#ifndef COCONEXT_SIGNED_HPP
#define COCONEXT_SIGNED_HPP

#include <algorithm>
#include <coconext/types/concepts.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/int_base.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/range.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace coconext::types {

template <auto... Args, typename X>
    requires(sizeof...(Args) > 0 && detail::is_coconext_signed_v<std::remove_cvref_t<X>>)
constexpr auto resize(
    X&& x, overflow_mode ovf = overflow_mode::wrap, round_mode rnd = round_mode::truncate
);

namespace detail {

// for cross type constructor
template <Range R>
class Unsigned;

template <Range R>
class Signed {
    static_assert(R.length() >= 0, "Signed width must not be negative");

    template <typename T>
    constexpr T to_native_int() const {
        static_assert(
            R.length() > 0, "Signed<0> has no integer value; cannot convert to native int"
        );
        static_assert(
            !detail::Bits<R.length()>::is_wide, "Conversion from wide Bits to native int"
        );

        auto ext = value_.sra(0).raw();
        using SType = std::make_signed_t<decltype(ext)>;
        SType signed_val = static_cast<SType>(ext << (sizeof(ext) * 8 - R.length()))
                        >> (sizeof(ext) * 8 - R.length());

        if constexpr (
            R.length() - 1 > std::numeric_limits<T>::digits || std::is_unsigned_v<T>
        )
        {
            if (signed_val < std::numeric_limits<T>::min()
                || signed_val > std::numeric_limits<T>::max())
            {
                throw std::out_of_range("Value outside destination native type range");
            }
        }

        return static_cast<T>(signed_val);
    }

  public:
    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return R; }
    static constexpr size_t size() noexcept { return R.length(); }

    template <Range R2>
    friend class Signed;

    constexpr Signed() noexcept = default;

    template <size_t W>
    constexpr Signed(Bits<W> const& val) {
        static_assert(W == R.length(), "Construction from Bits requires identical width");
        value_ = val;
    }

    template <NativeInteger T>
    explicit(
        (std::is_signed_v<T> && std::numeric_limits<T>::digits >= R.length())
        || (std::is_unsigned_v<T> && std::numeric_limits<T>::digits >= R.length() - 1)
    ) constexpr Signed(T v) {
        static_assert(
            R.length() > 0, "Signed<0> has no integer representation; use Signed<0>{}"
        );
        if constexpr (std::is_unsigned_v<T>) {
            if (v > std::numeric_limits<T>::max()) {
                throw std::out_of_range("Unsigned value does not fit in Signed width");
            }
        } else {
            if constexpr (std::numeric_limits<T>::digits >= R.length()) {
                long long max_val = (1ULL << (R.length() - 1)) - 1;
                long long min_val = -(1LL << (R.length() - 1));
                if (v < min_val || v > max_val) {
                    throw std::out_of_range("Signed value does not fit in Signed width");
                }
            }
        }

        value_ = detail::Bits<R.length()>(static_cast<uint64_t>(v));
        if constexpr (R.length() > 64) {
            if constexpr (std::is_signed_v<T>) {
                if (v < 0) {
                    Signed<R> temp(value_);
                    temp = temp << (R.length() - 64);
                    temp = temp >> (R.length() - 64);
                    value_ = temp.value_;
                }
            }
        }
    }

    template <Range R2>
    explicit(R.length() < R2.length()) constexpr Signed(Signed<R2> const& other) {
        if constexpr (R.length() >= R2.length()) {
            value_ = coconext::types::resize<R.length()>(other).value_;
        } else {
            value_ = coconext::types::resize<R.length()>(other).value_;
            if (coconext::types::resize<R2.length()>(Signed<R>(value_)) != other) {
                throw std::out_of_range(
                    "Value does not fit in narrowing Signed conversion"
                );
            }
        }
    }

    template <Range R2>
    explicit(R.length() <= R2.length()) constexpr Signed(Unsigned<R2> const& other) {
        auto const& src_bits = bits(other);
        if constexpr (R.length() <= R2.length()) {
            // The sign bit costs one bit, so the source must fit in R-1 bits:
            // every bit at or above R-1 has to be clear.
            for (size_t i = R.length() - 1; i < R2.length(); ++i) {
                if (src_bits.get_bit(i)) {
                    throw std::out_of_range(
                        "Unsigned value does not fit in narrowing Signed conversion"
                    );
                }
            }
        }

        if constexpr (R.length() >= R2.length()) {
            value_ = src_bits.template zero_extend<R.length()>();
        } else {
            value_ = src_bits.template truncate<R.length()>();
        }
    }

    template <Range R2>
    explicit constexpr Signed(detail::Array<Bit, R2> const& other) {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );
        detail::Bits<R.length()> temp_bit(0);
        for (auto const& bit : other) {
            temp_bit = bit ? 1 : 0;
            value_ = (value_ << 1) | temp_bit;
        }
    }

    template <Range R2>
    constexpr operator detail::Array<Bit, R2>() const noexcept {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );
        return detail::Array<Bit, R2>(value_);
    }

    template <typename SourceT>
    constexpr Signed(auto_reinterpreted<SourceT>&& wrapper) {
        *this = as<Signed<R>>(std::move(wrapper).consume());
    }

    template <typename SourceT>
    constexpr Signed& operator=(auto_reinterpreted<SourceT>&& wrapper) {
        *this = as<Signed<R>>(std::move(wrapper).consume());
        return *this;
    }

    template <typename SourceWrapper>
    constexpr Signed(detail::auto_resized<SourceWrapper>&& wrapper) {
        auto [src, ovf, rnd] = std::move(wrapper).consume();
        using ActualSource = std::remove_cvref_t<SourceWrapper>;

        static_assert(
            detail::is_coconext_signed_v<ActualSource>,
            "resize() target and source must both be signed. Use as() for cross-type "
            "conversions."
        );

        constexpr size_t TargetW = R.length();
        constexpr size_t SourceW = ActualSource::size();

        if constexpr (TargetW >= SourceW) {
            // Widening (and the null cases) never lose information.
            value_ = src.value_.template sign_extend<TargetW>();
        } else if (ovf == overflow_mode::wrap) {
            value_ = src.value_.template truncate<TargetW>();
        } else {
            value_ = src.value_.template saturate_signed<TargetW>();
        }
    }

    template <typename SourceWrapper>
    constexpr Signed& operator=(detail::auto_resized<SourceWrapper>&& wrapper) {
        *this = Signed(std::move(wrapper));
        return *this;
    }

    friend constexpr bool operator==(Signed const& lhs, Signed const& rhs) noexcept {
        return lhs.value_ == rhs.value_;
    }

    friend constexpr auto operator<(Signed const& lhs, Signed const& rhs) noexcept {
        return lhs.value_.slt(rhs.value_);
    }

    friend constexpr auto operator<=(Signed const& lhs, Signed const& rhs) noexcept {
        return lhs.value_.sle(rhs.value_);
    }

    friend constexpr auto operator>(Signed const& lhs, Signed const& rhs) noexcept {
        return lhs.value_.sgt(rhs.value_);
    }

    friend constexpr auto operator>=(Signed const& lhs, Signed const& rhs) noexcept {
        return lhs.value_.sge(rhs.value_);
    }

    explicit constexpr operator bool() const noexcept {
        return this->value_ != detail::Bits<R.length()>{};
    }

    explicit constexpr operator signed char() const noexcept(
        R.length() - 1 <= std::numeric_limits<signed char>::digits
    ) {
        return to_native_int<signed char>();
    }
    explicit constexpr operator unsigned char() const noexcept(false) {
        return to_native_int<unsigned char>();
    }
    explicit constexpr operator short() const noexcept(
        R.length() - 1 <= std::numeric_limits<short>::digits
    ) {
        return to_native_int<short>();
    }
    explicit constexpr operator unsigned short() const noexcept(false) {
        return to_native_int<unsigned short>();
    }
    explicit constexpr operator int() const noexcept(
        R.length() - 1 <= std::numeric_limits<int>::digits
    ) {
        return to_native_int<int>();
    }
    explicit constexpr operator unsigned int() const noexcept(false) {
        return to_native_int<unsigned int>();
    }
    explicit constexpr operator long() const noexcept(
        R.length() - 1 <= std::numeric_limits<long>::digits
    ) {
        return to_native_int<long>();
    }
    explicit constexpr operator unsigned long() const noexcept(false) {
        return to_native_int<unsigned long>();
    }
    explicit constexpr operator long long() const noexcept(
        R.length() - 1 <= std::numeric_limits<long long>::digits
    ) {
        return to_native_int<long long>();
    }
    explicit constexpr operator unsigned long long() const noexcept(false) {
        return to_native_int<unsigned long long>();
    }

#if defined(__SIZEOF_INT128__)
    explicit constexpr operator __int128_t() const noexcept(
        R.length() - 1 <= (__SIZEOF_INT128__ * 8) - 1
    ) {
        return to_native_int<__int128_t>();
    }
    explicit constexpr operator __uint128_t() const noexcept(false) {
        return to_native_int<__uint128_t>();
    }
#endif

    template <typename ShiftType>
    constexpr Signed<R> operator<<(ShiftType const& shift_amount) const {
        using CleanType = std::remove_cvref_t<ShiftType>;

        static_assert(
            std::is_integral_v<CleanType> || detail::is_coconext_unsigned_v<CleanType>
                || detail::is_coconext_signed_v<CleanType>,
            "Shift amount can only be a native integer, Signed, or Unsigned"
        );

        size_t safe_shift = 0;

        if constexpr (std::is_integral_v<CleanType>) {
            if constexpr (std::is_signed_v<CleanType>) {
                if (shift_amount < 0) {
                    throw std::invalid_argument("Negative shift amount");
                }
            }
            safe_shift = static_cast<size_t>(shift_amount);
        } else if constexpr (detail::is_coconext_unsigned_v<CleanType>) {
            static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
            safe_shift = static_cast<size_t>(static_cast<unsigned long long>(shift_amount));
        } else if constexpr (detail::is_coconext_signed_v<CleanType>) {
            static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
            long long signed_val = static_cast<long long>(shift_amount);
            if (signed_val < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
            safe_shift = static_cast<size_t>(signed_val);
        }

        if (safe_shift >= R.length()) {
            return Signed<R>(0);
        }

        return Signed<R>(value_ << safe_shift);
    }

    template <typename ShiftType>
    constexpr Signed<R> operator>>(ShiftType const& shift_amount) const {
        using CleanType = std::remove_cvref_t<ShiftType>;

        static_assert(
            std::is_integral_v<CleanType> || detail::is_coconext_unsigned_v<CleanType>
                || detail::is_coconext_signed_v<CleanType>,
            "Shift amount can only be a native integer, Signed, or Unsigned"
        );

        size_t safe_shift = 0;

        if constexpr (std::is_integral_v<CleanType>) {
            if constexpr (std::is_signed_v<CleanType>) {
                if (shift_amount < 0) {
                    throw std::invalid_argument("Negative shift amount");
                }
            }
            safe_shift = static_cast<size_t>(shift_amount);
        } else if constexpr (detail::is_coconext_unsigned_v<CleanType>) {
            static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
            safe_shift = static_cast<size_t>(static_cast<unsigned long long>(shift_amount));
        } else if constexpr (detail::is_coconext_signed_v<CleanType>) {
            static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
            long long signed_val = static_cast<long long>(shift_amount);
            if (signed_val < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
            safe_shift = static_cast<size_t>(signed_val);
        }

        if (safe_shift >= R.length()) {
            return Signed<R>(value_.sra(R.length() > 0 ? R.length() - 1 : 0));
        }

        return Signed<R>(value_.sra(safe_shift));
    }

    template <typename ShiftType>
    constexpr Signed<R>& operator<<=(ShiftType const& amt) {
        *this = *this << amt;
        return *this;
    }
    template <typename ShiftType>
    constexpr Signed<R>& operator>>=(ShiftType const& amt) {
        *this = *this >> amt;
        return *this;
    }

    constexpr auto operator+() const { return *this; }

    constexpr auto operator-() const {
        constexpr Range R_res = detail::int_downto_range(R.length() + 1);
        return Signed<R_res>(detail::negate_signed(value_));
    }

    template <Range R2>
    constexpr auto operator+(Signed<R2> const& rhs) const {
        constexpr Range R_res =
            detail::int_downto_range(std::max(R.length(), R2.length()) + 1);
        return Signed<R_res>(detail::add_signed(value_, rhs.value_));
    }

    template <Range R2>
    constexpr auto operator-(Signed<R2> const& rhs) const {
        constexpr Range R_res =
            detail::int_downto_range(std::max(R.length(), R2.length()) + 1);
        return Signed<R_res>(detail::sub_signed(value_, rhs.value_));
    }

    template <Range R2>
    constexpr auto operator*(Signed<R2> const& rhs) const {
        constexpr Range R_res = detail::int_downto_range(R.length() + R2.length());
        return Signed<R_res>(detail::mul_signed(value_, rhs.value_));
    }

    template <Range R2>
    constexpr auto operator/(Signed<R2> const& rhs) const {
        constexpr Range R_res = detail::int_downto_range(R.length() + 1);
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return Signed<R_res>(detail::div_signed(value_, rhs.value_));
    }

    template <Range R2>
    constexpr auto operator%(Signed<R2> const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return Signed<R2>(detail::rem_signed(value_, rhs.value_));
    }

    template <Range R2>
    constexpr Signed& operator+=(Signed<R2> const& rhs) {
        *this = coconext::types::resize<R.length()>(*this + rhs);
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator-=(Signed<R2> const& rhs) {
        *this = coconext::types::resize<R.length()>(*this - rhs);
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator*=(Signed<R2> const& rhs) {
        *this = coconext::types::resize<R.length()>(*this * rhs);
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator/=(Signed<R2> const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        *this = coconext::types::resize<R.length()>(*this / rhs);
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator%=(Signed<R2> const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        *this = coconext::types::resize<R.length()>(*this % rhs);
        return *this;
    }

    template <NativeInteger T>
    constexpr Signed& operator+=(T const& rhs) {
        if constexpr (R.length() > 0) {
            *this = coconext::types::resize<R.length()>(
                *this + Signed<make_int_range<R.length()>()>(rhs)
            );
        }
        return *this;
    }

    template <NativeInteger T>
    constexpr Signed& operator-=(T const& rhs) {
        if constexpr (R.length() > 0) {
            *this = coconext::types::resize<R.length()>(
                *this - Signed<make_int_range<R.length()>()>(rhs)
            );
        }
        return *this;
    }

    template <NativeInteger T>
    constexpr Signed& operator*=(T const& rhs) {
        if constexpr (R.length() > 0) {
            *this = coconext::types::resize<R.length()>(
                *this * Signed<make_int_range<R.length()>()>(rhs)
            );
        }
        return *this;
    }

    template <NativeInteger T>
    constexpr Signed& operator/=(T const& rhs) {
        if constexpr (R.length() > 0) {
            *this = coconext::types::resize<R.length()>(
                *this / Signed<make_int_range<R.length()>()>(rhs)
            );
        }
        return *this;
    }

    template <NativeInteger T>
    constexpr Signed& operator%=(T const& rhs) {
        if constexpr (R.length() > 0) {
            *this = coconext::types::resize<R.length()>(
                *this % Signed<make_int_range<R.length()>()>(rhs)
            );
        }
        return *this;
    }

    template <Range R2>
    constexpr Signed& operator+=(Unsigned<R2> const& rhs) {
        auto res = coconext::types::resize<R.length()>(*this + (+rhs), overflow_mode::wrap);
        *this = Signed<R>(static_cast<detail::Array<Bit, R>>(res));
        return *this;
    }

    template <Range R2>
    constexpr Signed& operator-=(Unsigned<R2> const& rhs) {
        auto res = coconext::types::resize<R.length()>(*this - (+rhs), overflow_mode::wrap);
        *this = Signed<R>(static_cast<detail::Array<Bit, R>>(res));
        return *this;
    }

    template <Range R2>
    constexpr Signed& operator*=(Unsigned<R2> const& rhs) {
        auto res = coconext::types::resize<R.length()>(*this * (+rhs), overflow_mode::wrap);
        *this = Signed<R>(static_cast<detail::Array<Bit, R>>(res));
        return *this;
    }

    template <Range R2>
    constexpr Signed& operator/=(Unsigned<R2> const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        auto res = coconext::types::resize<R.length()>(*this / (+rhs), overflow_mode::wrap);
        *this = Signed<R>(static_cast<detail::Array<Bit, R>>(res));
        return *this;
    }

    template <Range R2>
    constexpr Signed& operator%=(Unsigned<R2> const& rhs) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        auto res = coconext::types::resize<R.length()>(*this % (+rhs), overflow_mode::wrap);
        *this = Signed<R>(static_cast<detail::Array<Bit, R>>(res));
        return *this;
    }

    constexpr Signed& operator++() {
        *this += 1;
        return *this;
    }

    constexpr Signed operator++(int) {
        Signed tmp = *this;
        *this += 1;
        return tmp;
    }

    constexpr Signed& operator--() {
        *this -= 1;
        return *this;
    }

    constexpr Signed operator--(int) {
        Signed tmp = *this;
        *this -= 1;
        return tmp;
    }

    constexpr auto begin() const { return value_.begin(); }
    constexpr auto rbegin() const { return value_.rbegin(); }

    constexpr auto end() const { return value_.end(); }
    constexpr auto rend() const { return value_.rend(); }

    constexpr auto operator[](Range::value_type idx) const {
        auto const offset = offset_of(R, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        size_t bit_pos = R.length() - 1 - offset.value();

        return value_[bit_pos];
    }

    template <size_t N>
    constexpr auto index() const {
        return value_[N];
    }

    template <typename T, typename CharT>
    friend struct std::formatter;
    template <typename T>
    friend struct std::hash;

  private:
    friend struct bits_fn;

    Bits<R.length()> value_{};
};

template <Range R>
inline constexpr bool is_coconext_signed_v<Signed<R>> = true;

template <Range R1, Range R2>
constexpr auto operator+(Unsigned<R1> const& lhs, Signed<R2> const& rhs) {
    return (+lhs) + rhs;
}

template <Range R1, Range R2>
constexpr auto operator-(Unsigned<R1> const& lhs, Signed<R2> const& rhs) {
    return (+lhs) - rhs;
}

template <Range R1, Range R2>
constexpr auto operator*(Unsigned<R1> const& lhs, Signed<R2> const& rhs) {
    return (+lhs) * rhs;
}

template <Range R1, Range R2>
constexpr auto operator/(Unsigned<R1> const& lhs, Signed<R2> const& rhs) {
    return (+lhs) / rhs;
}

template <Range R1, Range R2>
constexpr auto operator%(Unsigned<R1> const& lhs, Signed<R2> const& rhs) {
    return (+lhs) % rhs;
}

template <Range R1, Range R2>
constexpr auto operator+(Signed<R1> const& lhs, Unsigned<R2> const& rhs) {
    return lhs + (+rhs);
}

template <Range R1, Range R2>
constexpr auto operator-(Signed<R1> const& lhs, Unsigned<R2> const& rhs) {
    return lhs - (+rhs);
}

template <Range R1, Range R2>
constexpr auto operator*(Signed<R1> const& lhs, Unsigned<R2> const& rhs) {
    return lhs * (+rhs);
}

template <Range R1, Range R2>
constexpr auto operator/(Signed<R1> const& lhs, Unsigned<R2> const& rhs) {
    return lhs / (+rhs);
}

template <Range R1, Range R2>
constexpr auto operator%(Signed<R1> const& lhs, Unsigned<R2> const& rhs) {
    return lhs % (+rhs);
}

}  // namespace detail

template <auto... Args>
using Signed = detail::Signed<detail::make_int_range<Args...>()>;

template <auto... Args, typename X>
    requires(sizeof...(Args) > 0 && detail::is_coconext_signed_v<std::remove_cvref_t<X>>)
constexpr auto resize(X&& x, overflow_mode ovf, round_mode rnd) {
    constexpr Range TargetRange = detail::make_int_range<Args...>();
    return Signed<TargetRange>(resize(std::forward<X>(x), ovf, rnd));
}

template <Range R>
constexpr auto abs(detail::Signed<R> const& s) {
    if (s < detail::Signed<R>(0)) {
        return -s;
    }
    return resize<R.length() + 1>(s);
}

consteval Signed<8> s8(long long v) { return Signed<8>(v); }
consteval Signed<16> s16(long long v) { return Signed<16>(v); }
consteval Signed<32> s32(long long v) { return Signed<32>(v); }
consteval Signed<64> s64(long long v) { return Signed<64>(v); }

}  // namespace coconext::types

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Signed<R>> {
    char presentation = 'd';

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') {
            presentation = *it++;
            if (presentation != 'd' && presentation != 'b' && presentation != 'o'
                && presentation != 'x')
            {
                throw std::format_error("Invalid format specifier for Signed");
            }
        }
        if (it != end && *it != '}') {
            throw std::format_error("Invalid format string");
        }
        return it;
    }

    auto format(
        coconext::types::detail::Signed<R> const& v, std::format_context& ctx
    ) const {
        std::string str_r;
        switch (presentation) {
        case 'b':
            str_r = v.value_.to_binary_string();
            break;
        case 'o':
            str_r = v.value_.to_octal_string();
            break;
        case 'x':
            str_r = v.value_.to_hexadecimal_string();
            break;
        default:
            str_r = v.value_.to_decimal_string(true);
        }
        return std::format_to(ctx.out(), "Signed{}{{{}}}", R, str_r);
    }
};

template <coconext::types::Range R>
struct std::hash<coconext::types::detail::Signed<R>> {
    size_t operator()(coconext::types::detail::Signed<R> const& v) const noexcept {
        std::string_view type_name = typeid(coconext::types::detail::Signed<R>).name();
        size_t signed_seed = std::hash<std::string_view>{}(type_name);
        constexpr size_t W = R.length();
        size_t value_hash = 0;

        if constexpr (W > 0) {
            if constexpr (!coconext::types::detail::Bits<W>::is_wide) {
                auto raw_val = v.value_.raw();
                if constexpr (sizeof(raw_val) > sizeof(size_t)) {
                    uint64_t low = static_cast<uint64_t>(raw_val);
                    uint64_t high = static_cast<uint64_t>(raw_val >> 64);
                    value_hash = coconext::types::detail::hash_combine(low, high);
                } else {
                    value_hash = std::hash<decltype(raw_val)>{}(raw_val);
                }
            } else {
                auto val = v.value_.raw();
                constexpr size_t num_words = (W + 63) / 64;
                for (size_t i = 0; i < num_words; ++i) {
                    value_hash =
                        coconext::types::detail::hash_combine(value_hash, val.word(i));
                }
            }
        }

        return coconext::types::detail::hash_combine(signed_seed, R, value_hash);
    }
};

#endif  // COCONEXT_SIGNED_HPP

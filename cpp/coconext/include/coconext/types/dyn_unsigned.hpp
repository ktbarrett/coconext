#ifndef COCONEXT_DYN_UNSIGNED_HPP
#define COCONEXT_DYN_UNSIGNED_HPP

#include <coconext/types/bits.hpp>
#include <coconext/types/concepts.hpp>
#include <coconext/types/dyn_bits.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/range.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>

namespace coconext::types::detail {

// template <auto... Args, typename X>
//     requires(sizeof...(Args) > 0 && is_coconext_unsigned_v<std::remove_cvref_t<X>>)
// constexpr auto resize(
//     X&& x, overflow_mode ovf = overflow_mode::wrap, round_mode rnd = round_mode::truncate
// );

class DynSigned;

class DynUnsigned {
    // static_assert(R.length() >= 0, "Unsigned width must not be negative");

    template <typename T>
    T to_native_int() const {
        auto val = value_;
        if (val.ugt(DynBits(val.get_width(), std::numeric_limits<T>::max()))) {
            throw std::out_of_range("Value too large for destination native type");
        }

        return static_cast<T>(val.raw().word(0));
    }

  public:
    // static constexpr Range static_range = R;
    // static constexpr Range range() noexcept { return R; }
    // static constexpr size_t size() noexcept { return R.length(); }

    DynUnsigned(DynBits const& val) : value_(val) {}

    size_t get_width() const { return value_.get_width(); }

    // Construct from a native integer.
    template <NativeInteger T>
    DynUnsigned(size_t width, T v) : value_(width, v) {
        if constexpr (std::is_signed_v<T>) {
            if (v < 0) {
                throw std::overflow_error("negative value in Unsigned construction");
            }
        }

        if (std::numeric_limits<T>::digits > width) {
            using unsigned_T = std::make_unsigned_t<T>;
            unsigned_T max_unsigned = (width >= sizeof(unsigned_T) * 8)
                                        ? static_cast<unsigned_T>(-1)
                                        : (static_cast<unsigned_T>(1) << width) - 1;

            if (static_cast<unsigned_T>(v) > max_unsigned) {
                throw std::overflow_error("value does not fit in Unsigned width");
            }
        }
    }

    //     // Cross-width conversion. Throws if the source value doesn't fit in N bits.
    //     template <Range R2>
    //     explicit(R.length() < R2.length()) constexpr Unsigned(Unsigned<R2> const& other)
    //     {
    //         if constexpr (R.length() >= R2.length()) {
    //             value_ = other.value_;
    //         } else if (
    //             bits(other).ule((~Bits<R.length()>{}).template
    //             zero_extend<R2.length()>())
    //         )
    //         {
    //             value_ = coconext::types::resize<R.length()>(other).value_;
    //         } else {
    //             throw std::out_of_range("value does not fit in Unsigned width");
    //         }
    //     }

    //     // Cross-width conversion from DynSigned. Throws if the source value doesn't fit
    //     in N bits
    //     // or is negative.
    //     template <Range R2>
    //     explicit constexpr Unsigned(DynSigned<R2> const& other) {
    //         auto const& src_bits = bits(other);
    //         if constexpr (R2.length() > 0) {
    //             if (src_bits.get_bit(R2.length() - 1)) {
    //                 throw std::out_of_range(
    //                     "Cannot construct Unsigned from negative DynSigned value"
    //                 );
    //             }
    //         }
    //         *this = Unsigned<R>(Unsigned<R2>(src_bits));
    //     }

    // Construct from a BitArray. Throws if the source value is not exactly N bits.
    // explicit constexpr Unsigned(Array<Bit, R2> const& other) {
    //     static_assert(
    //         R.length() == R2.length(), "BitArray reinterpret requires identical width"
    //     );

    //     value_ = bits(other);
    // }

    //     // Implicit conversion to supertype BitArray
    //     template <Range R2>
    //     constexpr operator Array<Bit, R2>() const noexcept {
    //         static_assert(
    //             R.length() == R2.length(), "BitArray reinterpret requires identical
    //             width"
    //         );
    //         return Array<Bit, R2>(value_);
    //     }

    //     // Consume deduced-target reinterpret wrapper
    //     template <typename SourceT>
    //     constexpr Unsigned(auto_reinterpreted<SourceT>&& wrapper) {
    //         *this = as<Unsigned<R>>(std::move(wrapper).consume());
    //     }

    //     template <typename SourceT>
    //     constexpr Unsigned& operator=(auto_reinterpreted<SourceT>&& wrapper) {
    //         *this = as<Unsigned<R>>(std::move(wrapper).consume());
    //         return *this;
    //     }

    //     template <typename SourceWrapper>
    //     constexpr Unsigned(auto_resized<SourceWrapper>&& wrapper) {
    //         auto [src, ovf, rnd] = std::move(wrapper).consume();
    //         using ActualSource = std::remove_cvref_t<SourceWrapper>;

    //         static_assert(
    //             is_coconext_unsigned_v<ActualSource>,
    //             "reR.length() target and source must both be Unsigned. Use as() for
    //             cross-type " "conversions."
    //         );

    //         constexpr size_t TargetW = R.length();
    //         constexpr size_t SourceW = ActualSource::size();

    //         if constexpr (TargetW >= SourceW) {
    //             // Widening (and the null cases) never lose information.
    //             value_ = bits(src).template zero_extend<TargetW>();
    //         } else if (ovf == overflow_mode::wrap) {
    //             value_ = bits(src).template truncate<TargetW>();
    //         } else {
    //             value_ = bits(src).template saturate_unsigned<TargetW>();
    //         }
    //     }

    //     template <typename SourceWrapper>
    //     constexpr Unsigned& operator=(auto_resized<SourceWrapper>&& wrapper) {
    //         *this = Unsigned(std::move(wrapper));
    //         return *this;
    //     }

    bool operator==(DynUnsigned const& rhs) const noexcept { return value_ == rhs.value_; }

    bool operator<(DynUnsigned const& rhs) const noexcept { return value_.ult(rhs.value_); }

    bool operator<=(DynUnsigned const& rhs) const noexcept {
        return value_.ule(rhs.value_);
    }

    bool operator>(DynUnsigned const& rhs) const noexcept { return value_.ugt(rhs.value_); }

    bool operator>=(DynUnsigned const& rhs) const noexcept {
        return value_.uge(rhs.value_);
    }

    explicit operator bool() const noexcept {
        return value_ != DynBits{value_.get_width(), 0};
    }

    explicit operator long long() const { return to_native_int<long long>(); }

    //     template <typename ShiftType>
    //     constexpr Unsigned<R> operator<<(ShiftType const& shift_amount) const {
    //         using CleanType = std::remove_cvref_t<ShiftType>;

    //         // Shift amounts must fit in a native integer.
    //         static_assert(
    //             std::is_integral_v<CleanType> || is_coconext_unsigned_v<CleanType>
    //                 || is_coconext_signed_v<CleanType>,
    //             "Shift amount can only be a native integer, DynSigned, or Unsigned"
    //         );

    //         size_t safe_shift = 0;

    //         if constexpr (std::is_integral_v<CleanType>) {
    //             if constexpr (std::is_signed_v<CleanType>) {
    //                 if (shift_amount < 0) {
    //                     throw std::invalid_argument("Negative shift amount");
    //                 }
    //             }
    //             safe_shift = static_cast<size_t>(shift_amount);
    //         } else if constexpr (is_coconext_unsigned_v<CleanType>) {
    //             static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
    //             safe_shift = static_cast<size_t>(static_cast<unsigned long
    //             long>(shift_amount));
    //         } else if constexpr (is_coconext_signed_v<CleanType>) {
    //             static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
    //             long long signed_val = static_cast<long long>(shift_amount);
    //             if (signed_val < 0) {
    //                 throw std::invalid_argument("Negative shift amount");
    //             }
    //             safe_shift = static_cast<size_t>(signed_val);
    //         }

    //         if (safe_shift >= R.length()) {
    //             return Unsigned<R>(0);
    //         }

    //         return Unsigned<R>(value_ << safe_shift);
    //     }

    //     template <typename ShiftType>
    //     constexpr Unsigned<R> operator>>(ShiftType const& shift_amount) const {
    //         using CleanType = std::remove_cvref_t<ShiftType>;

    //         // Shift amounts must fit in a native integer.
    //         // thing
    //         static_assert(
    //             std::is_integral_v<CleanType> || is_coconext_unsigned_v<CleanType>
    //                 || is_coconext_signed_v<CleanType>,
    //             "Shift amount can only be a native integer, DynSigned, or Unsigned"
    //         );

    //         size_t safe_shift = 0;

    //         if constexpr (std::is_integral_v<CleanType>) {
    //             if constexpr (std::is_signed_v<CleanType>) {
    //                 if (shift_amount < 0) {
    //                     throw std::invalid_argument("Negative shift amount");
    //                 }
    //             }
    //             safe_shift = static_cast<size_t>(shift_amount);
    //         } else if constexpr (is_coconext_unsigned_v<CleanType>) {
    //             static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
    //             safe_shift = static_cast<size_t>(static_cast<unsigned long
    //             long>(shift_amount));
    //         } else if constexpr (is_coconext_signed_v<CleanType>) {
    //             static_assert(CleanType::size() < 64, "Bit Width cap 2**64");
    //             long long signed_val = static_cast<long long>(shift_amount);
    //             if (signed_val < 0) {
    //                 throw std::invalid_argument("Negative shift amount");
    //             }
    //             safe_shift = static_cast<size_t>(signed_val);
    //         }

    //         if (safe_shift >= R.length()) {
    //             return Unsigned<R>(0);
    //         }

    //         return Unsigned<R>(value_.srl(safe_shift));
    //     }

    //     template <typename ShiftType>
    //     constexpr Unsigned<R>& operator<<=(ShiftType const& shift_amount) {
    //         *this = *this << shift_amount;
    //         return *this;
    //     }

    //     template <typename ShiftType>
    //     constexpr Unsigned<R>& operator>>=(ShiftType const& shift_amount) {
    //         *this = *this >> shift_amount;
    //         return *this;
    //     }

    //     constexpr auto operator+() const {
    //         constexpr Range R_res = int_downto_range(R.length() + 1);
    //         return coconext::types::as<DynSigned<R_res>>(
    //             coconext::types::resize<R_res.length()>(*this)
    //         );
    //     }

    //     // The operand is unsigned, so it zero-extends into the wider result before
    //     // being negated: -Unsigned<8>(150) is -150, not -(-106).
    //     constexpr auto operator-() const {
    //         constexpr size_t Wr = R.length() + 1;
    //         return DynSigned<int_downto_range(Wr)>(
    //             Bits<Wr>{} - value_.template zero_extend<Wr>()
    //         );
    //     }

    auto operator+(DynUnsigned const& rhs) const {
        return DynUnsigned(add_unsigned(value_, rhs.value_));
    }

    auto operator*(DynUnsigned const& rhs) const {
        return DynUnsigned(mul_unsigned(value_, rhs.value_));
    }

    auto operator/(DynUnsigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynUnsigned(div_unsigned(value_, rhs.value_));
    }

    auto operator%(DynUnsigned const& rhs) const {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return DynUnsigned(rem_unsigned(value_, rhs.value_));
    }

    auto operator+=(DynUnsigned const& rhs) {
        value_ = add_unsigned(value_, rhs.value_);
        return *this;
    }

    //     template <Range R2>
    //     constexpr Unsigned& operator-=(Unsigned<R2> const& rhs) {
    //         auto res = coconext::types::resize<R.length()>(*this - rhs);
    //         *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator*=(Unsigned<R2> const& rhs) {
    //         *this = coconext::types::resize<R.length()>(*this * rhs);
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator/=(Unsigned<R2> const& rhs) {
    //         if (!static_cast<bool>(rhs)) {
    //             throw std::domain_error("Division by zero");
    //         }
    //         *this = coconext::types::resize<R.length()>(*this / rhs);
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator%=(Unsigned<R2> const& rhs) {
    //         if (!static_cast<bool>(rhs)) {
    //             throw std::domain_error("Division by zero");
    //         }
    //         *this = coconext::types::resize<R.length()>(*this % rhs);
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator+=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             *this = coconext::types::resize<R.length()>(
    //                 *this + Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //         }
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator-=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             auto res = coconext::types::resize<R.length()>(
    //                 *this - Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //             *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         }
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator*=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             *this = coconext::types::resize<R.length()>(
    //                 *this * Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //         }
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator/=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             *this = coconext::types::resize<R.length()>(
    //                 *this / Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //         }
    //         return *this;
    //     }

    //     template <NativeInteger T>
    //     constexpr Unsigned& operator%=(T const& rhs) {
    //         if constexpr (R.length() > 0) {
    //             *this = coconext::types::resize<R.length()>(
    //                 *this % Unsigned<make_int_range<R.length()>()>(rhs)
    //             );
    //         }
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator+=(DynSigned<R2> const& rhs) {
    //         auto res = coconext::types::resize<R.length()>(+(*this) + rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator-=(DynSigned<R2> const& rhs) {
    //         auto res = coconext::types::resize<R.length()>(+(*this) - rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator*=(DynSigned<R2> const& rhs) {
    //         auto res = coconext::types::resize<R.length()>(+(*this) * rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator/=(DynSigned<R2> const& rhs) {
    //         if (!static_cast<bool>(rhs)) {
    //             throw std::domain_error("Division by zero");
    //         }
    //         auto res = coconext::types::resize<R.length()>(+(*this) / rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     template <Range R2>
    //     constexpr Unsigned& operator%=(DynSigned<R2> const& rhs) {
    //         if (!static_cast<bool>(rhs)) {
    //             throw std::domain_error("Division by zero");
    //         }
    //         auto res = coconext::types::resize<R.length()>(+(*this) % rhs,
    //         overflow_mode::wrap); *this = Unsigned<R>(static_cast<Array<Bit, R>>(res));
    //         return *this;
    //     }

    //     constexpr Unsigned& operator++() {
    //         *this += 1;
    //         return *this;
    //     }

    //     constexpr Unsigned operator++(int) {
    //         Unsigned tmp = *this;
    //         *this += 1;
    //         return tmp;
    //     }

    //     constexpr Unsigned& operator--() {
    //         *this -= 1;
    //         return *this;
    //     }

    //     constexpr Unsigned operator--(int) {
    //         Unsigned tmp = *this;
    //         *this -= 1;
    //         return tmp;
    //     }

    //     constexpr auto begin() const { return value_.begin(); }
    //     constexpr auto rbegin() const { return value_.rbegin(); }

    //     constexpr auto end() const { return value_.end(); }
    //     constexpr auto rend() const { return value_.rend(); }

    //     constexpr auto operator[](Range::value_type idx) const {
    //         auto const offset = offset_of(R, idx);
    //         if (!offset.has_value()) {
    //             throw std::out_of_range("Index out of bounds");
    //         }
    //         size_t bit_pos = R.length() - 1 - offset.value();

    //         return value_[bit_pos];
    //     }

    //     template <size_t N>
    //     constexpr auto index() const {
    //         return value_[N];
    //     }

  private:
    friend struct bits_fn;
    DynBits value_;
};

// inline constexpr bool is_coconext_unsigned_v<DynUnsigned> = true;

// User-facing alias: accepts the same NTTP forms as Array<T, ...>, with HDL
// DOWNTO defaulting (see make_int_range for the rules).
// template <auto... Args>
// using Unsigned = Unsigned<make_int_range<Args...>()>;

// template <auto... Args, typename X>
//     requires(sizeof...(Args) > 0 && is_coconext_unsigned_v<std::remove_cvref_t<X>>)
// constexpr auto resize(X&& x, overflow_mode ovf, round_mode rnd) {
//     constexpr Range TargetRange = make_int_range<Args...>();
//     return Unsigned<TargetRange>(:resize(std::forward<X>(x), ovf, rnd));
// }

// consteval Unsigned<8> u8(unsigned long long v) { return Unsigned<8>(v); }
// consteval Unsigned<16> u16(unsigned long long v) { return Unsigned<16>(v); }
// consteval Unsigned<32> u32(unsigned long long v) { return Unsigned<32>(v); }
// consteval Unsigned<64> u64(unsigned long long v) { return Unsigned<64>(v); }

}  // namespace coconext::types::detail

// template <coconext::types::Range R>
// struct std::formatter<coconext::types::detail::Unsigned<R>> {
//     char presentation = 'd';

//     constexpr auto parse(std::format_parse_context& ctx) {
//         auto it = ctx.begin(), end = ctx.end();
//         if (it != end && *it != '}') {
//             presentation = *it++;
//             if (presentation != 'd' && presentation != 'b' && presentation != 'o'
//                 && presentation != 'x')
//             {
//                 throw std::format_error("Invalid format specifier for Unsigned");
//             }
//         }
//         if (it != end && *it != '}') {
//             throw std::format_error("Invalid format string");
//         }
//         return it;
//     }

//     auto format(
//         coconext::types::detail::Unsigned<R> const& v, std::format_context& ctx
//     ) const {
//         std::string str_r;
//         switch (presentation) {
//         case 'b':
//             str_r = coconext::types::detail::bits(v).to_binary_string();
//             break;
//         case 'o':
//             str_r = coconext::types::detail::bits(v).to_octal_string();
//             break;
//         case 'x':
//             str_r = coconext::types::detail::bits(v).to_hexadecimal_string();
//             break;
//         default:
//             str_r = coconext::types::detail::bits(v).to_decimal_string();
//         }
//         return std::format_to(ctx.out(), "Unsigned{}{{{}}}", R, str_r);
//     }
// };

// template <coconext::types::Range R>
// struct std::hash<coconext::types::detail::Unsigned<R>> {
//     size_t operator()(coconext::types::detail::Unsigned<R> const& v) const noexcept {
//         std::string_view type_name = typeid(coconext::types::detail::Unsigned<R>).name();
//         size_t unsigned_seed = std::hash<std::string_view>{}(type_name);
//         constexpr size_t W = R.length();
//         size_t value_hash = 0;

//         if constexpr (W > 0) {
//             if constexpr (!coconext::types::detail::Bits<W>::is_wide) {
//                 auto raw_val = coconext::types::detail::bits(v).raw();
//                 if constexpr (sizeof(raw_val) > sizeof(size_t)) {
//                     uint64_t low = static_cast<uint64_t>(raw_val);
//                     uint64_t high = static_cast<uint64_t>(raw_val >> 64);
//                     value_hash = coconext::types::detail::hash_combine(low, high);
//                 } else {
//                     value_hash = std::hash<decltype(raw_val)>{}(raw_val);
//                 }
//             } else {
//                 auto val = coconext::types::detail::bits(v).raw();
//                 constexpr size_t num_words = (W + 63) / 64;
//                 for (size_t i = 0; i < num_words; ++i) {
//                     value_hash =
//                         coconext::types::detail::hash_combine(value_hash, val.word(i));
//                 }
//             }
//         }

//         return coconext::types::detail::hash_combine(unsigned_seed, R, value_hash);
//     }
// };

#endif  // COCONEXT_DYN_UNSIGNED_HPP

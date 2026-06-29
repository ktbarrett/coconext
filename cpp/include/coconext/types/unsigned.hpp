#ifndef COCONEXT_UNSIGNED_HPP
#define COCONEXT_UNSIGNED_HPP

#include <algorithm>
#include <coconext/types/concepts.hpp>
#include <coconext/types/int_base.hpp>
#include <coconext/types/logic_array.hpp>
#include <coconext/types/range.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace coconext::types {

namespace detail {

template <Range R>
class Unsigned;

template <typename T>
struct is_unsigned_type : std::false_type {};

template <Range R>
struct is_unsigned_type<Unsigned<R>> : std::true_type {};

template <Range R>
class Unsigned {
    static_assert(R.length() >= 0, "Unsigned width must not be negative");

    template <typename T>
    constexpr T to_native_int() const {
        auto val = this->value_;
        if constexpr (size() > std::numeric_limits<T>::digits) {
            if (val > std::numeric_limits<T>::max()) {
                throw std::out_of_range("Value too large for destination native type");
            }
        }

        return static_cast<T>(val.raw());
    }

  public:
    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return R; }
    static constexpr size_t size() noexcept { return R.length(); }

    // Allow different width Unsigned templates to read our private value_ (required for
    // resize)
    template <Range R2>
    friend class Unsigned;

    constexpr Unsigned() noexcept : value_(0) {}

    template <size_t W>
    constexpr Unsigned(Bits<W> const& val) {
        static_assert(W == size(), "Construction from Bits requires identical width");
        value_ = val;
    }

    // Construct from a native integer. Throws std::out_of_range if the value is
    // negative or does not fit in R.length() bits.
    template <Integer T>
    explicit(
        std::is_signed_v<T> || std::numeric_limits<T>::digits > size()
    ) constexpr Unsigned(T v) {
        if constexpr (std::is_signed_v<T>) {
            if (v < 0) {
                throw std::out_of_range("negative value in Unsigned construction");
            }
        }

        if constexpr (std::numeric_limits<T>::digits <= size()) {
            value_ = v;
        } else {
            using unsigned_T = std::make_unsigned_t<T>;
            if (static_cast<unsigned_T>(v) > value_.template max_unsigned_native<size()>())
            {
                throw std::out_of_range("value does not fit in Unsigned width");
            }
            value_ = v;
        }
    }

    // Cross-width conversion. Throws if the source value doesn't fit in N bits.
    template <Range R2>
    explicit(size() < R2.length()) constexpr Unsigned(Unsigned<R2> const& other) {
        if constexpr (size() >= other.size()) {
            value_ = other.value_;
        } else {
            if constexpr (!other.value_::is_not_native_int) {
                if (other.value_ <= value_.template max_unsigned_native<size()>()) {
                    value_ = other.value_;
                } else {
                    throw std::out_of_range("value does not fit in Unsigned width");
                }
            } else {
                if (other.value_ <= value_.template max_unsigned_bigInt<size()>()) {
                    value_ = other.value_;
                } else {
                    throw std::out_of_range("value does not fit in Unsigned width");
                }
            }
        }
    }

    // Cross-width conversion from Signed. Throws if the source value doesn't fit in N bits.
    template <Range R2>
    explicit constexpr Unsigned(Signed<R2> const& other) {
        // TODO
    }

    // Construct from a BitArray. Throws if the source value is not exactly N bits.
    template <Range R2>
    explicit constexpr Unsigned(detail::Array<Bit, R2> const& other) {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );

        detail::Bits<size()> temp_bit(0);

        for (auto const& bit : other) {
            temp_bit = bit ? 1 : 0;
            value_ = (value_ << 1) | temp_bit;
        }
    }

    // Implicit conversion to supertype BitArray
    template <Range R2>
    constexpr operator detail::Array<Bit, R2>() const noexcept {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );

        BitArray<R2> temp;

        detail::Bits<size()> temp_bit(1);
        size_t bit_idx = size() - 1;

        for (auto& bit : temp) {
            if constexpr (!Bits<size()>::is_not_native_int) {
                bit = to_bit(((value_.srl(bit_idx)) & temp_bit).raw());
            } else {
                bit = to_bit(((value_.srl(bit_idx)) & temp_bit).raw().get_word(0));
            }

            if (bit_idx > 0) {
                bit_idx--;
            }
        }

        return temp;
    }

    // Consume deduced-target reinterpret wrapper
    template <typename SourceT>
    constexpr Unsigned(auto_reinterpreted<SourceT>&& wrapper) {
        *this = as<Unsigned<R>>(std::move(wrapper).consume());
    }

    template <typename SourceT>
    constexpr Unsigned& operator=(auto_reinterpreted<SourceT>&& wrapper) {
        *this = as<Unsigned<R>>(std::move(wrapper).consume());
        return *this;
    }

    template <typename SourceWrapper>
    constexpr Unsigned(detail::auto_resized<SourceWrapper>&& wrapper) {
        auto [src, ovf, rnd] = std::move(wrapper).consume();
        using ActualSource = std::remove_cvref_t<SourceWrapper>;

        static_assert(
            detail::is_unsigned_type<ActualSource>::value,
            "resize() target and source must both be Unsigned. Use as() for cross-type "
            "conversions."
        );

        constexpr size_t TargetW = size();
        constexpr size_t SourceW = ActualSource::size();

        if constexpr (TargetW >= SourceW) {
            if constexpr (
                !Bits<TargetW>::is_not_native_int
                && !detail::Bits<SourceW>::is_not_native_int
            )
            {
                value_ = detail::Bits<TargetW>(
                    static_cast<typename detail::Bits<TargetW>::IntType>(src.value_.raw())
                );
            } else {
                value_ = detail::Bits<TargetW>(src.value_.raw());
            }
        } else {
            // Narrowing
            if (ovf == overflow_mode::wrap) {
                if constexpr (
                    !Bits<TargetW>::is_not_native_int
                    && !detail::Bits<SourceW>::is_not_native_int
                )
                {
                    value_ = detail::Bits<TargetW>(
                        static_cast<typename detail::Bits<TargetW>::IntType>(
                            src.value_.raw()
                        )
                    );
                } else {
                    value_ = detail::Bits<TargetW>(src.value_.raw());
                }
            } else {
                // Saturate Clamp to the maximum representable value of the Target width
                if constexpr (
                    !Bits<TargetW>::is_not_native_int
                    && !detail::Bits<SourceW>::is_not_native_int
                )
                {
                    auto src_raw = src.value_.raw();
                    auto target_max = value_.template max_unsigned_native<TargetW>();

                    if (src_raw > target_max) {
                        value_ = detail::Bits<TargetW>(
                            static_cast<typename detail::Bits<TargetW>::IntType>(target_max)
                        );
                    } else {
                        value_ = detail::Bits<TargetW>(
                            static_cast<typename detail::Bits<TargetW>::IntType>(src_raw)
                        );
                    }
                } else {
                    // BigInt saturate fallback
                    auto src_raw = src.value_.raw();
                    auto target_max = value_.template max_unsigned_bigInt<TargetW>();
                    if (src_raw > target_max) {
                        value_ = detail::Bits<TargetW>(target_max);
                    } else {
                        value_ = detail::Bits<TargetW>(src_raw);
                    }
                }
            }
        }
    }

    template <typename SourceWrapper>
    constexpr Unsigned& operator=(detail::auto_resized<SourceWrapper>&& wrapper) {
        *this = Unsigned(std::move(wrapper));
        return *this;
    }

    friend constexpr bool operator==(Unsigned const& lhs, Unsigned const& rhs) noexcept {
        return lhs.value_ == rhs.value_;
    }

    friend constexpr auto operator<=>(Unsigned const& lhs, Unsigned const& rhs) noexcept {
        return lhs.value_ <=> rhs.value_;
    }

    explicit constexpr operator bool() const noexcept { return this->value_ != 0; }

    explicit constexpr operator signed char() const noexcept(
        size() <= std::numeric_limits<signed char>::digits
    ) {
        return to_native_int<signed char>();
    }
    explicit constexpr operator unsigned char() const noexcept(
        size() <= std::numeric_limits<unsigned char>::digits
    ) {
        return to_native_int<unsigned char>();
    }
    explicit constexpr operator short() const noexcept(
        size() <= std::numeric_limits<short>::digits
    ) {
        return to_native_int<short>();
    }
    explicit constexpr operator unsigned short() const noexcept(
        size() <= std::numeric_limits<unsigned short>::digits
    ) {
        return to_native_int<unsigned short>();
    }
    explicit constexpr operator int() const noexcept(
        size() <= std::numeric_limits<int>::digits
    ) {
        return to_native_int<int>();
    }
    explicit constexpr operator unsigned int() const noexcept(
        size() <= std::numeric_limits<unsigned int>::digits
    ) {
        return to_native_int<unsigned int>();
    }
    explicit constexpr operator long() const noexcept(
        size() <= std::numeric_limits<long>::digits
    ) {
        return to_native_int<long>();
    }
    explicit constexpr operator unsigned long() const noexcept(
        size() <= std::numeric_limits<unsigned long>::digits
    ) {
        return to_native_int<unsigned long>();
    }
    explicit constexpr operator long long() const noexcept(
        size() <= std::numeric_limits<long long>::digits
    ) {
        return to_native_int<long long>();
    }
    explicit constexpr operator unsigned long long() const noexcept(
        size() <= std::numeric_limits<unsigned long long>::digits
    ) {
        return to_native_int<unsigned long long>();
    }

#if defined(__SIZEOF_INT128__)
    explicit constexpr operator __int128_t() const noexcept(
        size() <= (__SIZEOF_INT128__ * 8) - 1
    ) {
        return to_native_int<__int128_t>();
    }
    explicit constexpr operator __uint128_t() const noexcept(
        size() <= (__SIZEOF_INT128__ * 8)
    ) {
        return to_native_int<__uint128_t>();
    }
#endif

    constexpr auto operator+() const noexcept {
        return coconext::types::Signed<size() + 1>(*this);
    }

    constexpr auto operator-() const noexcept {
        Unsigned<R> widened = resize<size() + 1>(*this);
        coconext::types::Signed<size() + 1> result(~widened + 1);
        return result;
    }

    friend std::string to_string(Unsigned const& u) {
        std::string result;
        constexpr size_t W = size();
        result.reserve(W);

        detail::Bits<W> temp_bit(1);
        size_t bit_idx = W - 1;

        for (size_t i = 0; i < W; ++i) {
            if constexpr (!detail::Bits<W>::is_not_native_int) {
                // Extract the specific bit, check if it's non-zero
                bool is_set = ((u.value_.srl(bit_idx)) & temp_bit).raw() != 0;
                result.push_back(is_set ? '1' : '0');
            } else {
                // BigInt fallback
                bool is_set = ((u.value_.srl(bit_idx)) & temp_bit).raw().get_word(0) != 0;
                result.push_back(is_set ? '1' : '0');
            }

            if (bit_idx > 0) {
                bit_idx--;
            }
        }

        return result;
    }

    // TODO indexing, slicing, etc ....

  private:
    Bits<size()> value_;
};

}  // namespace detail

// User-facing alias: accepts the same NTTP forms as Array<T, ...>, with HDL
// DOWNTO defaulting (see detail::make_int_range for the rules).
template <auto... Args>
using Unsigned = detail::Unsigned<detail::make_int_range<Args...>()>;

template <typename Target, typename Source>
    requires detail::is_unsigned_type<Target>::value
constexpr Target as(Source const& source) noexcept {
    static_assert(
        Target::static_range.length() == Source::static_range.length(),
        "as() requires equal widths. Use resize() for width changes."
    );

    return Target(static_cast<detail::Array<Bit, Source::static_range>>(source));
}

template <auto... Args, typename X>
    requires(sizeof...(Args) > 0 && detail::is_unsigned_type<std::remove_cvref_t<X>>::value)
constexpr auto resize(
    X&& x, overflow_mode ovf = overflow_mode::wrap, round_mode rnd = round_mode::truncate
) {
    constexpr Range TargetRange = detail::make_int_range<Args...>();
    return Unsigned<TargetRange>(resize(std::forward<X>(x), ovf, rnd));
}

consteval Unsigned<8> u8(unsigned long long v) { return Unsigned<8>(v); }

consteval Unsigned<16> u16(unsigned long long v) { return Unsigned<16>(v); }

consteval Unsigned<32> u32(unsigned long long v) { return Unsigned<32>(v); }

consteval Unsigned<64> u64(unsigned long long v) { return Unsigned<64>(v); }

}  // namespace coconext::types

// TODO
// as<U>
// resize
// abs, concat, to_string()

// TODO
// template <coconext::types::Range R>
// struct std::formatter<coconext::types::detail::Unsigned<R>> {
//     constexpr auto parse(std::format_parse_context& ctx) {
//         auto it = ctx.begin();
//         if (it != ctx.end() && *it != '}') {
//             throw std::format_error("Unsigned formatter takes no format spec");
//         }
//         return it;
//     }
//     auto format(
//         coconext::types::detail::Unsigned<R> const& v, std::format_context& ctx
//     ) const {
//         return std::format_to(ctx.out(), "{}", v.value());
//     }
// };

// TODO
// template <coconext::types::Range R>
// struct std::hash<coconext::types::detail::Unsigned<R>> {
//     size_t operator()(coconext::types::detail::Unsigned<R> const& v) const noexcept {
//         return std::hash<uint64_t>{}(v.value());
//     }
// };

#endif  // COCONEXT_UNSIGNED_HPP

#ifndef COCONEXT_UFIXED_HPP
#define COCONEXT_UFIXED_HPP

#include <cmath>
#include <coconext/types/bits.hpp>
#include <coconext/types/concepts.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/resize_mode.hpp>

namespace coconext::types {

namespace detail {

template <Range R>
class Ufixed {
    static_assert(R.length() >= 0, "Width must not be negative");

    template <typename T>
    constexpr T to_native_int() const {
        static_assert(
            R.length() > 0, "Ufixed<0> has no integer value, cannot convert to native int"
        );

        auto val = value_.srl(frac_bits());
        if constexpr (int_bits() > std::numeric_limits<T>::digits) {
            if (val.ugt(Bits<R.length()>(std::numeric_limits<T>::max()))) {
                throw std::out_of_range("Value too large for destination native type");
            }
        }

        return static_cast<T>(val.raw());
    }

    template <typename T>
    constexpr T to_native_float() const noexcept {
        static_assert(
            R.length() > 0, "Ufixed<0> has no value, cannot convert to native float"
        );

        if constexpr (!Bits<R.length()>::is_wide) {
            T base_val = static_cast<T>(value_.raw());
            return std::ldexp(base_val, R.right);  // base_val * 2^(R.right)
        } else {
            if (value_ == Bits<R.length()>{0}) {
                return T{0.0};
            }

            int msb_index = value_.highest_set_index();
            constexpr int mantissa_bits = std::numeric_limits<T>::digits;

            int shift_amount = 0;
            if (msb_index >= mantissa_bits) {
                shift_amount = msb_index - mantissa_bits + 1;
            }

            auto aligned = value_.srl(shift_amount);
            uint64_t raw_mantissa = static_cast<uint64_t>(aligned.raw());

            if (shift_amount > 0) {
                bool round_bit = value_.get_bit(shift_amount - 1);
                bool sticky_bit = false;

                for (int i = 0; i < shift_amount - 1; ++i) {
                    if (value_.get_bit(i)) {
                        sticky_bit = true;
                        break;
                    }
                }

                if (round_bit && (sticky_bit || (raw_mantissa & 1))) {
                    raw_mantissa += 1;
                }
            }

            T base_val = static_cast<T>(raw_mantissa);
            return std::ldexp(base_val, R.right + shift_amount);
        }
    }

  public:
    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return R; }
    static constexpr size_t size() noexcept { return R.length(); }

    // Construct from a native integer
    template <NativeInteger T>
    explicit(
        std::is_signed_v<T> || (std::numeric_limits<T>::digits > (R.left + 1))
    ) constexpr Ufixed(T v) {
        if constexpr (std::is_signed_v<T>) {
            if (v < 0) {
                throw std::out_of_range("negative value in Unsigned construction");
            }
        }

        if constexpr (
            std::numeric_limits<T>::digits <= (R.left + 1)
            && R.direction == Direction::DOWNTO
        )
        {
            value_ = v;
        } else {
            using unsigned_T = std::make_unsigned_t<T>;
            if (static_cast<unsigned_T>(v) > max_unsigned<int_bits()>()) {
                throw std::out_of_range("value does not fit in Unsigned width");
            }
            value_ = v;
        }
        value_ = value_ << frac_bits();
    }

    // Construction from Same kind
    template <Range R2>
    explicit(!(R.left > R2.left && R.right < R2.right)) constexpr Ufixed(
        Ufixed<R2> const& other,
        overflow_mode om = overflow_mode::saturate,
        round_mode rm = round_mode::round_to_even
    ) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Ufixed same-kind construction requires DOWNTO direction"
        );
        if constexpr (R.left >= R2.left && R.right <= R2.right) {
            if constexpr (R.length() == R2.length()) {
                value_ = other.value_;
            } else {
                int frac_shift = R2.right - R.right;
                Bits<R.length()> padded_bits = other.value_;
                value_ = padded_bits << frac_shift;
            }
        } else {
            auto aligned = other.value_;
            bool round_up = false;

            if constexpr (R.right > R2.right) {
                int drop_count = R.right - R2.right;

                bool half_bit = aligned.get_bit(drop_count - 1);
                bool lower_bits = false;
                for (int i = 0; i < drop_count - 1; ++i) {
                    if (aligned.get_bit(i)) {
                        lower_bits = true;
                    }
                }

                if (rm == round_mode::round_to_pos) {
                    round_up = half_bit || lower_bits;
                } else if (rm == round_mode::round) {
                    round_up = half_bit;
                } else if (rm == round_mode::round_to_even) {
                    bool keep_bit = aligned.get_bit(drop_count);
                    round_up = half_bit && (lower_bits || keep_bit);
                }

                aligned = aligned.srl(drop_count);
                if (round_up) {
                    aligned = aligned + 1;
                }

            } else if constexpr (R.right < R2.right) {
                aligned = aligned << (R2.right - R.right);
            }

            bool overflow = false;
            if constexpr (R.left < R2.left) {
                for (int i = R.length(); i < aligned.length(); ++i) {
                    if (aligned.get_bit(i)) {
                        overflow = true;
                        break;
                    }
                }
            }

            if (overflow && om == overflow_mode::saturate) {
                value_ = ~Bits<R.length()>(0);
            } else {
                value_ = Bits<R.length()>(0);
                for (int i = 0; i < R.length(); ++i) {
                    if (i < aligned.length() && aligned.get_bit(i)) {
                        value_.set_bit(i, true);
                    }
                }
            }
        }
    }

    // Construction from Sfixed
    // template <Range R2>
    // explicit constexpr Ufixed(Sfixed v, overflow_mode m = overflow_mode::saturate) {
    //     // TODO
    // }

    // Construct from float
    template <std::floating_point FloatType>
    explicit Ufixed(
        FloatType v,
        overflow_mode om = overflow_mode::saturate,
        round_mode rm = round_mode::round_to_even
    ) {
        if (std::isnan(v)) {
            throw std::domain_error("Cannot convert NaN to fixed-point.");
        }
        if (std::isinf(v)) {
            if (om == overflow_mode::wrap) {
                throw std::domain_error("Cannot wrap Infinity.");
            }
            if (v > 0) {
                value_ = ~Bits<R.length()>(0);
            } else {
                value_ = Bits<R.length()>(0);
            }
            return;
        }

        if (v < 0.0) {
            throw std::out_of_range("Cannot construct Ufixed from negative float type");
        }

        FloatType scale_factor = std::exp2(static_cast<FloatType>(-R.right));
        FloatType scaled_v = v * scale_factor;

        FloatType rounded_v = 0.0;

        switch (rm) {
        case round_mode::truncate:
            rounded_v = std::floor(scaled_v);
            break;
        case round_mode::round_to_zero:
            rounded_v = std::trunc(scaled_v);
            break;
        case round_mode::round_to_pos:
            rounded_v = std::ceil(scaled_v);
            break;
        case round_mode::round:
            rounded_v = std::round(scaled_v);
            break;
        case round_mode::round_to_even:
            rounded_v = std::nearbyint(scaled_v);
            break;
        }

        FloatType max_raw_val = std::exp2(static_cast<FloatType>(R.length())) - 1.0;

        bool overflow_high = (rounded_v > max_raw_val);
        bool overflow_low = (rounded_v < 0.0);  // Ufixed cannot be negative!

        if (overflow_high || overflow_low) {
            if (om == overflow_mode::saturate) {
                if (overflow_high) {
                    value_ = ~Bits<R.length()>(0);  // Max out (All 1s)
                } else if (overflow_low) {
                    value_ = Bits<R.length()>(0);  // Bottom out at 0 (All 0s)
                }
            } else if (om == overflow_mode::wrap) {
                value_ = static_cast<Bits<R.length()>>(static_cast<long long>(rounded_v));
            }
        } else {
            value_ = static_cast<Bits<R.length()>>(static_cast<long long>(rounded_v));
        }
    }

    // Construction from Unsigned
    template <Range R2>
    constexpr Ufixed(Unsigned<R2> v)
        requires(R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO)
    {
        static_assert(
            R.right == 0, "Construction from Unsigned requires zero fractional bits"
        );
        static_assert(
            R.length() == R2.length(), "Construction from Unsigned requires equal length"
        );
        value_ = v.value_;
    }

    // Construct from a BitArray
    template <Range R2>
    explicit constexpr Ufixed(detail::Array<Bit, R2> const& other) {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );
        value_ = bits(other);
    }

    // Implicit conversion to supertype BitArray
    template <Range R2>
    constexpr operator detail::Array<Bit, R2>() const noexcept {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );
        return detail::Array<Bit, R2>(value_);
    }

    explicit constexpr operator bool() const noexcept
        requires(R.direction == Direction::DOWNTO)
    {
        return value_ != Bits<R.length()>{};
    }

    explicit constexpr operator signed char() const noexcept(
        R.length() <= std::numeric_limits<signed char>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<signed char>();
    }
    explicit constexpr operator unsigned char() const noexcept(
        R.length() <= std::numeric_limits<unsigned char>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned char>();
    }
    explicit constexpr operator short() const noexcept(
        R.length() <= std::numeric_limits<short>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<short>();
    }
    explicit constexpr operator unsigned short() const noexcept(
        R.length() <= std::numeric_limits<unsigned short>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned short>();
    }
    explicit constexpr operator int() const noexcept(
        R.length() <= std::numeric_limits<int>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<int>();
    }
    explicit constexpr operator unsigned int() const noexcept(
        R.length() <= std::numeric_limits<unsigned int>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned int>();
    }
    explicit constexpr operator long() const noexcept(
        R.length() <= std::numeric_limits<long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<long>();
    }
    explicit constexpr operator unsigned long() const noexcept(
        R.length() <= std::numeric_limits<unsigned long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned long>();
    }
    explicit constexpr operator long long() const noexcept(
        R.length() <= std::numeric_limits<long long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<long long>();
    }
    explicit constexpr operator unsigned long long() const noexcept(
        R.length() <= std::numeric_limits<unsigned long long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned long long>();
    }
#if defined(__SIZEOF_INT128__)
    explicit constexpr operator __int128_t() const noexcept(
        R.length() <= (__SIZEOF_INT128__ * 8) - 1
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<__int128_t>();
    }
    explicit constexpr operator __uint128_t() const noexcept(
        R.length() <= (__SIZEOF_INT128__ * 8)
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<__uint128_t>();
    }
#endif
    explicit constexpr operator float() const noexcept
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_float<float>();
    }
    explicit constexpr operator double() const noexcept
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_float<double>();
    }
    explicit constexpr operator long double() const noexcept
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_float<long double>();
    }

    template <Range R2>
    constexpr std::strong_ordering operator<=>(Ufixed<R2> const& other) const noexcept {
        static_assert(
            R.direction == Direction::DOWNTO, "Comparison requires downto Direction"
        );
        static_assert(R == R2, "Comparison requires equal Ranges");

        if (value_ == other.value_) {
            return std::strong_ordering::equal;
        }

        if (value_.ugt(other.value_)) {
            return std::strong_ordering::greater;
        }

        return std::strong_ordering::less;
    }

    static constexpr size_t frac_bits() {
        if constexpr (R.direction == Direction::DOWNTO) {
            return R.length() - R.left - 1;
        } else {
            return -R.left;
        }
    }

    static constexpr size_t int_bits() { return R.length() - frac_bits(); }

    static constexpr double resolution() {
        long long exp = (R.direction == Direction::DOWNTO) ? R.right : R.left;
        double result = 1.0;

        if (exp > 0) {
            for (long long i = 0; i < exp; ++i) {
                result *= 2.0;
            }
        } else if (exp < 0) {
            for (long long i = 0; i > exp; --i) {
                result /= 2.0;
            }
        }

        return result;
    }

    // returns bit at index in storage
    constexpr auto at_ordinal(size_t index) const {
        if (R.length() >= index) {
            throw std::out_of_range("index out of bounds");
        }
        return value_.get_bit(R.length() - index);
    }

    constexpr auto begin() const { return value_.begin(); }
    constexpr auto rbegin() const { return value_.rbegin(); }

    constexpr auto end() const { return value_.end(); }
    constexpr auto rend() const { return value_.rend(); }

    constexpr auto operator[](Range::value_type idx) const {
        if constexpr (R.direction == Direction::DOWNTO) {
            if (idx > R.left || idx < R.right) {
                throw std::out_of_range("index out of bounds");
            }
            return value_.get_bit(idx - R.right);
        } else {
            if (idx < R.left || idx > R.right) {
                throw std::out_of_range("index out of bounds");
            }
            return value_.get_bit(-(idx - R.right));
        }
    }

  private:
    Bits<R.length()> value_{0};
};

}  // namespace detail

template <Range R>
inline constexpr bool is_fixed<detail::Ufixed<R>> = true;

// see detail::make_fixed_range for the rules
template <auto... Args>
using Ufixed = detail::Ufixed<detail::make_fixed_range<Args...>()>;

template <Range R>
constexpr auto reverse(detail::Ufixed<R> v) {
    if constexpr (R.direction == Direction::DOWNTO) {
        return detail::Ufixed<Range{R.left, Direction::TO, R.right}>(v);
    } else {
        return detail::Ufixed<Range{R.left, Direction::DOWNTO, R.right}>(
            v.value_.reverse()
        );
    }
}

}  // namespace coconext::types

// TODO hash

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Ufixed<R>> {
    char presentation = 'd';

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') {
            presentation = *it++;
            if (presentation != 'd' && presentation != 'b') {
                throw std::format_error("Invalid format specifier for Ufixed");
            }
        }
        if (it != end && *it != '}') {
            throw std::format_error("Invalid format string");
        }
        return it;
    }

    auto format(
        coconext::types::detail::Ufixed<R> const& v, std::format_context& ctx
    ) const {
        std::string str_r;
        switch (presentation) {
        case 'b':
            str_r = v.value_.to_binary_string()(v.frac_bits());
            break;
        default:
            str_r = v.value_.to_decimal_string();
        }
        return std::format_to(ctx.out(), "Ufixed{}{{{}}}", R, str_r);
    }
};

template <coconext::types::Range R>
struct std::hash<coconext::types::detail::Ufixed<R>> {
    size_t operator()(coconext::types::detail::Ufixed<R> const& v) const noexcept {
        std::string_view type_name = typeid(coconext::types::detail::Ufixed<R>).name();
        size_t ufixed_seed = std::hash<std::string_view>{}(type_name);
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

        return coconext::types::detail::hash_combine(ufixed_seed, R, value_hash);
    }
};

#endif  // COCONEXT_UFIXED_HPP

// TODO

// floor(x), ceil(x), round(x), trunc(x) free functions
// reverse
// as
// resize

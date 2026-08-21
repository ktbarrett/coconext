#ifndef COCONEXT_SFIXED_HPP
#define COCONEXT_SFIXED_HPP

#include <coconext/types/signed.hpp>
#include <coconext/types/ufixed.hpp>

namespace coconext::types {

namespace detail {

template <Range R>
class Sfixed;

template <typename T>
inline constexpr bool is_coconext_sfixed_v = false;

template <Range R>
inline constexpr bool is_coconext_sfixed_v<detail::Sfixed<R>> = true;

}  // namespace detail

template <auto... Args, typename X>
    requires(
        sizeof...(Args) > 0 && sizeof...(Args) <= 3
        && detail::is_coconext_sfixed_v<std::remove_cvref_t<X>>
    )
constexpr auto resize(
    X&& x,
    overflow_mode ovf = overflow_mode::saturate,
    round_mode rnd = round_mode::round_to_even
);

namespace detail {

template <Range R>
class Sfixed {
    static_assert(R.length() > 0, "Width must positive");

    template <typename T>
    constexpr T to_native_int() const {
        static_assert(
            R.length() > 0, "Sfixed<0> has no integer value, cannot convert to native int"
        );

        if constexpr (!Bits<R.length()>::is_wide) {
            int64_t signed_raw = static_cast<int64_t>(value_.raw());
            constexpr size_t W = R.length();
            if constexpr (W < 64) {
                int64_t const m = 1ULL << (W - 1);
                signed_raw = (signed_raw ^ m) - m;
            }

            int64_t int_val = 0;
            if constexpr (frac_bits() < 64) {
                int_val = signed_raw / static_cast<int64_t>(1ULL << frac_bits());
            }

            bool out_of_bounds = false;
            if constexpr (std::is_signed_v<T>) {
                if (int_val < std::numeric_limits<T>::min()
                    || int_val > std::numeric_limits<T>::max())
                {
                    out_of_bounds = true;
                }
            } else {
                if (int_val < 0
                    || static_cast<uint64_t>(int_val) > std::numeric_limits<T>::max())
                {
                    out_of_bounds = true;
                }
            }

            if (out_of_bounds) {
                throw std::out_of_range("Value too large for destination native type");
            }

            return static_cast<T>(int_val);

        } else {
            bool is_negative = value_.get_bit(R.length() - 1);
            Bits<R.length()> abs_value =
                is_negative ? (~value_) + Bits<R.length()>(1) : value_;

            // Truncate fractional bits (logical shift on absolute value == round_to_zero)
            Bits<R.length()> int_magnitude = abs_value.srl(frac_bits());

            int msb_index = int_magnitude.highest_set_index();
            bool out_of_bounds = false;

            if (msb_index >= 0) {
                int max_bits = std::numeric_limits<T>::digits;

                if (msb_index > max_bits) {
                    out_of_bounds = true;
                } else if (msb_index == max_bits) {
                    if (is_negative && std::is_signed_v<T>) {
                        Bits<R.length()> remainder = int_magnitude;
                        remainder.set_bit(max_bits, false);
                        if (remainder != Bits<R.length()>{0}) {
                            out_of_bounds = true;
                        }
                    } else {
                        out_of_bounds = true;
                    }
                }

                if (is_negative && !std::is_signed_v<T>) {
                    out_of_bounds = true;
                }
            }

            if (out_of_bounds) {
                throw std::out_of_range("Value too large for destination native type");
            }

            if (is_negative
                && std::is_signed_v<T> && msb_index == std::numeric_limits<T>::digits)
            {
                return std::numeric_limits<T>::min();
            }

            auto raw_struct = int_magnitude.raw();
            T native_mag = static_cast<T>(raw_struct.word(0));

            if constexpr (sizeof(T) > 8) {
                native_mag |= (static_cast<T>(raw_struct.word(1)) << 64);
            }

            return is_negative ? -native_mag : native_mag;
        }
    }

    template <typename T>
    constexpr T to_native_float() const noexcept {
        static_assert(
            R.length() > 0, "Sfixed<0> has no value, cannot convert to native float"
        );

        if constexpr (!Bits<R.length()>::is_wide) {
            int64_t signed_raw = static_cast<int64_t>(value_.raw());
            constexpr size_t W = R.length();
            if constexpr (W < 64) {
                int64_t const m = 1ULL << (W - 1);
                signed_raw = (signed_raw ^ m) - m;
            }
            return std::ldexp(static_cast<T>(signed_raw), R.right);
        } else {
            if (value_ == Bits<R.length()>{0}) {
                return T{0.0};
            }

            bool is_negative = value_.get_bit(R.length() - 1);
            Bits<R.length()> abs_value =
                is_negative ? (~value_) + Bits<R.length()>(1) : value_;

            int msb_index = abs_value.highest_set_index();
            constexpr int mantissa_bits = std::numeric_limits<T>::digits;

            int shift_amount = 0;
            if (msb_index >= mantissa_bits) {
                shift_amount = msb_index - mantissa_bits + 1;
            }

            auto aligned = abs_value.srl(shift_amount);
            uint64_t raw_mantissa = static_cast<uint64_t>(aligned.raw());

            if (shift_amount > 0) {
                bool round_bit = abs_value.get_bit(shift_amount - 1);
                bool sticky_bit = false;

                for (int i = 0; i < shift_amount - 1; ++i) {
                    if (abs_value.get_bit(i)) {
                        sticky_bit = true;
                        break;
                    }
                }

                if (round_bit && (sticky_bit || (raw_mantissa & 1))) {
                    raw_mantissa += 1;
                }
            }

            T base_val = static_cast<T>(raw_mantissa);
            T final_val = std::ldexp(base_val, R.right + shift_amount);
            return is_negative ? -final_val : final_val;
        }
    }

  public:
    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return R; }
    static constexpr size_t size() noexcept { return R.length(); }

    constexpr Sfixed() noexcept = default;

    template <size_t W>
    constexpr Sfixed(Bits<W> const& val) {
        static_assert(W == R.length(), "Construction from Bits requires identical width");
        value_ = val;
    }

    // Construct from a native integer
    template <NativeInteger T>
    explicit(std::numeric_limits<T>::digits > R.left) constexpr Sfixed(T v) {
        static_assert(
            R.direction == Direction::DOWNTO,
            "Construction from int not allowed on a TO Direction Sfixed"
        );
        if constexpr (
            std::numeric_limits<T>::digits <= R.left && R.direction == Direction::DOWNTO
        )
        {
            value_ = v;
        } else {
            if constexpr (std::is_signed_v<T>) {
                long long min_val = -(1LL << R.left);
                long long max_val = (1LL << R.left) - 1;
                if (v < min_val || v > max_val) {
                    throw std::out_of_range("value does not fit in Sfixed width");
                }
            } else {
                unsigned long long max_val = (1ULL << R.left) - 1;
                if (static_cast<unsigned long long>(v) > max_val) {
                    throw std::out_of_range("value does not fit in Sfixed width");
                }
            }
            value_ = static_cast<long long>(v);
        }
        value_ = value_ << frac_bits();
    }

    // Construction from Same kind
    template <Range R2>
    explicit(!(R.left >= R2.left && R.right <= R2.right)) constexpr Sfixed(
        Sfixed<R2> const& other,
        overflow_mode om = overflow_mode::saturate,
        round_mode rm = round_mode::round_to_even
    ) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Sfixed same-kind construction requires DOWNTO direction"
        );
        if constexpr (R.left >= R2.left && R.right <= R2.right) {
            if constexpr (R.length() == R2.length()) {
                value_ = other.value_;
            } else {
                int frac_shift = R2.right - R.right;
                Bits<R.length()> padded_bits =
                    bits(other).template sign_extend<R.length()>();
                value_ = padded_bits << frac_shift;
            }
        } else {
            value_ = coconext::types::resize<R>(other, om, rm).value_;
        }
    }

    // Construction from Ufixed
    template <Range R2>
    explicit(!(R.left > R2.left && R.right <= R2.right)) constexpr Sfixed(
        Ufixed<R2> const& other,
        overflow_mode om = overflow_mode::saturate,
        round_mode rm = round_mode::round_to_even
    ) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Sfixed cross-kind construction requires DOWNTO direction"
        );

        if constexpr (R.left > R2.left && R.right <= R2.right) {
            int frac_shift = R2.right - R.right;
            Bits<R.length()> padded_bits = bits(other).template zero_extend<R.length()>();
            value_ = padded_bits << frac_shift;
        } else {
            value_ = coconext::types::resize<R>(other, om, rm).value_;
        }
    }

    // Construct from float
    template <std::floating_point FloatType>
    explicit Sfixed(
        FloatType v,
        overflow_mode om = overflow_mode::saturate,
        round_mode rm = round_mode::round_to_even
    ) {
        static_assert(
            R.direction == Direction::DOWNTO,
            "Construction from a float/double not allowed on a TO Direction Sfixed"
        );

        if (std::isnan(v)) {
            throw std::domain_error("Cannot convert NaN to fixed-point.");
        }

        constexpr size_t W = R.length();
        Bits<W> const max_signed_bits = ~(Bits<W>(1) << (W - 1));
        Bits<W> const min_signed_bits = Bits<W>(1) << (W - 1);

        if (std::isinf(v)) {
            if (om == overflow_mode::wrap) {
                throw std::domain_error("Cannot wrap Infinity.");
            }
            if (v > 0) {
                value_ = max_signed_bits;
            } else {
                value_ = min_signed_bits;
            }
            return;
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

        FloatType max_raw_val = std::exp2(static_cast<FloatType>(W - 1)) - 1.0;
        FloatType min_raw_val = -std::exp2(static_cast<FloatType>(W - 1));

        bool overflow_high = (rounded_v > max_raw_val);
        bool overflow_low = (rounded_v < min_raw_val);

        if (overflow_high || overflow_low) {
            if (om == overflow_mode::saturate) {
                if (overflow_high) {
                    value_ = max_signed_bits;
                } else if (overflow_low) {
                    value_ = min_signed_bits;
                }
            } else if (om == overflow_mode::wrap) {
                value_ = static_cast<Bits<W>>(static_cast<long long>(rounded_v));
            }
        } else {
            value_ = static_cast<Bits<W>>(static_cast<long long>(rounded_v));
        }
    }

    // Construction from Signed
    template <Range R2>
    constexpr Sfixed(Signed<R2> v)
        requires(R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO)
    {
        static_assert(
            R.right == 0, "Construction from Signed requires zero fractional bits"
        );
        static_assert(
            R.length() == R2.length(), "Construction from Signed requires equal length"
        );
        value_ = bits(v);
    }

    // Construction from Unsigned
    template <Range R2>
    explicit(R.length() <= R2.length()) constexpr Sfixed(Unsigned<R2> v)
        requires(R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO)
    {
        static_assert(
            R.right == 0, "Construction from Unsigned requires zero fractional bits"
        );
        if constexpr (R.length() > R2.length()) {
            value_ = bits(v).template zero_extend<R.length()>();
        } else {
            value_ = bits(v).template truncate<R.length()>();
        }
    }

    // Construct from a BitArray
    template <Range R2>
    explicit constexpr Sfixed(detail::Array<Bit, R2> const& other) {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );
        value_ = bits(other);
    }

    template <typename SourceWrapper>
    constexpr Sfixed(detail::auto_resized<SourceWrapper>&& wrapper) {
        auto [src, ovf, rnd] = std::move(wrapper).consume();
        using ActualSource = std::remove_cvref_t<decltype(src)>;

        static_assert(
            detail::is_coconext_sfixed_v<ActualSource>,
            "resize() target and source must both be Sfixed. Use as() for cross-type "
            "conversions."
        );

        constexpr Range R2 = ActualSource::range();
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "resize requires DOWNTO direction for both source and destination."
        );

        constexpr size_t TargetW = R.length();
        constexpr size_t SourceW = R2.length();
        constexpr int frac_diff = R2.right - R.right;

        constexpr size_t ShiftAmount = (frac_diff > 0) ? frac_diff : 0;
        constexpr size_t InterW =
            (TargetW > SourceW + ShiftAmount + 1) ? TargetW : (SourceW + ShiftAmount + 1);

        auto inter_val = bits(src).template sign_extend<InterW>();

        if constexpr (frac_diff > 0) {
            inter_val = inter_val << frac_diff;
        } else if constexpr (frac_diff < 0) {
            constexpr int drop_count = -frac_diff;

            bool half_bit = inter_val.get_bit(drop_count - 1);
            bool lower_bits = false;
            for (int i = 0; i < drop_count - 1; ++i) {
                if (inter_val.get_bit(i)) {
                    lower_bits = true;
                    break;
                }
            }

            bool is_neg = inter_val.get_bit(InterW - 1);
            bool round_up = false;

            switch (rnd) {
            case round_mode::truncate:
                round_up = false;
                break;
            case round_mode::round_to_zero:
                round_up = is_neg && (half_bit || lower_bits);
                break;
            case round_mode::round_to_pos:
                round_up = (half_bit || lower_bits);
                break;
            case round_mode::round:
                round_up = half_bit && (!is_neg || lower_bits);
                break;
            case round_mode::round_to_even:
                bool keep_bit = inter_val.get_bit(drop_count);
                round_up = half_bit && (lower_bits || keep_bit);
                break;
            }

            inter_val = inter_val.srl(drop_count);

            // Sign extend back the vacated high bits after the logical shift right
            if (is_neg) {
                for (size_t i = InterW - drop_count; i < InterW; ++i) {
                    inter_val.set_bit(i, true);
                }
            }

            if (round_up) {
                inter_val = inter_val + 1;
            }
        }

        bool overflow = false;
        bool is_neg_res = inter_val.get_bit(InterW - 1);
        if (is_neg_res) {
            for (size_t i = TargetW - 1; i < InterW; ++i) {
                if (!inter_val.get_bit(i)) {
                    overflow = true;
                    break;
                }
            }
        } else {
            for (size_t i = TargetW - 1; i < InterW; ++i) {
                if (inter_val.get_bit(i)) {
                    overflow = true;
                    break;
                }
            }
        }

        if (overflow && ovf == overflow_mode::saturate) {
            if (is_neg_res) {
                value_ = Bits<TargetW>(1) << (TargetW - 1);
            } else {
                value_ = ~(Bits<TargetW>(1) << (TargetW - 1));
            }
        } else {
            value_ = inter_val.template truncate<TargetW>();
        }
    }

    template <typename SourceWrapper>
    constexpr Sfixed& operator=(detail::auto_resized<SourceWrapper>&& wrapper) {
        *this = Sfixed(std::move(wrapper));
        return *this;
    }

    template <typename SourceT>
    constexpr Sfixed(auto_reinterpreted<SourceT>&& wrapper) {
        *this = coconext::types::as<Sfixed<R>>(std::move(wrapper).consume());
    }

    template <typename SourceT>
    constexpr Sfixed& operator=(auto_reinterpreted<SourceT>&& wrapper) {
        *this = coconext::types::as<Sfixed<R>>(std::move(wrapper).consume());
        return *this;
    }

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
        R.length() <= std::numeric_limits<signed char>::digits + 1
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
        R.length() <= std::numeric_limits<short>::digits + 1
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
        R.length() <= std::numeric_limits<int>::digits + 1
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
        R.length() <= std::numeric_limits<long>::digits + 1
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
        R.length() <= std::numeric_limits<long long>::digits + 1
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
        R.length() <= (__SIZEOF_INT128__ * 8)
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

    template <typename T>
    constexpr Sfixed operator<<(T amount) const {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(
            std::is_integral_v<T> || is_coconext_unsigned_v<T> || is_coconext_signed_v<T>,
            "Shift Amount can only be purely Integral"
        );

        if constexpr (std::is_signed_v<T> || is_coconext_signed_v<T>) {
            if (static_cast<int64_t>(amount) < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        uint64_t v = static_cast<unsigned long long>(amount);
        if (v < 0) {
            throw std::invalid_argument("Shift amount cannot be negative");
        }

        return Sfixed<R>(value_ << v);
    }

    template <typename T>
    constexpr Sfixed operator<<=(T amount) {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(
            std::is_integral_v<T> || is_coconext_unsigned_v<T> || is_coconext_signed_v<T>,
            "Shift Amount can only be purely Integral"
        );

        if constexpr (std::is_signed_v<T> || is_coconext_signed_v<T>) {
            if (static_cast<int64_t>(amount) < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        uint64_t v = static_cast<unsigned long long>(amount);
        if (v < 0) {
            throw std::invalid_argument("Shift amount cannot be negative");
        }

        value_ = value_ << v;
        return *this;
    }

    template <typename T>
    constexpr Sfixed operator>>(T amount) const {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(
            std::is_integral_v<T> || is_coconext_unsigned_v<T> || is_coconext_signed_v<T>,
            "Shift Amount can only be purely Integral"
        );

        if constexpr (std::is_signed_v<T> || is_coconext_signed_v<T>) {
            if (static_cast<int64_t>(amount) < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        uint64_t v = static_cast<unsigned long long>(amount);
        if (v < 0) {
            throw std::invalid_argument("Shift amount cannot be negative");
        }

        return Sfixed<R>(value_.sra(v));
    }

    template <typename T>
    constexpr Sfixed operator>>=(T amount) {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(
            std::is_integral_v<T> || is_coconext_unsigned_v<T> || is_coconext_signed_v<T>,
            "Shift Amount can only be purely Integral"
        );

        if constexpr (std::is_signed_v<T> || is_coconext_signed_v<T>) {
            if (static_cast<int64_t>(amount) < 0) {
                throw std::invalid_argument("Negative shift amount");
            }
        }

        uint64_t v = static_cast<unsigned long long>(amount);
        if (v < 0) {
            throw std::invalid_argument("Shift amount cannot be negative");
        }

        value_ = value_.sra(v);
        return *this;
    }

    template <Range R2>
    constexpr std::strong_ordering operator<=>(Sfixed<R2> const& other) const noexcept {
        static_assert(R == R2, "Comparison requires equal Ranges");

        if (value_ == other.value_) {
            return std::strong_ordering::equal;
        }

        if (value_.sgt(other.value_)) {
            return std::strong_ordering::greater;
        }

        return std::strong_ordering::less;
    }

    template <Range R2>
    constexpr bool operator==(Sfixed<R2> const& other) const noexcept {
        static_assert(R == R2, "Comparison requires equal Ranges");

        if (value_ == other.value_) {
            return true;
        }

        return false;
    }

    constexpr auto operator+() const {
        static_assert(
            R.direction == Direction::DOWNTO,
            "All arithmetic operations require downto Direction"
        );
        return *this;
    }

    constexpr auto operator-() const {
        static_assert(
            R.direction == Direction::DOWNTO,
            "All arithmetic operations require downto Direction"
        );

        constexpr auto TR = Range{R.left + 1, R.direction, R.right};
        constexpr size_t TargetW = TR.length();

        auto extended_bits = value_.template sign_extend<TargetW>();
        return Sfixed<TR>(Bits<TargetW>(0) - extended_bits);
    }

    // TODO
    // template <Range R2>
    // constexpr auto operator+(Sfixed<R2> const& rhs) const {
    //     static_assert(R.direction == Direction::DOWNTO && R2.direction ==
    //     Direction::DOWNTO, "Operations require DOWNTO"); constexpr Range
    //     R_res{std::max(R.left, R2.left) + 1, Direction::DOWNTO, std::min(R.right,
    //     R2.right)}; constexpr size_t ShiftL = R.right - R_res.right; constexpr size_t
    //     ShiftR = R2.right - R_res.right;

    //     auto lhs_aligned = value_.template sign_extend<R.length() + ShiftL>() << ShiftL;
    //     auto rhs_aligned = bits(rhs).template sign_extend<R2.length() + ShiftR>() <<
    //     ShiftR;

    //     return Sfixed<R_res>(detail::add_signed(lhs_aligned, rhs_aligned));
    // }

    // template <Range R2>
    // constexpr auto operator-(Sfixed<R2> const& rhs) const {
    //     static_assert(R.direction == Direction::DOWNTO && R2.direction ==
    //     Direction::DOWNTO, "Operations require DOWNTO"); constexpr Range
    //     R_res{std::max(R.left, R2.left) + 1, Direction::DOWNTO, std::min(R.right,
    //     R2.right)}; constexpr size_t ShiftL = R.right - R_res.right; constexpr size_t
    //     ShiftR = R2.right - R_res.right;

    //     auto lhs_aligned = value_.template sign_extend<R.length() + ShiftL>() << ShiftL;
    //     auto rhs_aligned = bits(rhs).template sign_extend<R2.length() + ShiftR>() <<
    //     ShiftR;

    //     return Sfixed<R_res>(detail::sub_signed(lhs_aligned, rhs_aligned));
    // }

    // template <Range R2>
    // constexpr auto operator*(Sfixed<R2> const& rhs) const {
    //     static_assert(R.direction == Direction::DOWNTO && R2.direction ==
    //     Direction::DOWNTO, "Operations require DOWNTO"); constexpr Range R_res{R.left +
    //     R2.left + 1, Direction::DOWNTO, R.right + R2.right}; return
    //     Sfixed<R_res>(detail::mul_signed(value_, bits(rhs)));
    // }

    // template <Range R2>
    // constexpr auto operator/(Sfixed<R2> const& rhs) const {
    //     static_assert(R.direction == Direction::DOWNTO && R2.direction ==
    //     Direction::DOWNTO, "Operations require DOWNTO"); if (!static_cast<bool>(rhs)) {
    //         throw std::domain_error("Division by zero");
    //     }
    //     constexpr Range R_res{R.left - R2.right + 1, Direction::DOWNTO, R.right - R2.left
    //     - 1}; constexpr size_t ShiftL = R2.length();

    //     auto lhs_shifted = value_.template sign_extend<R.length() + ShiftL>() << ShiftL;
    //     return Sfixed<R_res>(detail::div_signed(lhs_shifted, bits(rhs)));
    // }

    // template <Range R2>
    // constexpr auto operator%(Sfixed<R2> const& rhs) const {
    //     static_assert(R.direction == Direction::DOWNTO && R2.direction ==
    //     Direction::DOWNTO, "Operations require DOWNTO"); if (!static_cast<bool>(rhs)) {
    //         throw std::domain_error("Division by zero");
    //     }
    //     constexpr size_t min_R = std::min(R.right, R2.right);
    //     constexpr Range R_res{R2.left, Direction::DOWNTO, min_R};
    //     constexpr size_t ShiftL = R.right - min_R;
    //     constexpr size_t ShiftR = R2.right - min_R;

    //     auto lhs_aligned = value_.template sign_extend<R.length() + ShiftL>() << ShiftL;
    //     auto rhs_aligned = bits(rhs).template sign_extend<R2.length() + ShiftR>() <<
    //     ShiftR;

    //     return Sfixed<R_res>(detail::rem_signed(lhs_aligned, rhs_aligned));
    // }

    // template <Range R2>
    // constexpr Sfixed& operator+=(Sfixed<R2> const& rhs) {
    //     *this = coconext::types::resize<R>(*this + rhs);
    //     return *this;
    // }
    // template <Range R2>
    // constexpr Sfixed& operator-=(Sfixed<R2> const& rhs) {
    //     *this = coconext::types::resize<R>(*this - rhs);
    //     return *this;
    // }
    // template <Range R2>
    // constexpr Sfixed& operator*=(Sfixed<R2> const& rhs) {
    //     *this = coconext::types::resize<R>(*this * rhs);
    //     return *this;
    // }
    // template <Range R2>
    // constexpr Sfixed& operator/=(Sfixed<R2> const& rhs) {
    //     *this = coconext::types::resize<R>(*this / rhs);
    //     return *this;
    // }
    // template <Range R2>
    // constexpr Sfixed& operator%=(Sfixed<R2> const& rhs) {
    //     *this = coconext::types::resize<R>(*this % rhs);
    //     return *this;
    // }

    // template <NativeInteger T>
    // constexpr Sfixed& operator+=(T const& rhs) {
    //     *this = coconext::types::resize<R>(*this + Sfixed<Range{R.length() - 1,
    //     Direction::DOWNTO, 0}>(rhs)); return *this;
    // }
    // template <NativeInteger T>
    // constexpr Sfixed& operator-=(T const& rhs) {
    //     *this = coconext::types::resize<R>(*this - Sfixed<Range{R.length() - 1,
    //     Direction::DOWNTO, 0}>(rhs)); return *this;
    // }
    // template <NativeInteger T>
    // constexpr Sfixed& operator*=(T const& rhs) {
    //     *this = coconext::types::resize<R>(*this * Sfixed<Range{R.length() - 1,
    //     Direction::DOWNTO, 0}>(rhs)); return *this;
    // }
    // template <NativeInteger T>
    // constexpr Sfixed& operator/=(T const& rhs) {
    //     *this = coconext::types::resize<R>(*this / Sfixed<Range{R.length() - 1,
    //     Direction::DOWNTO, 0}>(rhs)); return *this;
    // }
    // template <NativeInteger T>
    // constexpr Sfixed& operator%=(T const& rhs) {
    //     *this = coconext::types::resize<R>(*this % Sfixed<Range{R.length() - 1,
    //     Direction::DOWNTO, 0}>(rhs)); return *this;
    // }

    // constexpr Sfixed& operator++() { *this += 1; return *this; }
    // constexpr Sfixed operator++(int) { Sfixed tmp = *this; *this += 1; return tmp; }
    // constexpr Sfixed& operator--() { *this -= 1; return *this; }
    // constexpr Sfixed operator--(int) { Sfixed tmp = *this; *this -= 1; return tmp; }

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
        if (R.length() <= index) {
            throw std::out_of_range("index out of bounds");
        }
        return value_.get_bit(R.length() - 1 - index);
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
    friend struct bits_fn;

    Bits<R.length()> value_{0};
};

// TODO
// template <Range R1, Range R2>
// constexpr auto operator+(Ufixed<R1> const& lhs, Sfixed<R2> const& rhs) { return (+lhs) +
// rhs; } template <Range R1, Range R2> constexpr auto operator-(Ufixed<R1> const& lhs,
// Sfixed<R2> const& rhs) { return (+lhs) - rhs; } template <Range R1, Range R2> constexpr
// auto operator*(Ufixed<R1> const& lhs, Sfixed<R2> const& rhs) { return (+lhs) * rhs; }
// template <Range R1, Range R2>
// constexpr auto operator/(Ufixed<R1> const& lhs, Sfixed<R2> const& rhs) { return (+lhs) /
// rhs; } template <Range R1, Range R2> constexpr auto operator%(Ufixed<R1> const& lhs,
// Sfixed<R2> const& rhs) { return (+lhs) % rhs; }

// template <Range R1, Range R2>
// constexpr auto operator+(Sfixed<R1> const& lhs, Ufixed<R2> const& rhs) { return lhs +
// (+rhs); } template <Range R1, Range R2> constexpr auto operator-(Sfixed<R1> const& lhs,
// Ufixed<R2> const& rhs) { return lhs - (+rhs); } template <Range R1, Range R2> constexpr
// auto operator*(Sfixed<R1> const& lhs, Ufixed<R2> const& rhs) { return lhs * (+rhs); }
// template <Range R1, Range R2>
// constexpr auto operator/(Sfixed<R1> const& lhs, Ufixed<R2> const& rhs) { return lhs /
// (+rhs); } template <Range R1, Range R2> constexpr auto operator%(Sfixed<R1> const& lhs,
// Ufixed<R2> const& rhs) { return lhs % (+rhs); }

}  // namespace detail

template <Range R>
inline constexpr bool is_fixed<detail::Sfixed<R>> = true;

template <auto... Args>
using Sfixed = detail::Sfixed<detail::make_fixed_range<Args...>()>;

template <auto... Args, typename X>
    requires(
        sizeof...(Args) > 0 && sizeof...(Args) <= 3
        && detail::is_coconext_sfixed_v<std::remove_cvref_t<X>>
    )
constexpr auto resize(X&& x, overflow_mode ovf, round_mode rnd) {
    constexpr Range TargetRange = detail::make_fixed_range<Args...>();
    return Sfixed<TargetRange>(detail::resize(std::forward<X>(x), ovf, rnd));
}

template <typename X>
    requires detail::is_coconext_sfixed_v<std::remove_cvref_t<X>>
constexpr auto floor(X&& x) {
    constexpr Range R = std::remove_cvref_t<X>::range();
    static_assert(
        R.direction == Direction::DOWNTO, "resizing to integer requires downto direction"
    );
    constexpr Range TargetRange = Range{R.left, Direction::DOWNTO, 0};

    return detail::Sfixed<TargetRange>(
        detail::resize(std::forward<X>(x), overflow_mode::saturate, round_mode::truncate)
    );
}

template <typename X>
    requires detail::is_coconext_sfixed_v<std::remove_cvref_t<X>>
constexpr auto ceil(X&& x) {
    constexpr Range R = std::remove_cvref_t<X>::range();
    static_assert(
        R.direction == Direction::DOWNTO, "rounding to integer requires downto direction"
    );
    constexpr Range TargetRange = Range{R.left, Direction::DOWNTO, 0};

    return detail::Sfixed<TargetRange>(detail::resize(
        std::forward<X>(x), overflow_mode::saturate, round_mode::round_to_pos
    ));
}

template <typename X>
    requires detail::is_coconext_sfixed_v<std::remove_cvref_t<X>>
constexpr auto trunc(X&& x) {
    constexpr Range R = std::remove_cvref_t<X>::range();
    static_assert(
        R.direction == Direction::DOWNTO, "rounding to integer requires downto direction"
    );
    constexpr Range TargetRange = Range{R.left, Direction::DOWNTO, 0};

    return detail::Sfixed<TargetRange>(detail::resize(
        std::forward<X>(x), overflow_mode::saturate, round_mode::round_to_zero
    ));
}

template <typename X>
    requires detail::is_coconext_sfixed_v<std::remove_cvref_t<X>>
constexpr auto round(X&& x) {
    constexpr Range R = std::remove_cvref_t<X>::range();
    static_assert(
        R.direction == Direction::DOWNTO, "rounding to integer requires downto direction"
    );
    constexpr Range TargetRange = Range{R.left, Direction::DOWNTO, 0};

    return detail::Sfixed<TargetRange>(
        detail::resize(std::forward<X>(x), overflow_mode::saturate, round_mode::round)
    );
}

template <Range R>
constexpr auto reverse(detail::Sfixed<R> const& v) noexcept {
    if constexpr (R.direction == Direction::TO) {
        return detail::Sfixed<reverse(R)>(detail::bits(v).reverse());
    } else {
        return detail::Sfixed<reverse(R)>(detail::bits(v));
    }
}

// abs(s) free function for Sfixed<L, R> -> Sfixed<L+1, R>
template <Range R>
constexpr auto abs(detail::Sfixed<R> const& v) noexcept {
    constexpr auto TargetRange = Range{R.left + 1, R.direction, R.right};

    if (v < detail::Sfixed<R>{0}) {
        return -v;
    }
    return detail::Sfixed<TargetRange>(
        detail::resize(v, overflow_mode::saturate, round_mode::round)
    );
}

}  // namespace coconext::types

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Sfixed<R>> {
    char presentation = 'd';

    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && *it != '}') {
            presentation = *it++;
            if (presentation != 'd' && presentation != 'b') {
                throw std::format_error("Invalid format specifier for Sfixed");
            }
        }
        if (it != end && *it != '}') {
            throw std::format_error("Invalid format string");
        }

        if (presentation == 'd' && R.direction != coconext::types::Direction::DOWNTO) {
            throw std::format_error("Decimal format requires downto Direction");
        }

        return it;
    }

    auto format(
        coconext::types::detail::Sfixed<R> const& v, std::format_context& ctx
    ) const {
        std::string str_r;
        switch (presentation) {
        case 'b': {
            constexpr size_t F = coconext::types::detail::Sfixed<R>::frac_bits();
            constexpr size_t I = coconext::types::detail::Sfixed<R>::int_bits();
            constexpr auto decimal_pos = (F && I) ? F : 0;
            str_r = coconext::types::detail::bits(v).to_binary_string(decimal_pos);
            break;
        }
        default: {
            constexpr size_t F = coconext::types::detail::Sfixed<R>::frac_bits();
            str_r =
                coconext::types::detail::bits(v).template to_fixed_decimal_string<F>(true);
            break;
        }
        }
        return std::format_to(ctx.out(), "Sfixed{}{{{}}}", R, str_r);
    }
};

template <coconext::types::Range R>
struct std::hash<coconext::types::detail::Sfixed<R>> {
    size_t operator()(coconext::types::detail::Sfixed<R> const& v) const noexcept {
        std::string_view type_name = typeid(coconext::types::detail::Sfixed<R>).name();
        size_t sfixed_seed = std::hash<std::string_view>{}(type_name);
        constexpr size_t W = R.length();
        size_t value_hash = 0;

        if constexpr (W > 0) {
            if constexpr (!coconext::types::detail::Bits<W>::is_wide) {
                auto raw_val = coconext::types::detail::bits(v).raw();
                if constexpr (sizeof(raw_val) > sizeof(size_t)) {
                    uint64_t low = static_cast<uint64_t>(raw_val);
                    uint64_t high = static_cast<uint64_t>(raw_val >> 64);
                    value_hash = coconext::types::detail::hash_combine(low, high);
                } else {
                    value_hash = std::hash<decltype(raw_val)>{}(raw_val);
                }
            } else {
                auto val = coconext::types::detail::bits(v).raw();
                constexpr size_t num_words = (W + 63) / 64;
                for (size_t i = 0; i < num_words; ++i) {
                    value_hash =
                        coconext::types::detail::hash_combine(value_hash, val.word(i));
                }
            }
        }

        return coconext::types::detail::hash_combine(sfixed_seed, R, value_hash);
    }
};

#endif  // COCONEXT_SFIXED_HPP

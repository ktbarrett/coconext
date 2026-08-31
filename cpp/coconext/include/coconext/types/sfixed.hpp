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
    template <size_t SourceW>
    constexpr void assign_signed_integer(SInt<SourceW> const& source) {
        if constexpr (SourceW == 0) {
            value_ = detail::convert_signed_magnitude<R.length()>(
                UInt<SourceW>{}, false, 0, R.right
            );
        } else {
            bool const negative = source.is_negative();
            auto const source_bits = source.logical_bits();
            UInt<SourceW> const magnitude =
                negative ? detail::wrapped_negate(source_bits) : source_bits;
            value_ = detail::convert_signed_magnitude<R.length()>(
                magnitude, negative, 0, R.right
            );
        }
    }

    template <size_t SourceW>
    constexpr void assign_unsigned_integer(UInt<SourceW> const& source) {
        value_ = detail::convert_signed_magnitude<R.length()>(source, false, 0, R.right);
    }

    template <NativeInteger T>
    static constexpr auto integer_operand(T value) {
        constexpr Range IntegerRange{std::numeric_limits<T>::digits, Direction::DOWNTO, 0};
        return Sfixed<IntegerRange>(value);
    }

    template <typename T>
    constexpr T to_native_int() const {
        static_assert(
            R.length() > 0, "Sfixed<0> has no integer value, cannot convert to native int"
        );

        if constexpr (R.right > 0) {
            constexpr Range IntegerRange{R.left, Direction::DOWNTO, 0};
            auto scaled = SInt<IntegerRange.length()>(value_);
            scaled = scaled << R.right;
            return static_cast<T>(Sfixed<IntegerRange>(scaled));
        } else if constexpr (!UInt<R.length()>::is_wide) {
            using RawType = decltype(value_.raw());
            using SignedRawType = std::make_signed_t<RawType>;
            SignedRawType const signed_raw = static_cast<SignedRawType>(value_.raw());

            SignedRawType int_val = 0;
            if constexpr (frac_bits() < std::numeric_limits<SignedRawType>::digits) {
                int_val = static_cast<SignedRawType>(
                    signed_raw / static_cast<SignedRawType>(RawType{1} << frac_bits())
                );
            } else if constexpr (frac_bits() == std::numeric_limits<SignedRawType>::digits)
            {
                int_val = signed_raw == std::numeric_limits<SignedRawType>::min() ? -1 : 0;
            }

            bool out_of_bounds = false;
            if constexpr (std::is_signed_v<T>) {
                if constexpr (
                    std::numeric_limits<T>::digits
                    < std::numeric_limits<SignedRawType>::digits
                )
                {
                    if (int_val < static_cast<SignedRawType>(std::numeric_limits<T>::min())
                        || int_val
                               > static_cast<SignedRawType>(std::numeric_limits<T>::max()))
                    {
                        out_of_bounds = true;
                    }
                }
            } else {
                if (int_val < 0) {
                    out_of_bounds = true;
                } else if constexpr (
                    std::numeric_limits<T>::digits < std::numeric_limits<RawType>::digits
                )
                {
                    if (static_cast<RawType>(int_val)
                        > static_cast<RawType>(std::numeric_limits<T>::max()))
                    {
                        out_of_bounds = true;
                    }
                }
            }

            if (out_of_bounds) {
                throw std::out_of_range("Value too large for destination native type");
            }

            return static_cast<T>(int_val);

        } else {
            bool is_negative = value_.is_negative();
            auto const raw_value = value_.logical_bits();
            UInt<R.length()> abs_value =
                is_negative ? detail::wrapped_negate(raw_value) : raw_value;

            // Truncate fractional bits (logical shift on absolute value == round_to_zero)
            UInt<R.length()> int_magnitude = abs_value >> frac_bits();

            if (int_magnitude == UInt<R.length()>{}) {
                return T{0};
            }

            int msb_index = int_magnitude.highest_set_index();
            bool out_of_bounds = false;

            if (msb_index >= 0) {
                int max_bits = std::numeric_limits<T>::digits;

                if (msb_index > max_bits) {
                    out_of_bounds = true;
                } else if (msb_index == max_bits) {
                    if (is_negative && std::is_signed_v<T>) {
                        UInt<R.length()> remainder = int_magnitude;
                        remainder.set_bit(max_bits, false);
                        if (remainder != UInt<R.length()>{0}) {
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

        if constexpr (!UInt<R.length()>::is_wide) {
            using RawType = decltype(value_.raw());
            using SignedRawType = std::make_signed_t<RawType>;
            SignedRawType const signed_raw = static_cast<SignedRawType>(value_.raw());
            return std::ldexp(static_cast<T>(signed_raw), R.right);
        } else {
            if (value_ == SInt<R.length()>{0}) {
                return T{0.0};
            }

            bool is_negative = value_.is_negative();
            auto const raw_value = value_.logical_bits();
            UInt<R.length()> abs_value =
                is_negative ? detail::wrapped_negate(raw_value) : raw_value;

            int msb_index = abs_value.highest_set_index();
            constexpr int mantissa_bits = std::numeric_limits<T>::digits;

            int shift_amount = 0;
            if (msb_index >= mantissa_bits) {
                shift_amount = msb_index - mantissa_bits + 1;
            }

            auto aligned = abs_value >> shift_amount;
            uint64_t raw_mantissa = static_cast<uint64_t>(aligned.raw().word(0));

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

    template <size_t W, bool SignedRepresentation>
    constexpr Sfixed(Int<W, SignedRepresentation> const& val) : value_(val) {
        static_assert(W == R.length(), "Construction from Int requires identical width");
    }

    // Construct from a native integer
    template <NativeInteger T>
    explicit(std::numeric_limits<T>::digits > R.left || R.right > 0) constexpr Sfixed(T v) {
        static_assert(
            R.direction == Direction::DOWNTO,
            "Construction from int not allowed on a TO Direction Sfixed"
        );
        static_assert(
            R.length() > 0,
            "Sfixed with a null range has no integer representation; use default "
            "construction"
        );
        constexpr size_t SourceW =
            std::numeric_limits<T>::digits + (std::is_signed_v<T> ? 1 : 0);
        if constexpr (std::is_signed_v<T>) {
            assign_signed_integer(SInt<SourceW>(v));
        } else {
            assign_unsigned_integer(UInt<SourceW>(v));
        }
    }

    // Exact numeric conversion from the same kind. Conversions that are guaranteed
    // lossless for every source value are implicit; other conversions are checked.
    template <Range R2>
    explicit(
        !(R2.length() == 0 || (R.length() > 0 && R.left >= R2.left && R.right <= R2.right))
    ) constexpr Sfixed(Sfixed<R2> const& other) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Sfixed same-kind construction requires DOWNTO direction"
        );
        if constexpr (R2.length() == 0) {
            value_ = detail::convert_signed_magnitude<R.length()>(
                bits(other).logical_bits(), false, R2.right, R.right
            );
        } else {
            auto const other_bits = bits(other);
            bool const negative = other_bits.get_bit(R2.length() - 1);
            auto const raw_bits = other_bits.logical_bits();
            UInt<R2.length()> const magnitude =
                negative ? detail::wrapped_negate(raw_bits) : raw_bits;
            value_ = detail::convert_signed_magnitude<R.length()>(
                magnitude, negative, R2.right, R.right
            );
        }
    }

    // Exact numeric conversion from Ufixed.
    template <Range R2>
    explicit(
        !(R2.length() == 0 || (R.length() > 0 && R.left > R2.left && R.right <= R2.right))
    ) constexpr Sfixed(Ufixed<R2> const& other) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Sfixed cross-kind construction requires DOWNTO direction"
        );
        value_ = detail::convert_signed_magnitude<R.length()>(
            bits(other), false, R2.right, R.right
        );
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
        static_assert(
            R.length() > 0,
            "Sfixed with a null range has no floating-point representation; use default "
            "construction"
        );

        if (std::isnan(v)) {
            throw std::domain_error("Cannot convert NaN to fixed-point.");
        }

        constexpr size_t W = R.length();
        UInt<W> const max_signed_bits = ~(UInt<W>(1) << (W - 1));
        UInt<W> const min_signed_bits = UInt<W>(1) << (W - 1);

        if (std::isinf(v)) {
            if (om == overflow_mode::wrap) {
                throw std::domain_error("Cannot wrap Infinity.");
            }
            if (v > 0) {
                value_ = SInt<W>(max_signed_bits);
            } else {
                value_ = SInt<W>(min_signed_bits);
            }
            return;
        }

        bool const negative = v < FloatType{0};
        FloatType const magnitude = negative ? -v : v;
        auto aligned = detail::align_floating_magnitude<W + 1>(magnitude, R.right);
        detail::round_magnitude(aligned, rm, negative);

        UInt<W + 1> const negative_limit = UInt<W + 1>{1} << (W - 1);
        bool const out_of_range =
            aligned.overflow
            || (negative ? aligned.bits > negative_limit : aligned.bits >= negative_limit);

        if (out_of_range && om == overflow_mode::saturate) {
            UInt<W> const sign_bit = UInt<W>{1} << (W - 1);
            value_ = SInt<W>(negative ? sign_bit : ~sign_bit);
        } else {
            UInt<W> const magnitude_bits = aligned.bits.template truncate<W>();
            value_ =
                SInt<W>(negative ? detail::wrapped_negate(magnitude_bits) : magnitude_bits);
        }
    }

    // Construction from Signed
    template <Range R2>
    explicit(
        R2.length() > (R.left < 0 ? 0 : static_cast<size_t>(R.left) + 1) || R.right > 0
    ) constexpr Sfixed(Signed<R2> v)
        requires(R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO)
    {
        assign_signed_integer(bits(v));
    }

    // Construction from Unsigned
    template <Range R2>
    explicit(
        R2.length() > (R.left <= 0 ? 0 : static_cast<size_t>(R.left)) || R.right > 0
    ) constexpr Sfixed(Unsigned<R2> v)
        requires(R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO)
    {
        assign_unsigned_integer(bits(v));
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
        bool negative = false;
        UInt<SourceW> magnitude{};
        if constexpr (SourceW > 0) {
            negative = bits(src).get_bit(SourceW - 1);
            auto const raw_bits = bits(src).logical_bits();
            magnitude = negative ? detail::wrapped_negate(raw_bits) : raw_bits;
        }
        if constexpr (TargetW == 0) {
            value_ = {};
        } else {
            auto aligned =
                detail::align_magnitude<TargetW + 1>(magnitude, R2.right, R.right);
            detail::round_magnitude(aligned, rnd, negative);

            UInt<TargetW + 1> const negative_limit = UInt<TargetW + 1>{1} << (TargetW - 1);
            bool const out_of_range = aligned.overflow
                                   || (negative ? aligned.bits > negative_limit
                                                : aligned.bits >= negative_limit);

            if (out_of_range && ovf == overflow_mode::saturate) {
                UInt<TargetW> const sign_bit = UInt<TargetW>{1} << (TargetW - 1);
                value_ = SInt<TargetW>(negative ? sign_bit : ~sign_bit);
            } else {
                UInt<TargetW> const quantized_magnitude =
                    aligned.bits.template truncate<TargetW>();
                value_ = SInt<TargetW>(
                    negative ? detail::wrapped_negate(quantized_magnitude)
                             : quantized_magnitude
                );
            }
        }
    }

    template <typename SourceWrapper>
    constexpr Sfixed& operator=(detail::auto_resized<SourceWrapper>&& wrapper) {
        *this = Sfixed(std::move(wrapper));
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
        return value_ != SInt<R.length()>{};
    }

    explicit constexpr operator signed char() const noexcept(
        R.left <= std::numeric_limits<signed char>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<signed char>();
    }

    explicit constexpr operator unsigned char() const
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned char>();
    }

    explicit constexpr operator short() const noexcept(
        R.left <= std::numeric_limits<short>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<short>();
    }

    explicit constexpr operator unsigned short() const
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned short>();
    }

    explicit constexpr operator int() const noexcept(
        R.left <= std::numeric_limits<int>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<int>();
    }

    explicit constexpr operator unsigned int() const
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned int>();
    }

    explicit constexpr operator long() const noexcept(
        R.left <= std::numeric_limits<long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<long>();
    }

    explicit constexpr operator unsigned long() const
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned long>();
    }

    explicit constexpr operator long long() const noexcept(
        R.left <= std::numeric_limits<long long>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<long long>();
    }

    explicit constexpr operator unsigned long long() const
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<unsigned long long>();
    }

#if defined(__SIZEOF_INT128__)
    explicit constexpr operator __int128_t() const noexcept(
        R.left <= std::numeric_limits<__int128_t>::digits
    )
        requires(R.direction == Direction::DOWNTO)
    {
        return to_native_int<__int128_t>();
    }

    explicit constexpr operator __uint128_t() const
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
        static_assert(R.length() > 0, "shift on Sfixed with a null range is undefined");
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
    constexpr Sfixed& operator<<=(T amount) {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(R.length() > 0, "shift on Sfixed with a null range is undefined");
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
        static_assert(R.length() > 0, "shift on Sfixed with a null range is undefined");
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

        return Sfixed<R>(value_ >> v);
    }

    template <typename T>
    constexpr Sfixed& operator>>=(T amount) {
        static_assert(
            R.direction == Direction::DOWNTO, "Shift operation requires downto Direction"
        );
        static_assert(R.length() > 0, "shift on Sfixed with a null range is undefined");
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

        value_ = value_ >> v;
        return *this;
    }

    template <Range R2>
    constexpr std::strong_ordering operator<=>(Sfixed<R2> const& other) const noexcept
        requires(R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO)
    {
        static_assert(R == R2, "Comparison requires equal Ranges");
        static_assert(R.length() > 0, "ordering an Sfixed with a null range is undefined");

        if (value_ == other.value_) {
            return std::strong_ordering::equal;
        }
        if (value_ > other.value_) {
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

        auto extended_bits = SInt<TargetW>(value_);
        return Sfixed<TR>(SInt<TargetW>::exact_sub(SInt<TargetW>{}, extended_bits));
    }

    template <Range R2>
    constexpr auto operator+(Sfixed<R2> const& rhs) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range R_res{
            std::max(R.left, R2.left) + 1, Direction::DOWNTO, std::min(R.right, R2.right)
        };
        constexpr size_t ShiftL = R.right - R_res.right;
        constexpr size_t ShiftR = R2.right - R_res.right;

        auto lhs_aligned =
            detail::shift_left_sign_extended<R.length() + ShiftL>(value_, ShiftL);
        auto rhs_aligned =
            detail::shift_left_sign_extended<R2.length() + ShiftR>(bits(rhs), ShiftR);

        return Sfixed<R_res>(lhs_aligned + rhs_aligned);
    }

    template <Range R2>
    constexpr auto operator-(Sfixed<R2> const& rhs) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range R_res{
            std::max(R.left, R2.left) + 1, Direction::DOWNTO, std::min(R.right, R2.right)
        };
        constexpr size_t ShiftL = R.right - R_res.right;
        constexpr size_t ShiftR = R2.right - R_res.right;

        auto lhs_aligned =
            detail::shift_left_sign_extended<R.length() + ShiftL>(value_, ShiftL);
        auto rhs_aligned =
            detail::shift_left_sign_extended<R2.length() + ShiftR>(bits(rhs), ShiftR);

        return Sfixed<R_res>(lhs_aligned - rhs_aligned);
    }

    template <Range R2>
    constexpr auto operator*(Sfixed<R2> const& rhs) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range R_res{R.left + R2.left + 1, Direction::DOWNTO, R.right + R2.right};
        return Sfixed<R_res>(value_ * bits(rhs));
    }

    template <Range R2>
    constexpr auto operator/(Sfixed<R2> const& rhs) const {
        return divide(rhs);
    }

    template <Range R2>
    constexpr auto divide(
        Sfixed<R2> const& rhs,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        return divrem(rhs, rounding, guard_bits).first;
    }

    template <Range R2>
    constexpr auto divrem(
        Sfixed<R2> const& rhs,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range QuotientRange{
            R.left - R2.right + 1, Direction::DOWNTO, R.right - R2.left
        };
        constexpr auto result_right = std::min(R.right, R2.right);
        constexpr Range RemainderRange{
            std::min(R.left, R2.left), Direction::DOWNTO, result_right
        };
        constexpr size_t ShiftL = detail::index_distance(R.right, result_right);
        constexpr size_t ShiftR = detail::index_distance(R2.right, result_right);

        auto lhs_aligned =
            detail::shift_left_sign_extended<R.length() + ShiftL>(value_, ShiftL);
        auto rhs_aligned =
            detail::shift_left_sign_extended<R2.length() + ShiftR>(bits(rhs), ShiftR);
        auto [quotient_bits, remainder_bits] =
            detail::divrem_signed_fixed<false, QuotientRange.length(), QuotientRange.right>(
                lhs_aligned, rhs_aligned, rounding, guard_bits
            );

        constexpr size_t RemainderW = RemainderRange.length();
        constexpr size_t AlignedDivisorW = R2.length() + ShiftR;
        if constexpr (RemainderW <= AlignedDivisorW) {
            return std::pair{
                Sfixed<QuotientRange>(quotient_bits),
                Sfixed<RemainderRange>(remainder_bits.template truncate<RemainderW>())
            };
        } else {
            return std::pair{
                Sfixed<QuotientRange>(quotient_bits),
                Sfixed<RemainderRange>(SInt<RemainderW>(remainder_bits))
            };
        }
    }

    template <Range R2>
    constexpr auto divmod(
        Sfixed<R2> const& rhs,
        [[maybe_unused]] overflow_mode overflow = overflow_mode::saturate,
        round_mode rounding = round_mode::round_to_even,
        size_t guard_bits = fixed_guard_bits
    ) const {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        constexpr Range QuotientRange{
            R.left - R2.right + 1, Direction::DOWNTO, R.right - R2.left
        };
        constexpr auto result_right = std::min(R.right, R2.right);
        constexpr Range ModuloRange{R2.left, Direction::DOWNTO, result_right};
        constexpr size_t ShiftL = detail::index_distance(R.right, result_right);
        constexpr size_t ShiftR = detail::index_distance(R2.right, result_right);

        auto lhs_aligned =
            detail::shift_left_sign_extended<R.length() + ShiftL>(value_, ShiftL);
        auto rhs_aligned =
            detail::shift_left_sign_extended<R2.length() + ShiftR>(bits(rhs), ShiftR);
        auto [quotient_bits, modulo_bits] =
            detail::divrem_signed_fixed<true, QuotientRange.length(), QuotientRange.right>(
                lhs_aligned, rhs_aligned, rounding, guard_bits
            );

        constexpr size_t ModuloW = ModuloRange.length();
        constexpr size_t AlignedDivisorW = R2.length() + ShiftR;
        if constexpr (ModuloW <= AlignedDivisorW) {
            return std::pair{
                Sfixed<QuotientRange>(quotient_bits),
                Sfixed<ModuloRange>(modulo_bits.template truncate<ModuloW>())
            };
        } else {
            return std::pair{
                Sfixed<QuotientRange>(quotient_bits),
                Sfixed<ModuloRange>(SInt<ModuloW>(modulo_bits))
            };
        }
    }

    template <Range R2>
    constexpr auto operator%(Sfixed<R2> const& rhs) const {
        return divrem(rhs).second;
    }

    template <Range R2>
    constexpr Sfixed& operator+=(Sfixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this + rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }
    template <Range R2>
    constexpr Sfixed& operator-=(Sfixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this - rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }
    template <Range R2>
    constexpr Sfixed& operator*=(Sfixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this * rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }
    template <Range R2>
    constexpr Sfixed& operator/=(Sfixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this / rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }
    template <Range R2>
    constexpr Sfixed& operator%=(Sfixed<R2> const& rhs) {
        static_assert(
            R.direction == Direction::DOWNTO && R2.direction == Direction::DOWNTO,
            "Operations require DOWNTO"
        );
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (R.length() == 0) {
            return *this;
        }
        *this = coconext::types::resize(
            *this % rhs, overflow_mode::wrap, round_mode::round_to_zero
        );
        return *this;
    }

    template <NativeInteger T>
    constexpr Sfixed& operator+=(T const& rhs) {
        static_assert(R.direction == Direction::DOWNTO, "Downto Direction required");

        *this += integer_operand(rhs);
        return *this;
    }
    template <NativeInteger T>
    constexpr Sfixed& operator-=(T const& rhs) {
        *this -= integer_operand(rhs);
        return *this;
    }
    template <NativeInteger T>
    constexpr Sfixed& operator*=(T const& rhs) {
        *this *= integer_operand(rhs);
        return *this;
    }
    template <NativeInteger T>
    constexpr Sfixed& operator/=(T const& rhs) {
        *this /= integer_operand(rhs);
        return *this;
    }
    template <NativeInteger T>
    constexpr Sfixed& operator%=(T const& rhs) {
        *this %= integer_operand(rhs);
        return *this;
    }

    constexpr Sfixed& operator++() {
        *this += 1;
        return *this;
    }
    constexpr Sfixed operator++(int) {
        Sfixed tmp = *this;
        *this += 1;
        return tmp;
    }
    constexpr Sfixed& operator--() {
        *this -= 1;
        return *this;
    }
    constexpr Sfixed operator--(int) {
        Sfixed tmp = *this;
        *this -= 1;
        return tmp;
    }

    static constexpr Range::value_type frac_bits() {
        auto const lsb = R.direction == Direction::DOWNTO ? R.right : R.left;
        return -lsb;
    }

    static constexpr Range::value_type int_bits() {
        auto const msb = R.direction == Direction::DOWNTO ? R.left : R.right;
        return msb + 1;
    }

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

    constexpr auto begin() { return value_.begin(); }
    constexpr auto rbegin() { return value_.rbegin(); }
    constexpr auto begin() const { return value_.begin(); }
    constexpr auto rbegin() const { return value_.rbegin(); }

    constexpr auto end() { return value_.end(); }
    constexpr auto rend() { return value_.rend(); }
    constexpr auto end() const { return value_.end(); }
    constexpr auto rend() const { return value_.rend(); }

    constexpr auto operator[](Range::value_type idx) {
        auto const offset = offset_of(R, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        size_t bit_pos = R.length() - 1 - offset.value();

        return value_[bit_pos];
    }

    constexpr auto operator[](Range::value_type idx) const {
        auto const offset = offset_of(R, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        size_t bit_pos = R.length() - 1 - offset.value();

        return value_[bit_pos];
    }

  private:
    friend struct bits_fn;

    SInt<R.length()> value_{};
};

template <Range R1, Range R2>
constexpr auto operator+(Ufixed<R1> const& lhs, Sfixed<R2> const& rhs) {
    return (+lhs) + rhs;
}
template <Range R1, Range R2>
constexpr auto operator-(Ufixed<R1> const& lhs, Sfixed<R2> const& rhs) {
    return (+lhs) - rhs;
}
template <Range R1, Range R2>
constexpr auto operator*(Ufixed<R1> const& lhs, Sfixed<R2> const& rhs) {
    return (+lhs) * rhs;
}
template <Range R1, Range R2>
constexpr auto operator/(Ufixed<R1> const& lhs, Sfixed<R2> const& rhs) {
    return (+lhs) / rhs;
}
template <Range R1, Range R2>
constexpr auto operator%(Ufixed<R1> const& lhs, Sfixed<R2> const& rhs) {
    return (+lhs) % rhs;
}

template <Range R1, Range R2>
constexpr auto operator+(Sfixed<R1> const& lhs, Ufixed<R2> const& rhs) {
    return lhs + (+rhs);
}
template <Range R1, Range R2>
constexpr auto operator-(Sfixed<R1> const& lhs, Ufixed<R2> const& rhs) {
    return lhs - (+rhs);
}
template <Range R1, Range R2>
constexpr auto operator*(Sfixed<R1> const& lhs, Ufixed<R2> const& rhs) {
    return lhs * (+rhs);
}
template <Range R1, Range R2>
constexpr auto operator/(Sfixed<R1> const& lhs, Ufixed<R2> const& rhs) {
    return lhs / (+rhs);
}
template <Range R1, Range R2>
constexpr auto operator%(Sfixed<R1> const& lhs, Ufixed<R2> const& rhs) {
    return lhs % (+rhs);
}

template <Range R1, Range R2>
constexpr auto operator+(Ufixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a + Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator-(Ufixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a - Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator*(Ufixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a * Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator/(Ufixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a / Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator%(Ufixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a % Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator+(Signed<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) + b;
}

template <Range R1, Range R2>
constexpr auto operator-(Signed<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) - b;
}

template <Range R1, Range R2>
constexpr auto operator*(Signed<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) * b;
}

template <Range R1, Range R2>
constexpr auto operator/(Signed<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) / b;
}

template <Range R1, Range R2>
constexpr auto operator%(Signed<R1> const& a, Ufixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) % b;
}

template <Range R1, Range R2>
constexpr auto operator+(Sfixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a + Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator-(Sfixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a - Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator*(Sfixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a * Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator/(Sfixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a / Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator%(Sfixed<R1> const& a, Signed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R2.length());
    return a % Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator+(Signed<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) + b;
}

template <Range R1, Range R2>
constexpr auto operator-(Signed<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) - b;
}

template <Range R1, Range R2>
constexpr auto operator*(Signed<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) * b;
}

template <Range R1, Range R2>
constexpr auto operator/(Signed<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) / b;
}

template <Range R1, Range R2>
constexpr auto operator%(Signed<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = int_downto_range(R1.length());
    return Sfixed<f_range>(a) % b;
}

template <Range R1, Range R2>
constexpr auto operator+(Sfixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = Range{R2.length(), Direction::DOWNTO, 0};
    return a + Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator-(Sfixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = Range{R2.length(), Direction::DOWNTO, 0};
    return a - Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator*(Sfixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = Range{R2.length(), Direction::DOWNTO, 0};
    return a * Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator/(Sfixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = Range{R2.length(), Direction::DOWNTO, 0};
    return a / Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator%(Sfixed<R1> const& a, Unsigned<R2> const& b) {
    constexpr auto f_range = Range{R2.length(), Direction::DOWNTO, 0};
    return a % Sfixed<f_range>(b);
}

template <Range R1, Range R2>
constexpr auto operator+(Unsigned<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = Range{R1.length(), Direction::DOWNTO, 0};
    return Sfixed<f_range>(a) + b;
}

template <Range R1, Range R2>
constexpr auto operator-(Unsigned<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = Range{R1.length(), Direction::DOWNTO, 0};
    return Sfixed<f_range>(a) - b;
}

template <Range R1, Range R2>
constexpr auto operator*(Unsigned<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = Range{R1.length(), Direction::DOWNTO, 0};
    return Sfixed<f_range>(a) * b;
}

template <Range R1, Range R2>
constexpr auto operator/(Unsigned<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = Range{R1.length(), Direction::DOWNTO, 0};
    return Sfixed<f_range>(a) / b;
}

template <Range R1, Range R2>
constexpr auto operator%(Unsigned<R1> const& a, Sfixed<R2> const& b) {
    constexpr auto f_range = Range{R1.length(), Direction::DOWNTO, 0};
    return Sfixed<f_range>(a) % b;
}

template <typename T>
inline constexpr bool is_coconext_numeric_v = is_coconext_unsigned_v<std::remove_cvref_t<T>>
                                           || is_coconext_signed_v<std::remove_cvref_t<T>>
                                           || is_coconext_ufixed_v<std::remove_cvref_t<T>>
                                           || is_coconext_sfixed_v<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_coconext_fixed_v = is_coconext_ufixed_v<std::remove_cvref_t<T>>
                                         || is_coconext_sfixed_v<std::remove_cvref_t<T>>;

template <typename LHS, typename RHS>
concept MixedFixedArithmetic = is_coconext_numeric_v<LHS> && is_coconext_numeric_v<RHS>
                            && (is_coconext_fixed_v<LHS> || is_coconext_fixed_v<RHS>)
                            && !(is_coconext_ufixed_v<std::remove_cvref_t<LHS>>
                                 && is_coconext_ufixed_v<std::remove_cvref_t<RHS>>)
                            && !(is_coconext_sfixed_v<std::remove_cvref_t<LHS>>
                                 && is_coconext_sfixed_v<std::remove_cvref_t<RHS>>);

template <typename LHS, typename Result>
constexpr LHS& assign_mixed_fixed_result(LHS& lhs, Result&& result) {
    using LhsType = std::remove_cvref_t<LHS>;
    using ResultType = std::remove_cvref_t<Result>;

    if constexpr (is_coconext_ufixed_v<LhsType> && is_coconext_sfixed_v<ResultType>) {
        if constexpr (ResultType::size() > 0) {
            if (bits(result).get_bit(ResultType::size() - 1)) {
                throw std::out_of_range(
                    "compound arithmetic does not allow a negative Ufixed result"
                );
            }
        }
    }

    constexpr auto target_range = [] {
        if constexpr (is_coconext_fixed_v<LhsType>) {
            return LhsType::static_range;
        } else {
            return int_downto_range(LhsType::size());
        }
    }();

    auto resized = coconext::types::resize<target_range>(
        std::forward<Result>(result), overflow_mode::wrap, round_mode::round_to_zero
    );
    lhs = coconext::types::as<LhsType>(resized);
    return lhs;
}

template <typename LHS, typename RHS>
    requires MixedFixedArithmetic<LHS, RHS>
constexpr LHS& operator+=(LHS& lhs, RHS const& rhs) {
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        return lhs;
    } else {
        return assign_mixed_fixed_result(lhs, lhs + rhs);
    }
}

template <typename LHS, typename RHS>
    requires MixedFixedArithmetic<LHS, RHS>
constexpr LHS& operator-=(LHS& lhs, RHS const& rhs) {
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        return lhs;
    } else {
        return assign_mixed_fixed_result(lhs, lhs - rhs);
    }
}

template <typename LHS, typename RHS>
    requires MixedFixedArithmetic<LHS, RHS>
constexpr LHS& operator*=(LHS& lhs, RHS const& rhs) {
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        return lhs;
    } else {
        return assign_mixed_fixed_result(lhs, lhs * rhs);
    }
}

template <typename LHS, typename RHS>
    requires MixedFixedArithmetic<LHS, RHS>
constexpr LHS& operator/=(LHS& lhs, RHS const& rhs) {
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return lhs;
    } else {
        return assign_mixed_fixed_result(lhs, lhs / rhs);
    }
}

template <typename LHS, typename RHS>
    requires MixedFixedArithmetic<LHS, RHS>
constexpr LHS& operator%=(LHS& lhs, RHS const& rhs) {
    if constexpr (std::remove_cvref_t<LHS>::size() == 0) {
        if (!static_cast<bool>(rhs)) {
            throw std::domain_error("Division by zero");
        }
        return lhs;
    } else {
        return assign_mixed_fixed_result(lhs, lhs % rhs);
    }
}

}  // namespace detail

template <Range R>
inline constexpr bool is_fixed<detail::Sfixed<R>> = true;

template <auto... Args>
using Sfixed = detail::Sfixed<detail::make_fixed_range<Args...>()>;

template <Range R1, Range R2>
constexpr auto divide(
    detail::Sfixed<R1> const& lhs,
    detail::Sfixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divide(rhs, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto remainder(
    detail::Sfixed<R1> const& lhs,
    detail::Sfixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits).second;
}

template <Range R1, Range R2>
constexpr auto rem(
    detail::Sfixed<R1> const& lhs,
    detail::Sfixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return remainder(lhs, rhs, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto modulo(
    detail::Sfixed<R1> const& lhs,
    detail::Sfixed<R2> const& rhs,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divmod(rhs, overflow, rounding, guard_bits).second;
}

template <Range R1, Range R2>
constexpr auto mod(
    detail::Sfixed<R1> const& lhs,
    detail::Sfixed<R2> const& rhs,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return modulo(lhs, rhs, overflow, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto divrem(
    detail::Sfixed<R1> const& lhs,
    detail::Sfixed<R2> const& rhs,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divrem(rhs, rounding, guard_bits);
}

template <Range R1, Range R2>
constexpr auto divmod(
    detail::Sfixed<R1> const& lhs,
    detail::Sfixed<R2> const& rhs,
    overflow_mode overflow = overflow_mode::saturate,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    return lhs.divmod(rhs, overflow, rounding, guard_bits);
}

template <Range R>
constexpr auto reciprocal(
    detail::Sfixed<R> const& value,
    round_mode rounding = round_mode::round_to_even,
    size_t guard_bits = fixed_guard_bits
) {
    constexpr Range ResultRange{-R.right + 1, Direction::DOWNTO, -R.left};
    auto quotient = divide(Sfixed<1, 0>{1}, value, rounding, guard_bits);
    return Sfixed<ResultRange>(quotient);
}

template <auto... Args, typename X>
    requires(
        sizeof...(Args) > 0 && sizeof...(Args) <= 3
        && detail::is_coconext_sfixed_v<std::remove_cvref_t<X>>
    )
constexpr auto resize(X&& x, overflow_mode ovf, round_mode rnd) {
    constexpr Range TargetRange = detail::make_fixed_range<Args...>();
    return Sfixed<TargetRange>(detail::resize(std::forward<X>(x), ovf, rnd));
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
    static_assert(R.length() > 0, "abs on Sfixed with a null range is undefined");
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
            constexpr auto F = coconext::types::detail::Sfixed<R>::frac_bits();
            constexpr auto I = coconext::types::detail::Sfixed<R>::int_bits();
            constexpr size_t decimal_pos = F > 0 && I > 0 ? static_cast<size_t>(F) : 0;
            str_r = coconext::types::detail::bits(v).to_binary_string(decimal_pos);
            break;
        }
        default: {
            constexpr auto F = coconext::types::detail::Sfixed<R>::frac_bits();
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
            if constexpr (!coconext::types::detail::SInt<W>::is_wide) {
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

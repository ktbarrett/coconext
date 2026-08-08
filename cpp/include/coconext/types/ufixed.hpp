#ifndef COCONEXT_UFIXED_HPP
#define COCONEXT_UFIXED_HPP

#include <cmath>
#include <coconext/types/bits.hpp>
#include <coconext/types/concepts.hpp>
#include <coconext/types/range.hpp>

namespace coconext::types {

namespace detail {

template <Range R>
class Ufixed {
  public:
    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return R; }
    static constexpr size_t size() noexcept { return R.length(); }

    constexpr Ufixed() noexcept : value_(0) {}

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

        if constexpr (std::numeric_limits<T>::digits <= (R.left + 1)) {
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

    template <Range R2>
    constexpr Ufixed(Unsigned<R2> v) {
        static_assert(
            R.right == 0, "Construction from Unsigned requires zero fractional bits"
        );
        static_assert(
            R.length() == R2.length(), "Construction from Unsigned requires equal length"
        );
        value_ = v.value_;
    }

    // Implicit conversion to supertype BitArray
    template <Range R2>
    constexpr operator detail::Array<Bit, R2>() const noexcept {
        static_assert(
            R.length() == R2.length(), "BitArray reinterpret requires identical width"
        );
        return detail::Array<Bit, R2>(value_);
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
        if constexpr (R.direction == Direction::DOWNTO) {
            return std::pow(2, R.right);
        } else {
            return std::pow(2, R.left);
        }
    }

  private:
    Bits<R.length()> value_;
};

}  // namespace detail

template <Range R>
inline constexpr bool is_fixed<detail::Ufixed<R>> = true;

// see detail::make_fixed_range for the rules
template <auto... Args>
using Ufixed = detail::Ufixed<detail::make_fixed_range<Args...>()>;

}  // namespace coconext::types

#endif  // COCONEXT_UFIXED_HPP

// TODOs

// All arithmetic (+, -, *, /, mod, rem, unary -/+, compound assignment, shifts) requires
// R.direction == Direction::DOWNTO on every Sfixed/Ufixed operand.All comparisons (==, <=>,
// <, <=, >, >=) requires DOWNTO. The resize family requires DOWNTO for Sfixed/Ufixed
// sources and Sfixed/Ufixed destinations. explicit operator double/float/long double and
// the explicit integer-egress operators requires DOWNTO. The double ctor produces a
// DOWNTO-direction result type (it never produces a TO). int_bits() and frac_bits()
// accessors are direction-independent (the count of integer-positions and
// fractional-positions is a property of the position set, not the layout) and are available
// on both directions. This split means a Sfixed read out of a TO-ranged HDL handle can be
// inspected (format binary, slice, copy, hash via BitArray) without operator failure, but
// attempting a + b or double(a) on a TO-ranged value is a compile error with a direct fix:
// either as<Sfixed<reverse(R)>>(a) if the user believes the bits already follow DOWNTO
// weight despite the TO declaration, or reverse(a) if the bits follow the declared (TO)
// weight and need to be physically re-ordered. The user must choose; the library does not
// guess.

// reverse(x) and as<U>(x)

// same kind operations

// resize<...>(Ufixed)

// as<>/reverse overloads

// formatter and hash

// Governing principle for conversions
// The same principle as Unsigned/Signed:
// Operations which can change type without value loss are implicitly allowed. Lossy
// operations are explicit. We care about the value, not the type. Two carve-outs for
// ergonomic universals: compound assignment wraps at LHS range; shifts are destructive.
// Both noted at the relevant operator sections.

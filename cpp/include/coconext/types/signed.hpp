#ifndef COCONEXT_SIGNED_HPP
#define COCONEXT_SIGNED_HPP

#include <algorithm>
#include <coconext/types/concepts.hpp>
#include <coconext/types/int_base.hpp>
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
class Signed;

template <Range R>
constexpr Signed<R> make_signed(uint64_t bits) noexcept;

// Fixed-width two's-complement signed integer with wrap-on-overflow. The
// indexing range R carries HDL coordinates; only its length (in bits) matters
// for arithmetic. Backed by a single int64_t, so length is limited to 1..64.
// The stored value is always kept sign-extended from bit N-1.
template <Range R>
class Signed  // TODO inherit from BitArray for indexing, slicing, iteration, and implicit
              // conversion to BitArray<R>
{
    static constexpr Range static_range = R;
    static_assert(R.length() >= 0, "Signed width must not be negative");

  public:
    static constexpr Range range() noexcept { return R; }
    static constexpr size_t size() noexcept { return R.length(); }

    constexpr Signed() noexcept = default;

    // Construct from a native integer. Throws std::out_of_range if the value
    // does not fit in the N-bit signed range.
    template <Integer T>
    explicit constexpr Signed(T v) {
        auto const s = static_cast<int64_t>(v);
        value_ = v;
    }

    // Cross-width conversion. Throws if the source value doesn't fit in N bits.
    template <Range R2>
    explicit constexpr Signed(Signed<R2> other) {
        value_ = other.value();
    }

    // TODO this is to be removed and made a free function E.g. to_int()
    template <Integer T>
    constexpr T to() const {
        if (value_ < static_cast<int64_t>(std::numeric_limits<T>::min())
            || value_ > static_cast<int64_t>(std::numeric_limits<T>::max()))
        {
            throw std::out_of_range("Signed value does not fit in target type");
        }
        return static_cast<T>(value_);
    }

    constexpr Signed operator+() const noexcept { return *this; }
    constexpr Signed operator-() const noexcept {
        return make_signed<R>(~static_cast<uint64_t>(value_) + 1);
    }
    constexpr Signed operator~() const noexcept {
        return make_signed<R>(~static_cast<uint64_t>(value_));
    }

    constexpr Signed& operator++() noexcept {
        value_ = static_cast<uint64_t>(value_) + 1, size();
        return *this;
    }
    constexpr Signed operator++(int) noexcept {
        auto const old = *this;
        ++*this;
        return old;
    }
    constexpr Signed& operator--() noexcept {
        value_ = (static_cast<uint64_t>(value_) - 1, size());
        return *this;
    }
    constexpr Signed operator--(int) noexcept {
        auto const old = *this;
        --*this;
        return old;
    }

    // << drops bits beyond N; >> is arithmetic (sign-extending).
    constexpr Signed operator<<(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        return make_signed<R>(static_cast<uint64_t>(value_) << amount);
    }
    constexpr Signed operator>>(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        // Arithmetic right shift: shifting by >= width collapses to the sign.
        auto const shift =
            amount >= static_cast<int>(size()) ? static_cast<int>(size()) - 1 : amount;
        return make_signed<R>(static_cast<uint64_t>(value_ >> shift));
    }

    template <Range R2>
    constexpr Signed& operator+=(Signed<R2> rhs) noexcept {
        value_ =
            (static_cast<uint64_t>(value_) + static_cast<uint64_t>(rhs.value()), size());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator-=(Signed<R2> rhs) noexcept {
        value_ =
            (static_cast<uint64_t>(value_) - static_cast<uint64_t>(rhs.value()), size());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator*=(Signed<R2> rhs) noexcept {
        value_ =
            (static_cast<uint64_t>(value_) * static_cast<uint64_t>(rhs.value()), size());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator/=(Signed<R2> rhs) {
        if (rhs.value() == 0) {
            throw std::domain_error("division by zero");
        }
        value_ = (static_cast<uint64_t>(value_ / rhs.value()), size());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator%=(Signed<R2> rhs) {
        if (rhs.value() == 0) {
            throw std::domain_error("modulo by zero");
        }
        value_ = (static_cast<uint64_t>(value_ % rhs.value()), size());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator&=(Signed<R2> rhs) noexcept {
        value_ =
            (static_cast<uint64_t>(value_) & static_cast<uint64_t>(rhs.value()), size());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator|=(Signed<R2> rhs) noexcept {
        value_ =
            (static_cast<uint64_t>(value_) | static_cast<uint64_t>(rhs.value()), size());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator^=(Signed<R2> rhs) noexcept {
        value_ =
            (static_cast<uint64_t>(value_) ^ static_cast<uint64_t>(rhs.value()), size());
        return *this;
    }
    constexpr Signed& operator<<=(int amount) {
        *this = *this << amount;
        return *this;
    }
    constexpr Signed& operator>>=(int amount) {
        *this = *this >> amount;
        return *this;
    }

  private:
    struct raw_tag {};
    constexpr Signed(raw_tag, uint64_t bits) noexcept : value_((bits, size())) {}

    int64_t value_ = 0;

    friend constexpr Signed detail::make_signed<R>(uint64_t bits) noexcept;
};

template <Range R>
constexpr Signed<R> make_signed(uint64_t bits) noexcept {
    return Signed<R>(typename Signed<R>::raw_tag{}, bits);
}

}  // namespace detail

template <auto... Args>
using Signed = detail::Signed<detail::make_int_range<Args...>()>;

namespace detail {

// -- binary arithmetic: result range is {max(width)-1 DOWNTO 0} --------------
// Operands are widened to the result width (sign-preserving) before the op via
// the make_signed wrap step, which sign-extends from the result width.
// Defined inside detail:: so ADL finds them from detail::Signed<R> arguments.

template <Range A, Range B>
constexpr Signed<int_result_range<A, B>> operator+(Signed<A> a, Signed<B> b) noexcept {
    return make_signed<int_result_range<A, B>>(
        static_cast<uint64_t>(a.value()) + static_cast<uint64_t>(b.value())
    );
}
template <Range A, Range B>
constexpr Signed<int_result_range<A, B>> operator-(Signed<A> a, Signed<B> b) noexcept {
    return make_signed<int_result_range<A, B>>(
        static_cast<uint64_t>(a.value()) - static_cast<uint64_t>(b.value())
    );
}
template <Range A, Range B>
constexpr Signed<int_result_range<A, B>> operator*(Signed<A> a, Signed<B> b) noexcept {
    return make_signed<int_result_range<A, B>>(
        static_cast<uint64_t>(a.value()) * static_cast<uint64_t>(b.value())
    );
}
template <Range A, Range B>
constexpr Signed<int_result_range<A, B>> operator/(Signed<A> a, Signed<B> b) {
    if (b.value() == 0) {
        throw std::domain_error("division by zero");
    }
    return make_signed<int_result_range<A, B>>(
        static_cast<uint64_t>(a.value() / b.value())
    );
}
template <Range A, Range B>
constexpr Signed<int_result_range<A, B>> operator%(Signed<A> a, Signed<B> b) {
    if (b.value() == 0) {
        throw std::domain_error("modulo by zero");
    }
    return make_signed<int_result_range<A, B>>(
        static_cast<uint64_t>(a.value() % b.value())
    );
}

template <Range A, Range B>
constexpr Signed<int_result_range<A, B>> operator&(Signed<A> a, Signed<B> b) noexcept {
    return make_signed<int_result_range<A, B>>(
        static_cast<uint64_t>(a.value()) & static_cast<uint64_t>(b.value())
    );
}
template <Range A, Range B>
constexpr Signed<int_result_range<A, B>> operator|(Signed<A> a, Signed<B> b) noexcept {
    return make_signed<int_result_range<A, B>>(
        static_cast<uint64_t>(a.value()) | static_cast<uint64_t>(b.value())
    );
}
template <Range A, Range B>
constexpr Signed<int_result_range<A, B>> operator^(Signed<A> a, Signed<B> b) noexcept {
    return make_signed<int_result_range<A, B>>(
        static_cast<uint64_t>(a.value()) ^ static_cast<uint64_t>(b.value())
    );
}

template <Range A, Range B>
constexpr bool operator==(Signed<A> a, Signed<B> b) noexcept {
    return a.value() == b.value();
}
template <Range A, Range B>
constexpr bool operator!=(Signed<A> a, Signed<B> b) noexcept {
    return a.value() != b.value();
}
template <Range A, Range B>
constexpr bool operator<(Signed<A> a, Signed<B> b) noexcept {
    return a.value() < b.value();
}
template <Range A, Range B>
constexpr bool operator<=(Signed<A> a, Signed<B> b) noexcept {
    return a.value() <= b.value();
}
template <Range A, Range B>
constexpr bool operator>(Signed<A> a, Signed<B> b) noexcept {
    return a.value() > b.value();
}
template <Range A, Range B>
constexpr bool operator>=(Signed<A> a, Signed<B> b) noexcept {
    return a.value() >= b.value();
}

}  // namespace detail

}  // namespace coconext::types

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Signed<R>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Signed formatter takes no format spec");
        }
        return it;
    }
    auto format(
        coconext::types::detail::Signed<R> const& v, std::format_context& ctx
    ) const {
        return std::format_to(ctx.out(), "{}", v.value());
    }
};

template <coconext::types::Range R>
struct std::hash<coconext::types::detail::Signed<R>> {
    size_t operator()(coconext::types::detail::Signed<R> const& v) const noexcept {
        return std::hash<int64_t>{}(v.value());
    }
};

// TODO
// User Defined Literals for Signed Integers

#endif  // COCONEXT_SIGNED_HPP

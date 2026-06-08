#ifndef COCONEXT_SIGNED_HPP
#define COCONEXT_SIGNED_HPP

#include <algorithm>
#include <coconext/types/concepts.hpp>
#include <coconext/types/int_common.hpp>
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

// Smallest/largest values representable in a width-w two's-complement field.
constexpr int64_t sint_min(unsigned w) noexcept {
    return w >= int_max_width ? std::numeric_limits<int64_t>::min()
                              : -(int64_t{1} << (w - 1));
}
constexpr int64_t sint_max(unsigned w) noexcept {
    return w >= int_max_width ? std::numeric_limits<int64_t>::max()
                              : (int64_t{1} << (w - 1)) - 1;
}

// Fixed-width two's-complement signed integer with wrap-on-overflow. The
// indexing range R carries HDL coordinates; only its length (in bits) matters
// for arithmetic. Backed by a single int64_t, so length is limited to 1..64.
// The stored value is always kept sign-extended from bit N-1.
template <Range R>
class Signed {
    static_assert(
        R.length() >= 1 && R.length() <= int_max_width, "Signed width must be 1..64"
    );

  public:
    using storage_type = int64_t;

    static constexpr Range range() noexcept { return R; }
    static constexpr size_t width() noexcept { return R.length(); }

    constexpr Signed() noexcept = default;

    // Construct from a native integer. Throws std::out_of_range if the value
    // does not fit in the N-bit signed range.
    template <Integer T>
    explicit constexpr Signed(T v) {
        auto const s = static_cast<int64_t>(v);
        // For unsigned T larger than int64_t range, the cast above could go
        // negative; guard that explicitly.
        if constexpr (std::is_unsigned_v<T>) {
            if (static_cast<uint64_t>(v) > static_cast<uint64_t>(sint_max(width()))) {
                throw std::out_of_range("value does not fit in Signed width");
            }
        } else {
            if (s < sint_min(width()) || s > sint_max(width())) {
                throw std::out_of_range("value does not fit in Signed width");
            }
        }
        value_ = s;
    }

    // Cross-width conversion. Throws if the source value doesn't fit in N bits.
    template <Range R2>
    explicit constexpr Signed(Signed<R2> other) {
        if (other.value() < sint_min(width()) || other.value() > sint_max(width())) {
            throw std::out_of_range("value does not fit in Signed width");
        }
        value_ = other.value();
    }

    constexpr int64_t value() const noexcept { return value_; }

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
        value_ = sint_wrap(static_cast<uint64_t>(value_) + 1, width());
        return *this;
    }
    constexpr Signed operator++(int) noexcept {
        auto const old = *this;
        ++*this;
        return old;
    }
    constexpr Signed& operator--() noexcept {
        value_ = sint_wrap(static_cast<uint64_t>(value_) - 1, width());
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
        if (amount >= static_cast<int>(int_max_width)) {
            return make_signed<R>(0);
        }
        return make_signed<R>(static_cast<uint64_t>(value_) << amount);
    }
    constexpr Signed operator>>(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        // Arithmetic right shift: shifting by >= width collapses to the sign.
        auto const shift =
            amount >= static_cast<int>(width()) ? static_cast<int>(width()) - 1 : amount;
        return make_signed<R>(static_cast<uint64_t>(value_ >> shift));
    }

    template <Range R2>
    constexpr Signed& operator+=(Signed<R2> rhs) noexcept {
        value_ = sint_wrap(
            static_cast<uint64_t>(value_) + static_cast<uint64_t>(rhs.value()), width()
        );
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator-=(Signed<R2> rhs) noexcept {
        value_ = sint_wrap(
            static_cast<uint64_t>(value_) - static_cast<uint64_t>(rhs.value()), width()
        );
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator*=(Signed<R2> rhs) noexcept {
        value_ = sint_wrap(
            static_cast<uint64_t>(value_) * static_cast<uint64_t>(rhs.value()), width()
        );
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator/=(Signed<R2> rhs) {
        if (rhs.value() == 0) {
            throw std::domain_error("division by zero");
        }
        value_ = sint_wrap(static_cast<uint64_t>(value_ / rhs.value()), width());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator%=(Signed<R2> rhs) {
        if (rhs.value() == 0) {
            throw std::domain_error("modulo by zero");
        }
        value_ = sint_wrap(static_cast<uint64_t>(value_ % rhs.value()), width());
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator&=(Signed<R2> rhs) noexcept {
        value_ = sint_wrap(
            static_cast<uint64_t>(value_) & static_cast<uint64_t>(rhs.value()), width()
        );
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator|=(Signed<R2> rhs) noexcept {
        value_ = sint_wrap(
            static_cast<uint64_t>(value_) | static_cast<uint64_t>(rhs.value()), width()
        );
        return *this;
    }
    template <Range R2>
    constexpr Signed& operator^=(Signed<R2> rhs) noexcept {
        value_ = sint_wrap(
            static_cast<uint64_t>(value_) ^ static_cast<uint64_t>(rhs.value()), width()
        );
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
    constexpr Signed(raw_tag, uint64_t bits) noexcept : value_(sint_wrap(bits, width())) {}

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

// -- DynSigned: runtime-range counterpart -----------------------------------

class DynSigned {
  public:
    using storage_type = int64_t;

    constexpr Range const& range() const noexcept { return range_; }
    constexpr size_t width() const noexcept { return range_.length(); }

    template <Integer T>
    constexpr DynSigned(T v, Range range) : range_(range) {
        detail::check_width(static_cast<unsigned>(range.length()));
        auto const s = static_cast<int64_t>(v);
        if constexpr (std::is_unsigned_v<T>) {
            if (static_cast<uint64_t>(v) > static_cast<uint64_t>(detail::sint_max(width())))
            {
                throw std::out_of_range("value does not fit in DynSigned width");
            }
        } else {
            if (s < detail::sint_min(width()) || s > detail::sint_max(width())) {
                throw std::out_of_range("value does not fit in DynSigned width");
            }
        }
        value_ = s;
    }

    // Length-only sugar: produces a {length-1 DOWNTO 0} range (HDL convention).
    template <Integer T>
    constexpr DynSigned(T v, unsigned length)
        : DynSigned(v, detail::int_downto_range(length)) {}

    constexpr int64_t value() const noexcept { return value_; }

    template <Integer T>
    constexpr T to() const {
        if (value_ < static_cast<int64_t>(std::numeric_limits<T>::min())
            || value_ > static_cast<int64_t>(std::numeric_limits<T>::max()))
        {
            throw std::out_of_range("DynSigned value does not fit in target type");
        }
        return static_cast<T>(value_);
    }

    constexpr DynSigned operator+() const noexcept { return *this; }
    constexpr DynSigned operator-() const noexcept {
        return DynSigned(raw_tag{}, ~static_cast<uint64_t>(value_) + 1, range_);
    }
    constexpr DynSigned operator~() const noexcept {
        return DynSigned(raw_tag{}, ~static_cast<uint64_t>(value_), range_);
    }

    constexpr DynSigned& operator++() noexcept {
        value_ = detail::sint_wrap(static_cast<uint64_t>(value_) + 1, width());
        return *this;
    }
    constexpr DynSigned operator++(int) noexcept {
        auto const old = *this;
        ++*this;
        return old;
    }
    constexpr DynSigned& operator--() noexcept {
        value_ = detail::sint_wrap(static_cast<uint64_t>(value_) - 1, width());
        return *this;
    }
    constexpr DynSigned operator--(int) noexcept {
        auto const old = *this;
        --*this;
        return old;
    }

    constexpr DynSigned operator<<(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        if (amount >= static_cast<int>(detail::int_max_width)) {
            return DynSigned(raw_tag{}, 0, range_);
        }
        return DynSigned(raw_tag{}, static_cast<uint64_t>(value_) << amount, range_);
    }
    constexpr DynSigned operator>>(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        auto const shift =
            amount >= static_cast<int>(width()) ? static_cast<int>(width()) - 1 : amount;
        return DynSigned(raw_tag{}, static_cast<uint64_t>(value_ >> shift), range_);
    }

    constexpr DynSigned& operator+=(DynSigned rhs) noexcept {
        value_ = detail::sint_wrap(
            static_cast<uint64_t>(value_) + static_cast<uint64_t>(rhs.value_), width()
        );
        return *this;
    }
    constexpr DynSigned& operator-=(DynSigned rhs) noexcept {
        value_ = detail::sint_wrap(
            static_cast<uint64_t>(value_) - static_cast<uint64_t>(rhs.value_), width()
        );
        return *this;
    }
    constexpr DynSigned& operator*=(DynSigned rhs) noexcept {
        value_ = detail::sint_wrap(
            static_cast<uint64_t>(value_) * static_cast<uint64_t>(rhs.value_), width()
        );
        return *this;
    }
    constexpr DynSigned& operator/=(DynSigned rhs) {
        if (rhs.value_ == 0) {
            throw std::domain_error("division by zero");
        }
        value_ = detail::sint_wrap(static_cast<uint64_t>(value_ / rhs.value_), width());
        return *this;
    }
    constexpr DynSigned& operator%=(DynSigned rhs) {
        if (rhs.value_ == 0) {
            throw std::domain_error("modulo by zero");
        }
        value_ = detail::sint_wrap(static_cast<uint64_t>(value_ % rhs.value_), width());
        return *this;
    }
    constexpr DynSigned& operator&=(DynSigned rhs) noexcept {
        value_ = detail::sint_wrap(
            static_cast<uint64_t>(value_) & static_cast<uint64_t>(rhs.value_), width()
        );
        return *this;
    }
    constexpr DynSigned& operator|=(DynSigned rhs) noexcept {
        value_ = detail::sint_wrap(
            static_cast<uint64_t>(value_) | static_cast<uint64_t>(rhs.value_), width()
        );
        return *this;
    }
    constexpr DynSigned& operator^=(DynSigned rhs) noexcept {
        value_ = detail::sint_wrap(
            static_cast<uint64_t>(value_) ^ static_cast<uint64_t>(rhs.value_), width()
        );
        return *this;
    }
    constexpr DynSigned& operator<<=(int amount) {
        *this = *this << amount;
        return *this;
    }
    constexpr DynSigned& operator>>=(int amount) {
        *this = *this >> amount;
        return *this;
    }

  private:
    struct raw_tag {};
    constexpr DynSigned(raw_tag, uint64_t bits, Range range) noexcept
        : value_(detail::sint_wrap(bits, static_cast<unsigned>(range.length()))),
          range_(range) {}

    int64_t value_ = 0;
    Range range_ = detail::int_downto_range(1);

    friend constexpr DynSigned operator+(DynSigned, DynSigned) noexcept;
    friend constexpr DynSigned operator-(DynSigned, DynSigned) noexcept;
    friend constexpr DynSigned operator*(DynSigned, DynSigned) noexcept;
    friend constexpr DynSigned operator/(DynSigned, DynSigned);
    friend constexpr DynSigned operator%(DynSigned, DynSigned);
    friend constexpr DynSigned operator&(DynSigned, DynSigned) noexcept;
    friend constexpr DynSigned operator|(DynSigned, DynSigned) noexcept;
    friend constexpr DynSigned operator^(DynSigned, DynSigned) noexcept;
};

inline constexpr DynSigned operator+(DynSigned a, DynSigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynSigned(
        DynSigned::raw_tag{},
        static_cast<uint64_t>(a.value_) + static_cast<uint64_t>(b.value_),
        r
    );
}
inline constexpr DynSigned operator-(DynSigned a, DynSigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynSigned(
        DynSigned::raw_tag{},
        static_cast<uint64_t>(a.value_) - static_cast<uint64_t>(b.value_),
        r
    );
}
inline constexpr DynSigned operator*(DynSigned a, DynSigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynSigned(
        DynSigned::raw_tag{},
        static_cast<uint64_t>(a.value_) * static_cast<uint64_t>(b.value_),
        r
    );
}
inline constexpr DynSigned operator/(DynSigned a, DynSigned b) {
    if (b.value_ == 0) {
        throw std::domain_error("division by zero");
    }
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynSigned(DynSigned::raw_tag{}, static_cast<uint64_t>(a.value_ / b.value_), r);
}
inline constexpr DynSigned operator%(DynSigned a, DynSigned b) {
    if (b.value_ == 0) {
        throw std::domain_error("modulo by zero");
    }
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynSigned(DynSigned::raw_tag{}, static_cast<uint64_t>(a.value_ % b.value_), r);
}
inline constexpr DynSigned operator&(DynSigned a, DynSigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynSigned(
        DynSigned::raw_tag{},
        static_cast<uint64_t>(a.value_) & static_cast<uint64_t>(b.value_),
        r
    );
}
inline constexpr DynSigned operator|(DynSigned a, DynSigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynSigned(
        DynSigned::raw_tag{},
        static_cast<uint64_t>(a.value_) | static_cast<uint64_t>(b.value_),
        r
    );
}
inline constexpr DynSigned operator^(DynSigned a, DynSigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynSigned(
        DynSigned::raw_tag{},
        static_cast<uint64_t>(a.value_) ^ static_cast<uint64_t>(b.value_),
        r
    );
}

inline constexpr bool operator==(DynSigned a, DynSigned b) noexcept {
    return a.value() == b.value();
}
inline constexpr bool operator!=(DynSigned a, DynSigned b) noexcept {
    return a.value() != b.value();
}
inline constexpr bool operator<(DynSigned a, DynSigned b) noexcept {
    return a.value() < b.value();
}
inline constexpr bool operator<=(DynSigned a, DynSigned b) noexcept {
    return a.value() <= b.value();
}
inline constexpr bool operator>(DynSigned a, DynSigned b) noexcept {
    return a.value() > b.value();
}
inline constexpr bool operator>=(DynSigned a, DynSigned b) noexcept {
    return a.value() >= b.value();
}

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

template <>
struct std::formatter<coconext::types::DynSigned> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("DynSigned formatter takes no format spec");
        }
        return it;
    }
    auto format(coconext::types::DynSigned const& v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", v.value());
    }
};

template <coconext::types::Range R>
struct std::hash<coconext::types::detail::Signed<R>> {
    size_t operator()(coconext::types::detail::Signed<R> const& v) const noexcept {
        return std::hash<int64_t>{}(v.value());
    }
};

template <>
struct std::hash<coconext::types::DynSigned> {
    size_t operator()(coconext::types::DynSigned const& v) const noexcept {
        return std::hash<int64_t>{}(v.value());
    }
};

#endif  // COCONEXT_SIGNED_HPP

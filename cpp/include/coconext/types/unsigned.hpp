#ifndef COCONEXT_UNSIGNED_HPP
#define COCONEXT_UNSIGNED_HPP

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
class Unsigned;

// Wrapping factory: build an Unsigned<R> from raw bits, reducing modulo
// 2^R.length(). Used by arithmetic operators (which wrap rather than throw).
template <Range R>
constexpr Unsigned<R> make_unsigned(uint64_t bits) noexcept;

// Fixed-width unsigned integer with two's-complement wrap-on-overflow. The
// indexing range R carries HDL coordinates; only its length (in bits) matters
// for arithmetic. Backed by a single uint64_t, so length is limited to 1..64.
// The stored value is always kept masked to the low N bits.
template <Range R>
class Unsigned {
    static_assert(
        R.length() >= 1 && R.length() <= int_max_width, "Unsigned width must be 1..64"
    );

  public:
    using storage_type = uint64_t;

    static constexpr Range range() noexcept { return R; }
    static constexpr size_t width() noexcept { return R.length(); }

    constexpr Unsigned() noexcept = default;

    // Construct from a native integer. Throws std::out_of_range if the value is
    // negative or does not fit in N bits.
    template <Integer T>
    explicit constexpr Unsigned(T v) {
        if constexpr (std::is_signed_v<T>) {
            if (v < 0) {
                throw std::out_of_range("negative value in Unsigned construction");
            }
        }
        auto const u = static_cast<uint64_t>(v);
        if (u > uint_mask(width())) {
            throw std::out_of_range("value does not fit in Unsigned width");
        }
        value_ = u;
    }

    // Cross-width conversion. Throws if the source value doesn't fit in N bits.
    template <Range R2>
    explicit constexpr Unsigned(Unsigned<R2> other) {
        if (other.value() > uint_mask(width())) {
            throw std::out_of_range("value does not fit in Unsigned width");
        }
        value_ = other.value();
    }

    constexpr uint64_t value() const noexcept { return value_; }

    // Convert to a native integer. Throws std::out_of_range if the value
    // exceeds the target type's range.
    template <Integer T>
    constexpr T to() const {
        if (value_ > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
            throw std::out_of_range("Unsigned value does not fit in target type");
        }
        return static_cast<T>(value_);
    }

    // -- unary ----------------------------------------------------------------
    constexpr Unsigned operator+() const noexcept { return *this; }
    constexpr Unsigned operator-() const noexcept { return make_unsigned<R>(~value_ + 1); }
    constexpr Unsigned operator~() const noexcept { return make_unsigned<R>(~value_); }

    constexpr Unsigned& operator++() noexcept {
        value_ = uint_wrap(value_ + 1, width());
        return *this;
    }
    constexpr Unsigned operator++(int) noexcept {
        auto const old = *this;
        ++*this;
        return old;
    }
    constexpr Unsigned& operator--() noexcept {
        value_ = uint_wrap(value_ - 1, width());
        return *this;
    }
    constexpr Unsigned operator--(int) noexcept {
        auto const old = *this;
        --*this;
        return old;
    }

    // -- shifts (amount is a native integer) ----------------------------------
    constexpr Unsigned operator<<(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        if (amount >= static_cast<int>(int_max_width)) {
            return make_unsigned<R>(0);
        }
        return make_unsigned<R>(value_ << amount);
    }
    constexpr Unsigned operator>>(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        if (amount >= static_cast<int>(int_max_width)) {
            return make_unsigned<R>(0);
        }
        return make_unsigned<R>(value_ >> amount);
    }

    // -- compound assignment (result wrapped to this width) -------------------
    template <Range R2>
    constexpr Unsigned& operator+=(Unsigned<R2> rhs) noexcept {
        value_ = uint_wrap(value_ + rhs.value(), width());
        return *this;
    }
    template <Range R2>
    constexpr Unsigned& operator-=(Unsigned<R2> rhs) noexcept {
        value_ = uint_wrap(value_ - rhs.value(), width());
        return *this;
    }
    template <Range R2>
    constexpr Unsigned& operator*=(Unsigned<R2> rhs) noexcept {
        value_ = uint_wrap(value_ * rhs.value(), width());
        return *this;
    }
    template <Range R2>
    constexpr Unsigned& operator/=(Unsigned<R2> rhs) {
        if (rhs.value() == 0) {
            throw std::domain_error("division by zero");
        }
        value_ = uint_wrap(value_ / rhs.value(), width());
        return *this;
    }
    template <Range R2>
    constexpr Unsigned& operator%=(Unsigned<R2> rhs) {
        if (rhs.value() == 0) {
            throw std::domain_error("modulo by zero");
        }
        value_ = uint_wrap(value_ % rhs.value(), width());
        return *this;
    }
    template <Range R2>
    constexpr Unsigned& operator&=(Unsigned<R2> rhs) noexcept {
        value_ = uint_wrap(value_ & rhs.value(), width());
        return *this;
    }
    template <Range R2>
    constexpr Unsigned& operator|=(Unsigned<R2> rhs) noexcept {
        value_ = uint_wrap(value_ | rhs.value(), width());
        return *this;
    }
    template <Range R2>
    constexpr Unsigned& operator^=(Unsigned<R2> rhs) noexcept {
        value_ = uint_wrap(value_ ^ rhs.value(), width());
        return *this;
    }
    constexpr Unsigned& operator<<=(int amount) {
        *this = *this << amount;
        return *this;
    }
    constexpr Unsigned& operator>>=(int amount) {
        *this = *this >> amount;
        return *this;
    }

  private:
    struct raw_tag {};
    constexpr Unsigned(raw_tag, uint64_t bits) noexcept
        : value_(uint_wrap(bits, width())) {}

    uint64_t value_ = 0;

    friend constexpr Unsigned detail::make_unsigned<R>(uint64_t bits) noexcept;
};

template <Range R>
constexpr Unsigned<R> make_unsigned(uint64_t bits) noexcept {
    return Unsigned<R>(typename Unsigned<R>::raw_tag{}, bits);
}

template <size_t A, size_t B>
inline constexpr size_t max_width = A > B ? A : B;

}  // namespace detail

// User-facing alias: accepts the same NTTP forms as Array<T, ...>, with HDL
// DOWNTO defaulting (see detail::make_int_range for the rules).
template <auto... Args>
using Unsigned = detail::Unsigned<detail::make_int_range<Args...>()>;

namespace detail {

// -- binary arithmetic: result range is {max(width)-1 DOWNTO 0} --------------
// Defined inside detail:: so ADL finds them from detail::Unsigned<R> arguments
// (ADL looks in the namespace of the argument type, not enclosing namespaces).

template <Range A, Range B>
constexpr Unsigned<int_result_range<A, B>> operator+(
    Unsigned<A> a, Unsigned<B> b
) noexcept {
    return make_unsigned<int_result_range<A, B>>(a.value() + b.value());
}
template <Range A, Range B>
constexpr Unsigned<int_result_range<A, B>> operator-(
    Unsigned<A> a, Unsigned<B> b
) noexcept {
    return make_unsigned<int_result_range<A, B>>(a.value() - b.value());
}
template <Range A, Range B>
constexpr Unsigned<int_result_range<A, B>> operator*(
    Unsigned<A> a, Unsigned<B> b
) noexcept {
    return make_unsigned<int_result_range<A, B>>(a.value() * b.value());
}
template <Range A, Range B>
constexpr Unsigned<int_result_range<A, B>> operator/(Unsigned<A> a, Unsigned<B> b) {
    if (b.value() == 0) {
        throw std::domain_error("division by zero");
    }
    return make_unsigned<int_result_range<A, B>>(a.value() / b.value());
}
template <Range A, Range B>
constexpr Unsigned<int_result_range<A, B>> operator%(Unsigned<A> a, Unsigned<B> b) {
    if (b.value() == 0) {
        throw std::domain_error("modulo by zero");
    }
    return make_unsigned<int_result_range<A, B>>(a.value() % b.value());
}

template <Range A, Range B>
constexpr Unsigned<int_result_range<A, B>> operator&(
    Unsigned<A> a, Unsigned<B> b
) noexcept {
    return make_unsigned<int_result_range<A, B>>(a.value() & b.value());
}
template <Range A, Range B>
constexpr Unsigned<int_result_range<A, B>> operator|(
    Unsigned<A> a, Unsigned<B> b
) noexcept {
    return make_unsigned<int_result_range<A, B>>(a.value() | b.value());
}
template <Range A, Range B>
constexpr Unsigned<int_result_range<A, B>> operator^(
    Unsigned<A> a, Unsigned<B> b
) noexcept {
    return make_unsigned<int_result_range<A, B>>(a.value() ^ b.value());
}

template <Range A, Range B>
constexpr bool operator==(Unsigned<A> a, Unsigned<B> b) noexcept {
    return a.value() == b.value();
}
template <Range A, Range B>
constexpr bool operator!=(Unsigned<A> a, Unsigned<B> b) noexcept {
    return a.value() != b.value();
}
template <Range A, Range B>
constexpr bool operator<(Unsigned<A> a, Unsigned<B> b) noexcept {
    return a.value() < b.value();
}
template <Range A, Range B>
constexpr bool operator<=(Unsigned<A> a, Unsigned<B> b) noexcept {
    return a.value() <= b.value();
}
template <Range A, Range B>
constexpr bool operator>(Unsigned<A> a, Unsigned<B> b) noexcept {
    return a.value() > b.value();
}
template <Range A, Range B>
constexpr bool operator>=(Unsigned<A> a, Unsigned<B> b) noexcept {
    return a.value() >= b.value();
}

}  // namespace detail

// -- DynUnsigned: runtime-range counterpart ---------------------------------
//
// Same uint64_t storage and modular semantics as Unsigned<R>, but the indexing
// Range is a runtime value. Binary ops produce a result of range {N-1 DOWNTO
// 0} where N = max(width(a), width(b)). Mixing a DynUnsigned with a static
// Unsigned<R> is out of scope for now -- convert explicitly via value()/to<>()
// at the boundary.

class DynUnsigned {
  public:
    using storage_type = uint64_t;

    constexpr Range const& range() const noexcept { return range_; }
    constexpr size_t width() const noexcept { return range_.length(); }

    // Construct from a value plus an explicit Range.
    template <Integer T>
    constexpr DynUnsigned(T v, Range range) : range_(range) {
        detail::check_width(static_cast<unsigned>(range.length()));
        if constexpr (std::is_signed_v<T>) {
            if (v < 0) {
                throw std::out_of_range("negative value in DynUnsigned construction");
            }
        }
        auto const u = static_cast<uint64_t>(v);
        if (u > detail::uint_mask(width())) {
            throw std::out_of_range("value does not fit in DynUnsigned width");
        }
        value_ = u;
    }

    // Length-only sugar: produces a {length-1 DOWNTO 0} range (HDL convention).
    template <Integer T>
    constexpr DynUnsigned(T v, unsigned length)
        : DynUnsigned(v, detail::int_downto_range(length)) {}

    constexpr uint64_t value() const noexcept { return value_; }

    template <Integer T>
    constexpr T to() const {
        if (value_ > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
            throw std::out_of_range("DynUnsigned value does not fit in target type");
        }
        return static_cast<T>(value_);
    }

    constexpr DynUnsigned operator+() const noexcept { return *this; }
    constexpr DynUnsigned operator-() const noexcept {
        return DynUnsigned(raw_tag{}, ~value_ + 1, range_);
    }
    constexpr DynUnsigned operator~() const noexcept {
        return DynUnsigned(raw_tag{}, ~value_, range_);
    }

    constexpr DynUnsigned& operator++() noexcept {
        value_ = detail::uint_wrap(value_ + 1, width());
        return *this;
    }
    constexpr DynUnsigned operator++(int) noexcept {
        auto const old = *this;
        ++*this;
        return old;
    }
    constexpr DynUnsigned& operator--() noexcept {
        value_ = detail::uint_wrap(value_ - 1, width());
        return *this;
    }
    constexpr DynUnsigned operator--(int) noexcept {
        auto const old = *this;
        --*this;
        return old;
    }

    constexpr DynUnsigned operator<<(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        if (amount >= static_cast<int>(detail::int_max_width)) {
            return DynUnsigned(raw_tag{}, 0, range_);
        }
        return DynUnsigned(raw_tag{}, value_ << amount, range_);
    }
    constexpr DynUnsigned operator>>(int amount) const {
        if (amount < 0) {
            throw std::invalid_argument("negative shift amount");
        }
        if (amount >= static_cast<int>(detail::int_max_width)) {
            return DynUnsigned(raw_tag{}, 0, range_);
        }
        return DynUnsigned(raw_tag{}, value_ >> amount, range_);
    }

    constexpr DynUnsigned& operator+=(DynUnsigned rhs) noexcept {
        value_ = detail::uint_wrap(value_ + rhs.value_, width());
        return *this;
    }
    constexpr DynUnsigned& operator-=(DynUnsigned rhs) noexcept {
        value_ = detail::uint_wrap(value_ - rhs.value_, width());
        return *this;
    }
    constexpr DynUnsigned& operator*=(DynUnsigned rhs) noexcept {
        value_ = detail::uint_wrap(value_ * rhs.value_, width());
        return *this;
    }
    constexpr DynUnsigned& operator/=(DynUnsigned rhs) {
        if (rhs.value_ == 0) {
            throw std::domain_error("division by zero");
        }
        value_ = detail::uint_wrap(value_ / rhs.value_, width());
        return *this;
    }
    constexpr DynUnsigned& operator%=(DynUnsigned rhs) {
        if (rhs.value_ == 0) {
            throw std::domain_error("modulo by zero");
        }
        value_ = detail::uint_wrap(value_ % rhs.value_, width());
        return *this;
    }
    constexpr DynUnsigned& operator&=(DynUnsigned rhs) noexcept {
        value_ = detail::uint_wrap(value_ & rhs.value_, width());
        return *this;
    }
    constexpr DynUnsigned& operator|=(DynUnsigned rhs) noexcept {
        value_ = detail::uint_wrap(value_ | rhs.value_, width());
        return *this;
    }
    constexpr DynUnsigned& operator^=(DynUnsigned rhs) noexcept {
        value_ = detail::uint_wrap(value_ ^ rhs.value_, width());
        return *this;
    }
    constexpr DynUnsigned& operator<<=(int amount) {
        *this = *this << amount;
        return *this;
    }
    constexpr DynUnsigned& operator>>=(int amount) {
        *this = *this >> amount;
        return *this;
    }

  private:
    struct raw_tag {};
    constexpr DynUnsigned(raw_tag, uint64_t bits, Range range) noexcept
        : value_(detail::uint_wrap(bits, static_cast<unsigned>(range.length()))),
          range_(range) {}

    uint64_t value_ = 0;
    Range range_ = detail::int_downto_range(1);

    friend constexpr DynUnsigned operator+(DynUnsigned, DynUnsigned) noexcept;
    friend constexpr DynUnsigned operator-(DynUnsigned, DynUnsigned) noexcept;
    friend constexpr DynUnsigned operator*(DynUnsigned, DynUnsigned) noexcept;
    friend constexpr DynUnsigned operator/(DynUnsigned, DynUnsigned);
    friend constexpr DynUnsigned operator%(DynUnsigned, DynUnsigned);
    friend constexpr DynUnsigned operator&(DynUnsigned, DynUnsigned) noexcept;
    friend constexpr DynUnsigned operator|(DynUnsigned, DynUnsigned) noexcept;
    friend constexpr DynUnsigned operator^(DynUnsigned, DynUnsigned) noexcept;
};

inline constexpr DynUnsigned operator+(DynUnsigned a, DynUnsigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynUnsigned(DynUnsigned::raw_tag{}, a.value_ + b.value_, r);
}
inline constexpr DynUnsigned operator-(DynUnsigned a, DynUnsigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynUnsigned(DynUnsigned::raw_tag{}, a.value_ - b.value_, r);
}
inline constexpr DynUnsigned operator*(DynUnsigned a, DynUnsigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynUnsigned(DynUnsigned::raw_tag{}, a.value_ * b.value_, r);
}
inline constexpr DynUnsigned operator/(DynUnsigned a, DynUnsigned b) {
    if (b.value_ == 0) {
        throw std::domain_error("division by zero");
    }
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynUnsigned(DynUnsigned::raw_tag{}, a.value_ / b.value_, r);
}
inline constexpr DynUnsigned operator%(DynUnsigned a, DynUnsigned b) {
    if (b.value_ == 0) {
        throw std::domain_error("modulo by zero");
    }
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynUnsigned(DynUnsigned::raw_tag{}, a.value_ % b.value_, r);
}
inline constexpr DynUnsigned operator&(DynUnsigned a, DynUnsigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynUnsigned(DynUnsigned::raw_tag{}, a.value_ & b.value_, r);
}
inline constexpr DynUnsigned operator|(DynUnsigned a, DynUnsigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynUnsigned(DynUnsigned::raw_tag{}, a.value_ | b.value_, r);
}
inline constexpr DynUnsigned operator^(DynUnsigned a, DynUnsigned b) noexcept {
    auto const r = detail::int_downto_range(std::max(a.width(), b.width()));
    return DynUnsigned(DynUnsigned::raw_tag{}, a.value_ ^ b.value_, r);
}

inline constexpr bool operator==(DynUnsigned a, DynUnsigned b) noexcept {
    return a.value() == b.value();
}
inline constexpr bool operator!=(DynUnsigned a, DynUnsigned b) noexcept {
    return a.value() != b.value();
}
inline constexpr bool operator<(DynUnsigned a, DynUnsigned b) noexcept {
    return a.value() < b.value();
}
inline constexpr bool operator<=(DynUnsigned a, DynUnsigned b) noexcept {
    return a.value() <= b.value();
}
inline constexpr bool operator>(DynUnsigned a, DynUnsigned b) noexcept {
    return a.value() > b.value();
}
inline constexpr bool operator>=(DynUnsigned a, DynUnsigned b) noexcept {
    return a.value() >= b.value();
}

}  // namespace coconext::types

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Unsigned<R>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Unsigned formatter takes no format spec");
        }
        return it;
    }
    auto format(
        coconext::types::detail::Unsigned<R> const& v, std::format_context& ctx
    ) const {
        return std::format_to(ctx.out(), "{}", v.value());
    }
};

template <>
struct std::formatter<coconext::types::DynUnsigned> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("DynUnsigned formatter takes no format spec");
        }
        return it;
    }
    auto format(coconext::types::DynUnsigned const& v, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", v.value());
    }
};

template <coconext::types::Range R>
struct std::hash<coconext::types::detail::Unsigned<R>> {
    size_t operator()(coconext::types::detail::Unsigned<R> const& v) const noexcept {
        return std::hash<uint64_t>{}(v.value());
    }
};

template <>
struct std::hash<coconext::types::DynUnsigned> {
    size_t operator()(coconext::types::DynUnsigned const& v) const noexcept {
        return std::hash<uint64_t>{}(v.value());
    }
};

#endif  // COCONEXT_UNSIGNED_HPP

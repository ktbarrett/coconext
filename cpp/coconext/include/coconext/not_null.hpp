#ifndef COCONEXT_NOT_NULL_HPP
#define COCONEXT_NOT_NULL_HPP

#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>

namespace coconext {

// A non-owning pointer wrapper that statically communicates non-nullability.
//
// Template argument is the pointer type itself (e.g. not_null<Foo*>), matching
// gsl::not_null. Construction from a raw pointer asserts non-null; once a
// not_null exists, downstream code taking not_null skips redundant null checks.
// Constructor checks are the only cost; copies, comparisons, and dereferences
// are free.
template <typename P>
class not_null {
    static_assert(std::is_pointer_v<P>, "not_null<P> requires P to be a pointer type");

    using element_type = std::remove_pointer_t<P>;

  public:
    // No default-construct: there is no valid non-null default.
    not_null() = delete;
    // No construction from nullptr: caught at compile time.
    not_null(std::nullptr_t) = delete;
    not_null& operator=(std::nullptr_t) = delete;

    // Primary constructor. Asserts non-null; UB in release builds if violated.
    constexpr not_null(P p) noexcept : p_(p) { assert(p != nullptr); }

    // Converting construction from a compatible not_null (e.g. Derived* -> Base*).
    template <typename Q>
        requires(std::is_convertible_v<Q, P> && !std::is_same_v<Q, P>)
    constexpr not_null(not_null<Q> other) noexcept : p_(other.get()) {}

    constexpr not_null(not_null const&) noexcept = default;
    constexpr not_null& operator=(not_null const&) noexcept = default;

    // Access.
    [[nodiscard]] constexpr P get() const noexcept { return p_; }
    [[nodiscard]] constexpr element_type& operator*() const noexcept { return *p_; }
    [[nodiscard]] constexpr P operator->() const noexcept { return p_; }

    // Implicit conversion to the underlying pointer so not_null flows through
    // APIs that still take raw pointers. Callee-side annotations tighten over
    // time without forcing a big-bang migration.
    [[nodiscard]] constexpr operator P() const noexcept { return p_; }

    // Comparisons and hashing key off the underlying pointer, so a not_null
    // is interchangeable with a raw pointer in debug tables / logs.
    [[nodiscard]] friend constexpr bool operator==(not_null, not_null) noexcept = default;
    [[nodiscard]] friend constexpr auto operator<=>(not_null, not_null) noexcept = default;

    // Heterogeneous comparisons against raw pointers. Without these, `nn == p`
    // is ambiguous: the compiler can either convert p to not_null (via the
    // primary ctor) or convert nn to P (via operator P()). C++20 rewritten
    // operators give us the reversed form (p == nn, p <=> nn) for free.
    [[nodiscard]] friend constexpr bool operator==(not_null lhs, P rhs) noexcept {
        return lhs.p_ == rhs;
    }
    [[nodiscard]] friend constexpr auto operator<=>(not_null lhs, P rhs) noexcept {
        return lhs.p_ <=> rhs;
    }

    // Comparisons against nullptr are always false / meaningful only in one
    // direction, so ban them at compile time to catch dead checks.
    friend bool operator==(not_null, std::nullptr_t) = delete;
    friend bool operator==(std::nullptr_t, not_null) = delete;

  private:
    P p_;
};

// Deduction guide: `not_null(p)` deduces `not_null<P>` from a pointer `p`.
template <typename P>
    requires std::is_pointer_v<P>
not_null(P) -> not_null<P>;

}  // namespace coconext

template <typename P>
struct std::hash<coconext::not_null<P>> {
    [[nodiscard]] size_t operator()(coconext::not_null<P> p) const noexcept {
        return std::hash<P>{}(p.get());
    }
};

#endif  // COCONEXT_NOT_NULL_HPP

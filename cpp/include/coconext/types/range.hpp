#ifndef COCONEXT_RANGE_HPP
#define COCONEXT_RANGE_HPP

#include <cassert>
#include <coconext/types/concepts.hpp>
#include <coconext/types/count_iterator.hpp>
#include <coconext/types/direction.hpp>
#include <coconext/types/hash.hpp>
#include <cstdint>
#include <format>
#include <limits>
#include <ranges>
#include <stdexcept>

namespace coconext::types {

struct Range {
    using value_type = int64_t;
    using iterator = CountIterator<value_type>;

    // length() requires size_t to be at least as wide as value_type. This
    // implies two things at once:
    //   (a) `static_cast<size_t>(value_type_negative)` is well-defined
    //       modular reduction over 2^width(size_t); combined with C++20's
    //       mandated two's complement representation, the resulting bit
    //       pattern (or its sign-extended form for wider size_t) makes
    //       the unsigned subtraction `static_cast<size_t>(right) -
    //       static_cast<size_t>(left)` produce the correct mathematical
    //       difference when endpoints are in the expected order.
    //   (b) the +1 on a maximum-valid difference (one short of the full
    //       value_type domain) fits in size_t without wrapping.
    static_assert(
        sizeof(size_t) >= sizeof(value_type),
        "Range::length() requires size_t to be at least as wide as value_type"
    );

    constexpr Range() noexcept = default;

    constexpr Range(value_type l, Direction d, value_type r) noexcept
        : left(l), right(r), direction(d) {}

    constexpr Range(value_type l, value_type r) noexcept
        : left(l), right(r), direction(l > r ? Direction::DOWNTO : Direction::TO) {}

    explicit constexpr Range(size_t length)
        : left(0), right(static_cast<value_type>(length) - 1), direction(Direction::TO) {
        if (length > static_cast<size_t>(std::numeric_limits<value_type>::max())) {
            throw std::length_error("Range length overflows value_type");
        }
    }

    value_type left = 0;
    value_type right = -1;
    Direction direction = Direction::TO;

    // length() is the only safe place to enforce the size_t-overflow check:
    // the fields are public, so a Range can be mutated into the full-domain
    // span after construction.
    //
    // Throws std::length_error if the range spans the full value_type domain
    // (mathematical length 2^width(value_type), doesn't fit in size_t).
    //
    // The arithmetic is in size_t. See the static_assert above for why direct
    // signed-to-size_t casts give us the correct unsigned difference. Without
    // the full-span check below, the +1 on a SIZE_MAX-1 difference would wrap
    // silently to 0.
    constexpr size_t length() const {
        constexpr value_type lo = std::numeric_limits<value_type>::min();
        constexpr value_type hi = std::numeric_limits<value_type>::max();
        bool const full_to = direction == Direction::TO && left == lo && right == hi;
        bool const full_downto =
            direction == Direction::DOWNTO && left == hi && right == lo;
        if (full_to || full_downto) {
            throw std::length_error(
                "Range spans the full value_type domain; length overflows size_t"
            );
        }
        if (direction == Direction::TO) {
            if (right < left) {
                return 0;
            }
            return static_cast<size_t>(right) - static_cast<size_t>(left) + 1;
        }
        if (left < right) {
            return 0;
        }
        return static_cast<size_t>(left) - static_cast<size_t>(right) + 1;
    }

    constexpr iterator begin() const noexcept { return iterator(left, direction); }

    constexpr iterator end() const noexcept {
        auto const len = static_cast<value_type>(length());
        auto const end_value = direction == Direction::TO ? left + len : left - len;
        return iterator(end_value, direction);
    }

    constexpr iterator rbegin() const noexcept {
        return iterator(
            right, direction == Direction::TO ? Direction::DOWNTO : Direction::TO
        );
    }

    constexpr iterator rend() const noexcept {
        auto const len = static_cast<value_type>(length());
        auto const rend_value = direction == Direction::TO ? right - len : right + len;
        return iterator(
            rend_value, direction == Direction::TO ? Direction::DOWNTO : Direction::TO
        );
    }

    constexpr value_type operator[](size_t index) const {
        if (index >= length()) {
            throw std::out_of_range("Index out of range");
        }
        if (direction == Direction::TO) {
            return left + index;
        }
        return left - index;
    }

    // This range is a valid subsequence of `parent` iff every value in this
    // range appears (in order) in parent. The rule collapses to:
    //   - length 0:   always a subsequence (any direction, any bounds)
    //   - length 1:   the single value must exist in parent; this range's
    //                 direction is irrelevant (a one-element ordering is the
    //                 same either way)
    //   - length 2+:  this range's direction must match parent's (otherwise
    //                 the values appear in parent but in reversed order,
    //                 which isn't a subsequence) and both endpoints must
    //                 exist in parent
    //
    // constexpr-evaluable, so callers can static_assert it at compile time as
    // well as use it as a runtime predicate.
    constexpr bool is_subsequence_of(Range parent) const noexcept {
        auto const in_parent = [&](value_type v) {
            if (parent.direction == Direction::TO) {
                return v >= parent.left && v <= parent.right;
            }
            return v <= parent.left && v >= parent.right;
        };
        auto const len = length();
        if (len == 0) {
            return true;
        }
        if (!in_parent(left)) {
            return false;
        }
        if (len == 1) {
            return true;
        }
        if (direction != parent.direction) {
            return false;
        }
        return in_parent(right);
    }

    friend constexpr bool operator==(Range const& lhs, Range const& rhs) noexcept {
        auto left_len = lhs.length();
        if (left_len != rhs.length()) {
            return false;
        }
        if (left_len == 0) {
            // lengths are equal and all zero-length ranges are equal
            return true;
        }
        if (lhs.left != rhs.left) {
            // lengths are equal and >= 1, so if left endpoints differ the
            // ranges cannot be equal
            return false;
        }
        // If left endpoints are equal and lengths are equal, then right
        // endpoint and direction must be the same for length >= 2 cases. If
        // length is 1, then the direction and right endpoint do not matter,
        // since the range only contains one value and that's the left endpoint
        // we already checked.
        return true;
    }
};

constexpr Range reverse(Range const& r) noexcept {
    return Range{
        r.right, r.direction == Direction::TO ? Direction::DOWNTO : Direction::TO, r.left
    };
}

// more optimal implementation of std::ranges::find for Range
constexpr Range::iterator find(Range const& range, Range::value_type value) {
    if (range.direction == Direction::TO) {
        if (value < range.left || value > range.right) {
            return range.end();
        }
        return range.begin() + (value - range.left);
    } else {
        if (value > range.left || value < range.right) {
            return range.end();
        }
        return range.begin() + (range.left - value);
    }
}

static_assert(std::ranges::random_access_range<Range>);

}  // namespace coconext::types

template <>
struct std::formatter<coconext::types::Range> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Range formatter takes no format spec");
        }
        return it;
    }

    auto format(coconext::types::Range const& r, std::format_context& ctx) const {
        return std::format_to(
            ctx.out(),
            "[{} {} {}]",
            r.left,
            coconext::types::to_string(r.direction),
            r.right
        );
    }
};

template <>
struct std::hash<coconext::types::Range> {
    size_t operator()(coconext::types::Range const& range) const noexcept {
        // if a == a then hash(a) == hash(a), so we have to force all 0 and 1
        // length ranges to have repeatable hashes.
        auto const len = range.length();
        if (len == 0) {
            return hash<size_t>{}(0);
        } else if (len == 1) {
            return hash<coconext::types::Range::value_type>{}(range.left);
        } else {
            return coconext::types::detail::hash_combine(
                range.left, range.right, range.direction
            );
        }
    }
};

#endif  // COCONEXT_RANGE_HPP

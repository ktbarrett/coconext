#ifndef COCONEXT_RANGE_HPP
#define COCONEXT_RANGE_HPP

#include <cassert>
#include <coconext/types/concepts.hpp>
#include <coconext/types/count_iterator.hpp>
#include <coconext/types/direction.hpp>
#include <coconext/types/hash.hpp>
#include <cstdint>
#include <ranges>
#include <stdexcept>

namespace coconext::types {

class Range {
public:
    using value_type = int32_t;
    using iterator = CountIterator<value_type>;

public:
    constexpr Range() noexcept = default;
    explicit constexpr Range(value_type left, Direction direction,
                             value_type right) noexcept
        : left_(left), right_(right), direction_(direction) {}
    explicit constexpr Range(value_type left, value_type right) noexcept
        : left_(left),
          right_(right),
          direction_(left >= right ? Direction::DOWNTO : Direction::TO) {}

public:
    constexpr value_type left() const noexcept { return left_; }
    constexpr value_type right() const noexcept { return right_; }
    constexpr Direction direction() const noexcept { return direction_; }
    constexpr size_t length() const noexcept {
        int64_t len = direction_ == Direction::TO
                          ? static_cast<int64_t>(right_) - left_ + 1
                          : static_cast<int64_t>(left_) - right_ + 1;
        if (len < 0) {
            return 0;
        }
        return static_cast<size_t>(len);
    }

public:
    constexpr iterator begin() const noexcept {
        return iterator(left_, direction_);
    }
    constexpr iterator end() const noexcept {
        const auto len = static_cast<value_type>(length());
        const auto end_value =
            direction_ == Direction::TO ? left_ + len : left_ - len;
        return iterator(end_value, direction_);
    }
    constexpr iterator rbegin() const noexcept {
        return iterator(right_, reversed_());
    }
    constexpr iterator rend() const noexcept {
        const auto len = static_cast<value_type>(length());
        const auto rend_value =
            direction_ == Direction::TO ? right_ - len : right_ + len;
        return iterator(rend_value, reversed_());
    }

public:
    constexpr value_type operator[](size_t index) const {
        if (index >= length()) {
            throw std::out_of_range("Index out of range");
        }
        if (direction_ == Direction::TO) {
            return left_ + index;
        }
        return left_ - index;
    }

    constexpr Range operator()(size_t start, size_t stop) const {
        if (stop > length()) {
            throw std::out_of_range("Stop index out of range");
        }
        if (start > stop) {
            throw std::invalid_argument(
                "Start index must be less than or equal to stop index");
        }
        const auto from_start = static_cast<int64_t>(start);
        const auto from_last = static_cast<int64_t>(stop) - 1;
        const auto new_left = static_cast<value_type>(
            direction_ == Direction::TO ? left_ + from_start
                                        : left_ - from_start);
        const auto new_right = static_cast<value_type>(
            direction_ == Direction::TO ? left_ + from_last
                                        : left_ - from_last);
        return Range(new_left, direction_, new_right);
    }

#if __cplusplus >= 202302L
    constexpr Range operator[](size_t start, size_t stop) const {
        return this->operator()(start, stop);
    }
#endif

    friend constexpr bool operator==(const Range& lhs,
                                     const Range& rhs) noexcept;

private:
    constexpr Direction reversed_() const noexcept {
        return direction_ == Direction::TO ? Direction::DOWNTO : Direction::TO;
    }

    value_type left_ = 0;
    value_type right_ = -1;
    Direction direction_ = Direction::TO;
};

constexpr bool operator==(const Range& lhs, const Range& rhs) noexcept {
    if ((lhs.length() == 0) && (rhs.length() == 0)) {
        return true;
    }
    return lhs.left_ == rhs.left_ && lhs.right_ == rhs.right_ &&
           lhs.direction_ == rhs.direction_;
}

// more optimal implementation of std::ranges::find for Range
constexpr Range::iterator find(const Range& range, Range::value_type value) {
    if (range.direction() == Direction::TO) {
        if (value < range.left() || value > range.right()) {
            return range.end();
        }
        return range.begin() + (value - range.left());
    } else {
        if (value > range.left() || value < range.right()) {
            return range.end();
        }
        return range.begin() + (range.left() - value);
    }
}

inline std::string to_string(const Range& range) {
    auto res = std::string("Range(");
    res += std::to_string(range.left());
    res += ", '";
    res += to_string(range.direction());
    res += "', ";
    res += std::to_string(range.right());
    res += ")";
    return res;
}

static_assert(std::ranges::random_access_range<Range>);

}  // namespace coconext::types

namespace std {

template <>
struct hash<coconext::types::Range> {
    size_t operator()(const coconext::types::Range& range) const noexcept {
        return coconext::types::detail::hash_combine(
            range.left(), range.right(), range.direction());
    }
};

}  // namespace std

#endif  // COCONEXT_RANGE_HPP

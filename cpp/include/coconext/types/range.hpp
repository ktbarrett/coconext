#ifndef COCONEXT_RANGE_HPP
#define COCONEXT_RANGE_HPP

#include <algorithm>
#include <coconext/types/concepts.hpp>
#include <coconext/types/direction.hpp>
#include <cstdint>
#include <ranges>
#include <stdexcept>

namespace coconext::types {

class Range {
public:
    using value_type = int32_t;

    class iterator {
    public:
        using difference_type = int32_t;
        using value_type = int32_t;

    public:
        constexpr iterator() noexcept = default;
        constexpr iterator(const iterator&) noexcept = default;
        constexpr iterator& operator=(const iterator&) noexcept = default;
        constexpr iterator(iterator&&) noexcept = default;
        constexpr iterator& operator=(iterator&&) noexcept = default;
        constexpr iterator(value_type current, Direction direction) noexcept
            : current_(current), direction_(direction) {}

    public:
        constexpr value_type operator*() const noexcept { return current_; }
        constexpr iterator& operator++() noexcept {
            if (direction_ == Direction::TO) {
                ++current_;
            } else {
                --current_;
            }
            return *this;
        }
        constexpr iterator operator++(int) noexcept {
            iterator temp = *this;
            ++(*this);
            return temp;
        }
        constexpr iterator& operator--() noexcept {
            if (direction_ == Direction::TO) {
                --current_;
            } else {
                ++current_;
            }
            return *this;
        }
        constexpr iterator operator--(int) noexcept {
            iterator temp = *this;
            --(*this);
            return temp;
        }
        constexpr iterator operator+(difference_type n) const noexcept {
            if (direction_ == Direction::TO) {
                return iterator(current_ + n, direction_);
            } else {
                return iterator(current_ - n, direction_);
            }
        }
        constexpr iterator operator-(difference_type n) const noexcept {
            if (direction_ == Direction::TO) {
                return iterator(current_ - n, direction_);
            } else {
                return iterator(current_ + n, direction_);
            }
        }
        constexpr difference_type operator-(
            const iterator& other) const noexcept {
            if (direction_ == Direction::TO) {
                return current_ - other.current_;
            } else {
                return other.current_ - current_;
            }
        }
        constexpr iterator& operator+=(difference_type n) noexcept {
            if (direction_ == Direction::TO) {
                current_ += n;
            } else {
                current_ -= n;
            }
            return *this;
        }
        constexpr iterator& operator-=(difference_type n) noexcept {
            if (direction_ == Direction::TO) {
                current_ -= n;
            } else {
                current_ += n;
            }
            return *this;
        }
        constexpr value_type operator[](difference_type n) const noexcept {
            return *(*this + n);
        }
        friend constexpr iterator operator+(difference_type n,
                                            const iterator& it) noexcept;
        friend constexpr bool operator<(const iterator& lhs,
                                        const iterator& rhs) noexcept;

    private:
        value_type current_;
        Direction direction_;
    };

public:
    // not default constructible
    constexpr Range() noexcept = delete;

    // ensures that these are constexpr since enum classes are not literal types
    constexpr Range(const Range&) noexcept = default;
    constexpr Range& operator=(const Range&) noexcept = default;
    constexpr Range(Range&&) noexcept = default;
    constexpr Range& operator=(Range&&) noexcept = default;

    explicit constexpr Range(value_type left, Direction direction,
                             value_type right) noexcept
        : left_(left), right_(right), direction_(direction) {}
    explicit constexpr Range(value_type left, value_type right) noexcept
        : left_(left),
          right_(right),
          direction_(left >= right ? Direction::DOWNTO : Direction::TO) {}
    template <typename T>
        requires requires { to_range(std::declval<T>()); }
    explicit constexpr Range(T&& value)
        : Range(to_range(std::forward<T>(value))) {}

public:
    constexpr value_type left() const noexcept { return left_; }
    constexpr value_type right() const noexcept { return right_; }
    constexpr Direction direction() const noexcept { return direction_; }
    constexpr size_t length() const noexcept {
        return std::max(0, direction_ == Direction::TO ? right_ - left_ + 1
                                                       : left_ - right_ + 1);
    }

public:
    constexpr auto begin() const noexcept {
        return iterator(left_, direction_);
    };
    constexpr auto end() const noexcept {
        return iterator(
            direction_ == Direction::TO ? left_ + length() : left_ - length(),
            direction_);
    };
    constexpr auto rbegin() const noexcept {
        return iterator(right_, direction_ == Direction::TO ? Direction::DOWNTO
                                                            : Direction::TO);
    };
    constexpr auto rend() const noexcept {
        return iterator(
            direction_ == Direction::TO ? right_ - length() : right_ + length(),
            direction_ == Direction::TO ? Direction::DOWNTO : Direction::TO);
    };

public:
    constexpr value_type operator[](size_t index) const {
        if (index >= static_cast<size_t>(length())) {
            throw std::out_of_range("Index out of range");
        } else if (direction_ == Direction::TO) {
            if (index >= length()) {
                throw std::out_of_range("Index out of range");
            }
            return left_ + index;
        } else {
            if (index >= length()) {
                throw std::out_of_range("Index out of range");
            }
            return left_ - index;
        }
    }

    constexpr Range operator()(size_t start, size_t stop) const {
        if (start > stop) {
            throw std::invalid_argument(
                "Start index must be less than or equal to stop index");
        }
        auto new_left = this->operator[](start);
        auto new_right = this->operator[](stop - 1);
        return Range(new_left, direction_, new_right);
    }

#if __cplusplus >= 202302L
    constexpr Range operator[](size_t start, size_t stop) const {
        return this->operator()(start, stop);
    }
#endif

    friend constexpr Range::iterator find(const Range& range,
                                          Range::value_type value);

    friend constexpr bool operator==(const Range& lhs,
                                     const Range& rhs) noexcept;

    friend class std::hash<coconext::types::Range>;

private:
    value_type left_;
    value_type right_;
    Direction direction_;
};

constexpr bool operator==(const Range::iterator& lhs,
                          const Range::iterator& rhs) noexcept {
    return *lhs == *rhs;
}

constexpr bool operator!=(const Range::iterator& lhs,
                          const Range::iterator& rhs) noexcept {
    return !(lhs == rhs);
}

constexpr bool operator<(const Range::iterator& lhs,
                         const Range::iterator& rhs) noexcept {
    // We are assuming that the iterators are from the same range, so we don't
    // need to check if the directions are the same. If they are from different
    // Ranges, the behavior is undefined.
    if (lhs.direction_ == Direction::TO) {
        return *lhs < *rhs;
    } else {
        return *lhs > *rhs;
    }
}

constexpr bool operator<=(const Range::iterator& lhs,
                          const Range::iterator& rhs) noexcept {
    return (lhs < rhs) || (lhs == rhs);
}

constexpr bool operator>(const Range::iterator& lhs,
                         const Range::iterator& rhs) noexcept {
    return !(lhs <= rhs);
}

constexpr bool operator>=(const Range::iterator& lhs,
                          const Range::iterator& rhs) noexcept {
    return !(lhs < rhs);
}

constexpr Range::iterator operator+(Range::iterator::difference_type n,
                                    const Range::iterator& it) noexcept {
    return it + n;
}

constexpr bool operator==(const Range& lhs, const Range& rhs) noexcept {
    if ((lhs.length() == 0) && (rhs.length() == 0)) {
        return true;
    }
    return lhs.left_ == rhs.left_ && lhs.right_ == rhs.right_ &&
           lhs.direction_ == rhs.direction_;
}

// more optimal implementation of std::ranges::find for Range
constexpr Range::iterator find(const Range& range, Range::value_type value) {
    if (range.direction_ == Direction::TO) {
        if (value < range.left_ || value > range.right_) {
            return range.end();
        }
    } else {
        if (value > range.left_ || value < range.right_) {
            return range.end();
        }
    }
    return Range::iterator(value, range.direction_);
}

static_assert(std::ranges::random_access_range<Range>);

}  // namespace coconext::types

namespace std {

template <>
class hash<coconext::types::Range> {
public:
    size_t operator()(const coconext::types::Range& range) const noexcept {
        return std::hash<coconext::types::Range::value_type>()(range.left_) ^
               std::hash<coconext::types::Range::value_type>()(range.right_) ^
               std::hash<coconext::types::Direction>()(range.direction_);
    }
};

}  // namespace std

#endif  // COCONEXT_RANGE_HPP

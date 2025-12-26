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
        constexpr iterator(value_type current, Direction direction) noexcept
            : current_(current), direction_(direction) {}
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

    private:
        value_type current_;
        Direction direction_;
    };

    using const_iterator = iterator;
    using reverse_iterator = iterator;
    using const_reverse_iterator = iterator;

public:
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
        return std::max(0, direction_ == Direction::TO ? right_ - left_ + 1
                                                       : left_ - right_ + 1);
    }

public:
    constexpr iterator begin() const noexcept {
        return iterator(left_, direction_);
    };
    constexpr iterator end() const noexcept {
        return iterator(
            direction_ == Direction::TO ? left_ + length() : left_ - length(),
            direction_);
    };
    constexpr const_iterator cbegin() const noexcept { return begin(); }
    constexpr const_iterator cend() const noexcept { return end(); }
    constexpr reverse_iterator rbegin() const noexcept {
        return iterator(right_, direction_ == Direction::TO ? Direction::DOWNTO
                                                            : Direction::TO);
    };
    constexpr reverse_iterator rend() const noexcept {
        return iterator(
            direction_ == Direction::TO ? right_ - length() : right_ + length(),
            direction_ == Direction::TO ? Direction::DOWNTO : Direction::TO);
    };
    constexpr const_reverse_iterator crbegin() const noexcept {
        return rbegin();
    }
    constexpr const_reverse_iterator crend() const noexcept { return rend(); }

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

    constexpr size_t indexof(value_type value) const {
        if (direction_ == Direction::TO) {
            if (value < left_ || value > right_) {
                throw std::domain_error("Value out of range");
            }
            return static_cast<size_t>(value - left_);
        } else {
            if (value > left_ || value < right_) {
                throw std::domain_error("Value out of range");
            }
            return static_cast<size_t>(left_ - value);
        }
    }

private:
    value_type left_;
    value_type right_;
    Direction direction_;
};

constexpr bool operator==(const Range::iterator& lhs,
                          const Range::iterator& rhs) noexcept {
    return *lhs == *rhs;
}

constexpr bool operator==(const Range& lhs, const Range& rhs) noexcept {
    if ((lhs.length() == 0) && (rhs.length() == 0)) {
        return true;
    }
    return lhs.left() == rhs.left() && lhs.right() == rhs.right() &&
           lhs.direction() == rhs.direction();
}

inline size_t hash(const Range& range) noexcept {
    return std::hash<Range::value_type>()(range.left()) ^
           std::hash<Range::value_type>()(range.right()) ^
           std::hash<Direction::value_type>()(range.direction().value());
}

static_assert(std::ranges::bidirectional_range<Range>);

}  // namespace coconext::types

#endif  // COCONEXT_RANGE_HPP

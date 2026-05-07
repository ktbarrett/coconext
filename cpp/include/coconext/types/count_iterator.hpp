#ifndef COCONEXT_COUNT_ITERATOR_HPP
#define COCONEXT_COUNT_ITERATOR_HPP

#include <cassert>
#include <coconext/types/direction.hpp>
#include <concepts>
#include <iterator>

namespace coconext::types {

template <std::signed_integral IntT>
class CountIterator {
public:
    using value_type = IntT;
    using difference_type =
        std::conditional_t<(sizeof(IntT) > sizeof(std::ptrdiff_t)), IntT,
                           std::ptrdiff_t>;
    using reference = value_type;
    using pointer = void;
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;

public:
    constexpr CountIterator() noexcept = default;
    constexpr CountIterator(const CountIterator&) noexcept = default;
    constexpr CountIterator& operator=(const CountIterator&) noexcept = default;
    constexpr CountIterator(CountIterator&&) noexcept = default;
    constexpr CountIterator& operator=(CountIterator&&) noexcept = default;
    constexpr CountIterator(value_type current, Direction direction) noexcept
        : current_(current), direction_(direction) {}

public:
    constexpr value_type operator*() const noexcept { return current_; }
    constexpr CountIterator& operator++() noexcept {
        if (direction_ == Direction::TO) {
            ++current_;
        } else {
            --current_;
        }
        return *this;
    }
    constexpr CountIterator operator++(int) noexcept {
        CountIterator temp = *this;
        ++(*this);
        return temp;
    }
    constexpr CountIterator& operator--() noexcept {
        if (direction_ == Direction::TO) {
            --current_;
        } else {
            ++current_;
        }
        return *this;
    }
    constexpr CountIterator operator--(int) noexcept {
        CountIterator temp = *this;
        --(*this);
        return temp;
    }
    constexpr CountIterator operator+(difference_type n) const noexcept {
        return direction_ == Direction::TO
                   ? CountIterator(current_ + n, direction_)
                   : CountIterator(current_ - n, direction_);
    }
    constexpr CountIterator operator-(difference_type n) const noexcept {
        return direction_ == Direction::TO
                   ? CountIterator(current_ - n, direction_)
                   : CountIterator(current_ + n, direction_);
    }
    constexpr difference_type operator-(
        const CountIterator& other) const noexcept {
        assert(direction_ == other.direction_ &&
               "Iterators from different ranges");
        return direction_ == Direction::TO ? current_ - other.current_
                                           : other.current_ - current_;
    }
    constexpr CountIterator& operator+=(difference_type n) noexcept {
        if (direction_ == Direction::TO) {
            current_ += n;
        } else {
            current_ -= n;
        }
        return *this;
    }
    constexpr CountIterator& operator-=(difference_type n) noexcept {
        if (direction_ == Direction::TO) {
            current_ -= n;
        } else {
            current_ += n;
        }
        return *this;
    }
    constexpr reference operator[](difference_type n) const noexcept {
        return direction_ == Direction::TO ? current_ + n : current_ - n;
    }
    friend constexpr auto operator<=>(const CountIterator& lhs,
                                      const CountIterator& rhs) noexcept {
        assert(lhs.direction_ == rhs.direction_ &&
               "Iterators from different ranges");
        // For DOWNTO, current_ decreases as we advance, so the natural ordering
        // on current_ is inverted relative to iteration order.
        return lhs.direction_ == Direction::TO
                   ? (lhs.current_ <=> rhs.current_)
                   : (rhs.current_ <=> lhs.current_);
    }
    friend constexpr bool operator==(const CountIterator& lhs,
                                     const CountIterator& rhs) noexcept {
        assert(lhs.direction_ == rhs.direction_ &&
               "Iterators from different ranges");
        return lhs.current_ == rhs.current_;
    }
    friend constexpr CountIterator operator+(difference_type n,
                                             const CountIterator& it) noexcept {
        return it + n;
    }

private:
    value_type current_{0};
    Direction direction_{Direction::TO};
};

static_assert(std::random_access_iterator<CountIterator<int>>);

}  // namespace coconext::types

#endif  // COCONEXT_COUNT_ITERATOR_HPP

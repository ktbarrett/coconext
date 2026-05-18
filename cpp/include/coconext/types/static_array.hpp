#ifndef COCONEXT_STATIC_ARRAY_HPP
#define COCONEXT_STATIC_ARRAY_HPP

#include <algorithm>
#include <array>
#include <coconext/types/array_base.hpp>
#include <coconext/types/concepts.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/range.hpp>
#include <concepts>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace coconext::types {

// Compile-time-bounded array. The range R is part of the type, so length,
// bounds checks, and indexing fold against R. Storage is std::array, so
// instances live in automatic / static storage with no heap allocation.
template <typename T, Range R>
class StaticArray {
  public:
    using value_type = T;
    using index_type = Range::value_type;
    static_assert(!std::is_reference_v<value_type>);
    static_assert(!std::is_const_v<value_type>);
    using reference = value_type&;
    using const_reference = value_type const&;
    using iterator = typename std::array<value_type, R.length()>::iterator;
    using const_iterator = typename std::array<value_type, R.length()>::const_iterator;

    static constexpr Range static_range = R;

    constexpr StaticArray()
        requires std::default_initializable<value_type>
        : data_{} {}

    constexpr StaticArray(StaticArray const&) = default;
    constexpr StaticArray(StaticArray&&) = default;
    constexpr StaticArray& operator=(StaticArray const&) = default;
    constexpr StaticArray& operator=(StaticArray&&) = default;

    template <typename U>
        requires std::convertible_to<U, value_type>
    constexpr StaticArray(std::initializer_list<U> init) : data_{} {
        if (init.size() != R.length()) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " does not match StaticArray length " + std::to_string(R.length())
            );
        }
        std::copy(init.begin(), init.end(), data_.begin());
    }

    template <std::ranges::sized_range Rng>
        requires std::convertible_to<std::ranges::range_value_t<Rng>, value_type>
              && (!std::same_as<std::remove_cvref_t<Rng>, StaticArray>)
    explicit constexpr StaticArray(Rng const& obj) : data_{} {
        if (std::ranges::size(obj) != R.length()) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(std::ranges::size(obj))
                + " does not match StaticArray length " + std::to_string(R.length())
            );
        }
        std::ranges::copy(obj, data_.begin());
    }

    constexpr Range range() const noexcept { return R; }

    constexpr reference operator[](index_type idx) {
        if constexpr (R.direction == Direction::TO) {
            if (idx < R.left || idx > R.right) {
                throw std::out_of_range("Index out of bounds");
            }
            return *(begin() + (idx - R.left));
        } else {
            if (idx > R.left || idx < R.right) {
                throw std::out_of_range("Index out of bounds");
            }
            return *(begin() + (R.left - idx));
        }
    }
    constexpr const_reference operator[](index_type idx) const {
        if constexpr (R.direction == Direction::TO) {
            if (idx < R.left || idx > R.right) {
                throw std::out_of_range("Index out of bounds");
            }
            return *(begin() + (idx - R.left));
        } else {
            if (idx > R.left || idx < R.right) {
                throw std::out_of_range("Index out of bounds");
            }
            return *(begin() + (R.left - idx));
        }
    }
    constexpr ArraySlice<StaticArray> operator[](Range r) {
        detail::subsequence_check(static_range, r);
        return ArraySlice<StaticArray>(this, r);
    }
    constexpr ArraySlice<StaticArray const> operator[](Range r) const {
        detail::subsequence_check(static_range, r);
        return ArraySlice<StaticArray const>(this, r);
    }

    constexpr iterator begin() noexcept { return data_.begin(); }
    constexpr const_iterator begin() const noexcept { return data_.begin(); }
    constexpr iterator end() noexcept { return data_.end(); }
    constexpr const_iterator end() const noexcept { return data_.end(); }
    constexpr auto rbegin() noexcept { return data_.rbegin(); }
    constexpr auto rbegin() const noexcept { return data_.rbegin(); }
    constexpr auto rend() noexcept { return data_.rend(); }
    constexpr auto rend() const noexcept { return data_.rend(); }

  private:
    std::array<value_type, R.length()> data_;
};

template <typename T, Range R>
    requires std::equality_comparable<T>
constexpr bool operator==(
    StaticArray<T, R> const& lhs, StaticArray<T, R> const& rhs
) noexcept {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

static_assert(RangedSequence<StaticArray<int, Range{0, Direction::TO, 7}>>);
static_assert(RangedSequence<StaticArray<int, Range{0, Direction::TO, 7}> const>);
static_assert(RangedSequence<ArraySlice<StaticArray<int, Range{0, Direction::TO, 7}>>>);
static_assert(
    RangedSequence<ArraySlice<StaticArray<int, Range{0, Direction::TO, 7}> const>>
);

}  // namespace coconext::types

template <typename T, coconext::types::Range R>
struct std::hash<coconext::types::StaticArray<T, R>> {
    size_t operator()(coconext::types::StaticArray<T, R> const& arr) const noexcept {
        size_t seed = 0;
        for (auto const& elem : arr) {
            seed = coconext::types::detail::hash_combine(seed, elem);
        }
        return seed;
    }
};

template <typename T, coconext::types::Range R>
    requires coconext::types::detail::Formattable<T>
struct std::formatter<coconext::types::StaticArray<T, R>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("StaticArray formatter takes no format spec");
        }
        return it;
    }

    auto format(
        coconext::types::StaticArray<T, R> const& arr, std::format_context& ctx
    ) const {
        return coconext::types::detail::format_array(arr, ctx.out());
    }
};

#endif  // COCONEXT_STATIC_ARRAY_HPP

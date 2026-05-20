#ifndef COCONEXT_ARRAY_HPP
#define COCONEXT_ARRAY_HPP

#include <algorithm>
#include <array>
#include <coconext/types/array_base.hpp>
#include <coconext/types/concepts.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/range.hpp>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

namespace coconext::types {

namespace detail {

template <auto V>
constexpr bool fits_range_value_type =
    static_cast<long long>(V) >= std::numeric_limits<Range::value_type>::min()
    && static_cast<long long>(V) <= std::numeric_limits<Range::value_type>::max();

template <auto... Args>
constexpr Range make_static_range() {
    using std::get;
    static_assert(
        sizeof...(Args) >= 1 && sizeof...(Args) <= 3, "Array<T, ...> takes 1-3 range args"
    );
    constexpr auto t = std::tuple{Args...};
    if constexpr (sizeof...(Args) == 1) {
        using First = std::remove_cvref_t<decltype(get<0>(t))>;
        static_assert(
            std::is_same_v<First, Range> || std::integral<First>,
            "single template arg must be a Range value or an integral length"
        );
        if constexpr (std::is_same_v<First, Range>) {
            return get<0>(t);
        } else {
            static_assert(get<0>(t) >= 0, "Array<T, N>: N (length) must be non-negative");
            return Range(static_cast<size_t>(get<0>(t)));
        }
    } else if constexpr (sizeof...(Args) == 2) {
        static_assert(
            fits_range_value_type<get<0>(t)>,
            "Array<T, L, H>: L does not fit in Range::value_type (int32_t)"
        );
        static_assert(
            fits_range_value_type<get<1>(t)>,
            "Array<T, L, H>: H does not fit in Range::value_type (int32_t)"
        );
        return Range{
            static_cast<Range::value_type>(get<0>(t)),
            static_cast<Range::value_type>(get<1>(t))
        };
    } else {  // 3
        static_assert(
            std::is_same_v<std::remove_cvref_t<decltype(get<1>(t))>, Direction>,
            "three-arg form requires (left, Direction, right)"
        );
        static_assert(
            fits_range_value_type<get<0>(t)>,
            "Array<T, L, D, H>: L does not fit in Range::value_type (int32_t)"
        );
        static_assert(
            fits_range_value_type<get<2>(t)>,
            "Array<T, L, D, H>: H does not fit in Range::value_type (int32_t)"
        );
        return Range{
            static_cast<Range::value_type>(get<0>(t)),
            get<1>(t),
            static_cast<Range::value_type>(get<2>(t))
        };
    }
}

// Compile-time-bounded array. The range R is part of the type, so length,
// bounds checks, and indexing fold against R. Storage is std::array, so
// instances live in automatic / static storage with no heap allocation.
template <typename T, Range R>
class Array {
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

    constexpr Array()
        requires std::default_initializable<value_type>
        : data_{} {}

    constexpr Array(Array const&) = default;
    constexpr Array(Array&&) = default;
    constexpr Array& operator=(Array const&) = default;
    constexpr Array& operator=(Array&&) = default;

    template <typename U>
        requires std::convertible_to<U, value_type>
    constexpr Array(std::initializer_list<U> init) : data_{} {
        if (init.size() != R.length()) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " does not match Array length " + std::to_string(R.length())
            );
        }
        std::copy(init.begin(), init.end(), data_.begin());
    }

    template <std::ranges::sized_range Rng>
        requires std::convertible_to<std::ranges::range_value_t<Rng>, value_type>
              && (!std::same_as<std::remove_cvref_t<Rng>, Array>)
    explicit constexpr Array(Rng const& obj) : data_{} {
        if (std::ranges::size(obj) != R.length()) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(std::ranges::size(obj))
                + " does not match Array length " + std::to_string(R.length())
            );
        }
        std::ranges::copy(obj, data_.begin());
    }

    constexpr Range range() const noexcept { return R; }

    constexpr reference operator[](index_type idx) { return access_(*this, idx); }
    constexpr const_reference operator[](index_type idx) const {
        return access_(*this, idx);
    }
    constexpr ArraySlice<Array> operator[](Range r) {
        detail::subsequence_check(static_range, r);
        return ArraySlice<Array>(this, r);
    }
    constexpr ArraySlice<Array const> operator[](Range r) const {
        detail::subsequence_check(static_range, r);
        return ArraySlice<Array const>(this, r);
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
    template <typename Self>
    static constexpr auto& access_(Self& self, index_type idx) {
        if constexpr (R.direction == Direction::TO) {
            if (idx < R.left || idx > R.right) {
                throw std::out_of_range("Index out of bounds");
            }
            return *(self.begin() + (idx - R.left));
        } else {
            if (idx > R.left || idx < R.right) {
                throw std::out_of_range("Index out of bounds");
            }
            return *(self.begin() + (R.left - idx));
        }
    }

    std::array<value_type, R.length()> data_;
};

template <typename T, Range R>
    requires std::equality_comparable<T>
constexpr bool operator==(Array<T, R> const& lhs, Array<T, R> const& rhs) noexcept {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T, Range R>
struct is_array<Array<T, R>> : std::true_type {};

static_assert(RangedSequence<Array<int, Range{0, Direction::TO, 7}>>);
static_assert(RangedSequence<Array<int, Range{0, Direction::TO, 7}> const>);
static_assert(RangedSequence<ArraySlice<Array<int, Range{0, Direction::TO, 7}>>>);
static_assert(RangedSequence<ArraySlice<Array<int, Range{0, Direction::TO, 7}> const>>);

}  // namespace detail

// Static-bounded array. Range is built from the trailing NTTPs:
//   Array<T, N>           (integral N)      -> detail::Array<T, Range(N)>
//   Array<T, R>           (Range R)         -> detail::Array<T, R>
//   Array<T, L, H>        (integral L, H)   -> detail::Array<T, Range{L, H}>
//   Array<T, L, D, H>     (D = Direction)   -> detail::Array<T, Range{L, D, H}>
template <typename T, auto... Args>
using Array = detail::Array<T, detail::make_static_range<Args...>()>;

}  // namespace coconext::types

template <typename T, coconext::types::Range R>
struct std::hash<coconext::types::detail::Array<T, R>> {
    size_t operator()(coconext::types::detail::Array<T, R> const& arr) const noexcept {
        size_t seed = 0;
        for (auto const& elem : arr) {
            seed = coconext::types::detail::hash_combine(seed, elem);
        }
        return seed;
    }
};

#endif  // COCONEXT_ARRAY_HPP

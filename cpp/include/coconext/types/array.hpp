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
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

namespace coconext::types {

namespace detail {

template <typename T, Range R>
class Array;

template <typename T, Range R>

// The actual Array implementation. Separated out so it can be reused for the LogicArray
// specialization.
class ArrayImpl {
  public:
    using value_type = T;
    using index_type = Range::value_type;
    static_assert(!std::is_reference_v<value_type>);
    static_assert(!std::is_const_v<value_type>);
    using reference = value_type&;
    using const_reference = value_type const&;
    using iterator = typename std::array<value_type, R.length()>::iterator;
    using const_iterator = typename std::array<value_type, R.length()>::const_iterator;

    constexpr ArrayImpl()
        requires std::default_initializable<value_type>
        : data_{} {}

    constexpr ArrayImpl(ArrayImpl const&) = default;
    constexpr ArrayImpl(ArrayImpl&&) = default;
    constexpr ArrayImpl& operator=(ArrayImpl const&) = default;
    constexpr ArrayImpl& operator=(ArrayImpl&&) = default;

    template <typename U>
        requires std::convertible_to<U, value_type>
    constexpr ArrayImpl(std::initializer_list<U> init) : data_{} {
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
              && (!std::derived_from<std::remove_cvref_t<Rng>, ArrayImpl>)
    explicit constexpr ArrayImpl(Rng const& obj) : data_{} {
        if (std::ranges::size(obj) != R.length()) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(std::ranges::size(obj))
                + " does not match Array length " + std::to_string(R.length())
            );
        }
        std::ranges::copy(obj, data_.begin());
    }

    // The range, exposed two ways: `static_range` for type-level access
    // (`T::static_range`, used by the StaticRangedSequence concept), and a
    // plain constexpr `range()` member matching Vector's instance accessor
    // so generic code can write `obj.range()` uniformly across both.
    static constexpr Range static_range = R;
    constexpr Range range() const noexcept { return static_range; }
    constexpr size_t size() const noexcept { return static_range.length(); }

    constexpr reference operator[](index_type idx) { return access_(*this, idx); }
    constexpr const_reference operator[](index_type idx) const {
        return access_(*this, idx);
    }

    // Slices are constructed with the outer `Array<T, R>` (or const variant)
    // as the owner. `this` is an ArrayImpl*, but the outer Array derives from
    // it, so static_cast safely lands on the most-derived type at every
    // instantiation -- and routes through the Logic/Bit slice partial spec
    // when applicable.
    constexpr ArraySlice<Array<T, R>> operator[](Range r) {
        detail::subsequence_check(R, r);
        return ArraySlice<Array<T, R>>(static_cast<Array<T, R>*>(this), r);
    }
    constexpr ArraySlice<Array<T, R> const> operator[](Range r) const {
        detail::subsequence_check(R, r);
        return ArraySlice<Array<T, R> const>(static_cast<Array<T, R> const*>(this), r);
    }
#if __cplusplus >= 202302L
    constexpr ArraySlice<Array<T, R>> operator[](
        Range::value_type left, Range::value_type right
    ) {
        return (*this)[Range{left, R.direction, right}];
    }
    constexpr ArraySlice<Array<T, R>> operator[](
        Range::value_type left, Direction dir, Range::value_type right
    ) {
        return (*this)[Range{left, dir, right}];
    }
    constexpr ArraySlice<Array<T, R> const> operator[](
        Range::value_type left, Range::value_type right
    ) const {
        return (*this)[Range{left, R.direction, right}];
    }
    constexpr ArraySlice<Array<T, R> const> operator[](
        Range::value_type left, Direction dir, Range::value_type right
    ) const {
        return (*this)[Range{left, dir, right}];
    }
#endif

    template <Range R2>
    constexpr StaticArraySlice<Array<T, R>, R2> slice() {
        static_assert(
            R2.is_subsequence_of(R),
            "static sub-slice range is not a sub-range of the parent Array"
        );
        return StaticArraySlice<Array<T, R>, R2>(static_cast<Array<T, R>*>(this));
    }
    template <Range R2>
    constexpr StaticArraySlice<Array<T, R> const, R2> slice() const {
        static_assert(
            R2.is_subsequence_of(R),
            "static sub-slice range is not a sub-range of the parent Array"
        );
        return StaticArraySlice<Array<T, R> const, R2>(
            static_cast<Array<T, R> const*>(this)
        );
    }

    template <index_type I>
    constexpr reference index() {
        static_assert(find(R, I) != R.end(), "index is out of range");
        return (*this)[I];
    }
    template <index_type I>
    constexpr const_reference index() const {
        static_assert(find(R, I) != R.end(), "index is out of range");
        return (*this)[I];
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

// Local-allocated, compile-time-bounded array indexed according to its Range.
// This is the canonical "Array" implementation. It's stuck in detail since `Array`
template <typename T, Range R>
class Array : public ArrayImpl<T, R> {
  public:
    using ArrayImpl<T, R>::ArrayImpl;
    using ArrayImpl<T, R>::operator=;
};

template <typename T, Range R1, Range R2>
    requires std::equality_comparable<T>
constexpr bool operator==(Array<T, R1> const& lhs, Array<T, R2> const& rhs) noexcept {
    if constexpr (R1 != R2) {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename T, Range R>
struct is_array<Array<T, R>> : std::true_type {};

static_assert(RangedSequence<Array<int, Range{0, Direction::TO, 7}>>);
static_assert(RangedSequence<Array<int, Range{0, Direction::TO, 7}> const>);
static_assert(RangedSequence<ArraySlice<Array<int, Range{0, Direction::TO, 7}>>>);
static_assert(RangedSequence<ArraySlice<Array<int, Range{0, Direction::TO, 7}> const>>);
static_assert(RangedSequence<StaticArraySlice<
                  Array<int, Range{0, Direction::TO, 7}>,
                  Range{1, Direction::TO, 3}>>);
static_assert(RangedSequence<StaticArraySlice<
                  Array<int, Range{0, Direction::TO, 7}> const,
                  Range{1, Direction::TO, 3}>>);

static_assert(StaticRangedSequence<Array<int, Range{0, Direction::TO, 7}>>);
static_assert(StaticRangedSequence<Array<int, Range{0, Direction::TO, 7}> const>);
static_assert(!StaticRangedSequence<ArraySlice<Array<int, Range{0, Direction::TO, 7}>>>);
static_assert(StaticRangedSequence<StaticArraySlice<
                  Array<int, Range{0, Direction::TO, 7}>,
                  Range{1, Direction::TO, 3}>>);

static_assert(std::ranges::sized_range<Array<int, Range{0, Direction::TO, 7}>>);
static_assert(std::ranges::sized_range<ArraySlice<Array<int, Range{0, Direction::TO, 7}>>>);
static_assert(std::ranges::sized_range<StaticArraySlice<
                  Array<int, Range{0, Direction::TO, 7}>,
                  Range{1, Direction::TO, 3}>>);

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

}  // namespace detail

// Static-bounded array. Storage is local, not on the heap, similar to std::array. Range is
// built from the trailing NTTPs:
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

// Formatter for Array<T, R>. The Logic/Bit specializations in logic_array.hpp
// are more-specialized partial specs and win by C++'s partial-ordering rules
// when both headers are visible.
template <typename T, coconext::types::Range R>
    requires coconext::types::detail::Formattable<T>
struct std::formatter<coconext::types::detail::Array<T, R>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Array formatter takes no format spec");
        }
        return it;
    }

    auto format(
        coconext::types::detail::Array<T, R> const& arr, std::format_context& ctx
    ) const {
        return coconext::types::detail::format_array("Array", arr, ctx.out());
    }
};

#endif  // COCONEXT_ARRAY_HPP

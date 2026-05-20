#ifndef COCONEXT_ARRAY_BASE_HPP
#define COCONEXT_ARRAY_BASE_HPP

#include <algorithm>
#include <cassert>
#include <coconext/types/concepts.hpp>
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

namespace detail {

template <typename R>
concept sized_input_range = std::ranges::sized_range<R> && std::ranges::input_range<R>;

constexpr void subsequence_check(Range parent, Range child) {
    if (!is_subsequence(parent, child)) {
        throw std::invalid_argument("Range is not a valid sub-range of the parent");
    }
}

}  // namespace detail

// The minimum a type must expose so that index() and slice() can be
// implemented for it externally.
template <typename T>
concept RangedSequence = std::ranges::random_access_range<T> && requires(T& t) {
    { t.range() } -> std::convertible_to<Range>;
};

template <typename ArrayT>
class ArraySlice;

template <typename ArrayT>
class ArraySlice {
  public:
    // ArraySlice is a non-owning view (like std::span). Element mutability is
    // determined by ArrayT's constness:
    // - ArraySlice<ArrayT>          -- mutable view, can write elements.
    // - ArraySlice<const ArrayT>    -- read-only view.
    // - const ArraySlice<X>         -- pointer/range fixed; element access
    //                                  follows X's mutability rules.
    using value_type = std::ranges::range_value_t<ArrayT>;
    using reference = std::ranges::range_reference_t<ArrayT>;
    using index_type = Range::value_type;
    using iterator = std::ranges::iterator_t<ArrayT>;
    static_assert(!std::is_reference_v<value_type>);

    constexpr ArraySlice() = delete;
    constexpr ArraySlice(ArraySlice const&) = default;
    constexpr ArraySlice(ArraySlice&&) = default;
    constexpr ArraySlice(ArrayT* arr, Range range) noexcept : arr_(arr), range_(range) {}

    constexpr Range const& range() const noexcept { return range_; }

    // Element access bounds-checks against this slice's range_, not the
    // owner's range. An idx that's valid in the owner but outside the slice
    // is out-of-range.
    constexpr reference operator[](index_type idx) const {
        auto it = find(range_, idx);
        if (it == range_.end()) {
            throw std::out_of_range("Index out of bounds");
        }
        return *(begin() + std::distance(range_.begin(), it));
    }
    // Sub-slicing flattens: returns a new ArraySlice over the same underlying
    // array with a new range. Validity is checked against the slice's own
    // range_, not arr_->range().
    constexpr ArraySlice operator[](Range r) const {
        detail::subsequence_check(range_, r);
        return ArraySlice(arr_, r);
    }

    template <detail::sized_input_range R>
        requires(!std::is_const_v<ArrayT>)
             && std::convertible_to<std::ranges::range_value_t<R>, value_type>
    constexpr ArraySlice const& operator=(R const& obj) const {
        if (std::ranges::size(obj) != range_.length()) {
            throw std::invalid_argument(
                "Value of length " + std::to_string(std::ranges::size(obj))
                + " cannot be assigned to array slice of length "
                + std::to_string(range_.length())
            );
        }
        std::ranges::copy(obj, begin());
        return *this;
    }

    template <typename T>
        requires(!std::is_const_v<ArrayT>) && std::convertible_to<T, value_type>
    constexpr ArraySlice const& operator=(std::initializer_list<T> init) const {
        if (init.size() != static_cast<size_t>(range_.length())) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " cannot be assigned to array slice of length "
                + std::to_string(range_.length())
            );
        }
        std::ranges::copy(init, begin());
        return *this;
    }

    constexpr iterator begin() const noexcept {
        // Null slices may carry bounds outside the parent's range (the
        // validity rule allows that). Pin them to the parent's begin so
        // begin()/end() form a well-formed empty iterator pair.
        if (range_.length() == 0) {
            return arr_->begin();
        }
        auto start = find(arr_->range(), range_.left);
        assert(
            start != arr_->range().end()
            && "slice range not a sub-range of the owner's range"
        );
        return arr_->begin() + std::distance(arr_->range().begin(), start);
    }
    constexpr iterator end() const noexcept { return begin() + range_.length(); }
    constexpr auto rbegin() const noexcept { return std::reverse_iterator(end()); }
    constexpr auto rend() const noexcept { return std::reverse_iterator(begin()); }

  private:
    ArrayT* arr_;
    Range range_;
};

namespace detail {

// Opt-in trait that array types specialize to participate in the generic
// std::formatter<ArrayLike> below (and in any future array-only generic
// machinery). Specialized for ArraySlice here; each owning array type
// (DynamicArray, Array) specializes it in its own header so the trait
// visibility tracks the type's visibility.
template <typename T>
struct is_array : std::false_type {};

template <typename ArrayT>
struct is_array<ArraySlice<ArrayT>> : std::true_type {};

// Any array type whose element type is formattable. The single
// std::formatter<ArrayLike T> partial specialization in std:: below picks
// up every type that opts in via is_array, eliminating the per-type
// formatter boilerplate that otherwise scales with each new array family.
template <typename T>
concept ArrayLike =
    is_array<std::remove_cvref_t<T>>::value && Formattable<std::ranges::range_value_t<T>>;

// Walks a RangedSequence, emitting "[range]{elem, elem, ...}" via the formatter
// for each element type. Used by the generic Array/Slice formatter.
template <RangedSequence ArrayT, typename OutIt>
    requires Formattable<std::ranges::range_value_t<ArrayT>>
OutIt format_array(ArrayT const& arr, OutIt out) {
    out = std::format_to(out, "{}{{", arr.range());
    bool first = true;
    for (auto const& elem : arr) {
        if (!first) {
            out = std::format_to(out, ", ");
        }
        out = std::format_to(out, "{}", elem);
        first = false;
    }
    *out++ = '}';
    return out;
}

}  // namespace detail

}  // namespace coconext::types

// One generic formatter for every array type that opts into is_array. The
// LogicType-constrained version in logic_array.hpp subsumes this one for
// arrays of Logic/Bit (via constraint conjunction) and produces the terse
// "Logic[range]{0, 1, X}" form instead.
template <typename T>
    requires coconext::types::detail::ArrayLike<T>
struct std::formatter<T> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("ArrayLike formatter takes no format spec");
        }
        return it;
    }

    auto format(T const& arr, std::format_context& ctx) const {
        return coconext::types::detail::format_array(arr, ctx.out());
    }
};

#endif  // COCONEXT_ARRAY_BASE_HPP

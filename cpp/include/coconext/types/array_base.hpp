#ifndef COCONEXT_ARRAY_BASE_HPP
#define COCONEXT_ARRAY_BASE_HPP

#include <algorithm>
#include <cassert>
#include <coconext/types/concepts.hpp>
#include <coconext/types/range.hpp>
#include <concepts>
#include <format>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace coconext::types {

// -- Concepts and traits ------------------------------------------------------

namespace detail {

template <typename R>
concept sized_input_range = std::ranges::sized_range<R> && std::ranges::input_range<R>;

// Opt-in trait that array types.
template <typename T>
struct is_array : std::false_type {};

template <typename T>
concept has_range_member = requires(T const& t) {
    { t.range() } -> std::convertible_to<Range>;
};

template <typename T>
concept has_static_range_member = requires {
    { T::static_range } -> std::convertible_to<Range>;
};

}  // namespace detail

// Customization point for the runtime range of a sequence. Specialize this
// for external/wrapper types that can't or don't expose t.range() directly
// (e.g. std::vector, std::span, std::string). A default partial specialization
// below picks up types that already provide t.range(), so our own array types
// participate without writing one.
template <typename T>
struct range_of;

template <detail::has_range_member T>
struct range_of<T> {
    static constexpr Range get(T const& t) { return t.range(); }
};

// Customization point for the compile-time range. Specialize so that ::value
// is a Range constant expression. Non-intrusive: external/wrapper types can
// participate without modification. A default partial specialization below
// picks up types that expose a `static constexpr Range static_range` member,
// so our own array types participate without writing one.
template <typename T>
struct static_range_of;

template <detail::has_static_range_member T>
struct static_range_of<T> {
    static constexpr Range value = T::static_range;
};

// Fallback range_of for types that opt into static_range_of but lack a
// .range() member (e.g. std::array<T, N>). Picks up the compile-time range
// so a single static_range_of specialization is enough.
template <typename T>
    requires(!detail::has_range_member<T>) && requires {
        { static_range_of<T>::value } -> std::convertible_to<Range>;
    }
struct range_of<T> {
    static constexpr Range get(T const&) { return static_range_of<T>::value; }
};

// The minimum a type must expose so that index() and slice() can be
// implemented for it externally. Satisfied either via an intrusive .range()
// member or via a range_of specialization.
template <typename T>
concept RangedSequence = std::ranges::random_access_range<T> && requires(T const& t) {
    { range_of<std::remove_cvref_t<T>>::get(t) } -> std::convertible_to<Range>;
};

// A RangedSequence whose range is known at compile time, exposed via the
// static_range_of trait. Lets generic code fold offsets into the owner against
// a constexpr range instead of recomputing them at runtime via find()/distance().
template <typename T>
concept StaticRangedSequence = RangedSequence<T> && requires {
    { static_range_of<std::remove_cvref_t<T>>::value } -> std::convertible_to<Range>;
};

// Any type that opts into the array machinery via is_array. Element-type
// constraints (formattability, etc.) live on the consumers that need them,
// not on this concept.
template <typename T>
concept ArrayType = detail::is_array<std::remove_cvref_t<T>>::value;

// -- Slice types --------------------------------------------------------------

namespace detail {

constexpr void subsequence_check(Range parent, Range child) {
    if (!is_subsequence(parent, child)) {
        throw std::invalid_argument("Range is not a valid sub-range of the parent");
    }
}

}  // namespace detail

template <typename ArrayT, Range R>
class ArraySlice;

template <typename ArrayT>
class DynArraySlice {
  public:
    // DynArraySlice is a non-owning view (like std::span) with a runtime
    // range. Element mutability is determined by ArrayT's constness:
    // - DynArraySlice<ArrayT>          -- mutable view, can write elements.
    // - DynArraySlice<const ArrayT>    -- read-only view.
    // - const DynArraySlice<X>         -- pointer/range fixed; element access
    //                                     follows X's mutability rules.
    using value_type = std::ranges::range_value_t<ArrayT>;
    using reference = std::ranges::range_reference_t<ArrayT>;
    using index_type = Range::value_type;
    using iterator = std::ranges::iterator_t<ArrayT>;
    static_assert(!std::is_reference_v<value_type>);

    constexpr DynArraySlice() = delete;
    constexpr DynArraySlice(DynArraySlice const&) = default;
    constexpr DynArraySlice(DynArraySlice&&) = default;
    constexpr DynArraySlice(ArrayT* arr, Range range) noexcept : arr_(arr), range_(range) {}

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
    // Sub-slicing flattens: returns a new DynArraySlice over the same
    // underlying array with a new range. Validity is checked against the
    // slice's own range_, not arr_->range().
    constexpr DynArraySlice operator[](Range r) const {
        detail::subsequence_check(range_, r);
        return DynArraySlice(arr_, r);
    }

    // Static sub-slicing flattens to ArraySlice<ArrayT, R> over the same
    // underlying array. The subsequence check is runtime here (this slice's
    // own bound is dynamic).
    template <Range R>
    constexpr ArraySlice<ArrayT, R> slice() const {
        detail::subsequence_check(range_, R);
        return ArraySlice<ArrayT, R>(arr_);
    }

    template <detail::sized_input_range R>
        requires(!std::is_const_v<ArrayT>)
             && std::convertible_to<std::ranges::range_value_t<R>, value_type>
    constexpr DynArraySlice const& operator=(R const& obj) const {
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
    constexpr DynArraySlice const& operator=(std::initializer_list<T> init) const {
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

// Compile-time-bounded counterpart to DynArraySlice. R is part of the type,
// so length, bounds checks, and indexing fold against R. When the parent's
// range is also a compile-time constant (a static Array, or another static
// ArraySlice), begin()/end() collapse to a single pointer offset.
template <typename ArrayT, Range R>
class ArraySlice {
  public:
    using value_type = std::ranges::range_value_t<ArrayT>;
    using reference = std::ranges::range_reference_t<ArrayT>;
    using index_type = Range::value_type;
    using iterator = std::ranges::iterator_t<ArrayT>;
    static_assert(!std::is_reference_v<value_type>);

    static constexpr Range static_range = R;

    constexpr ArraySlice() = delete;
    constexpr ArraySlice(ArraySlice const&) = default;
    constexpr ArraySlice(ArraySlice&&) = default;
    constexpr explicit ArraySlice(ArrayT* arr) noexcept : arr_(arr) {}

    constexpr Range const& range() const noexcept { return static_range; }

    constexpr reference operator[](index_type idx) const {
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

    // Static sub-slicing flattens to ArraySlice<ArrayT, R2> over the same
    // underlying array. R2's validity against R is checked at compile time.
    template <Range R2>
    constexpr ArraySlice<ArrayT, R2> slice() const {
        static_assert(
            is_subsequence(R, R2),
            "static sub-slice range is not a sub-range of the parent slice"
        );
        return ArraySlice<ArrayT, R2>(arr_);
    }

    // Runtime sub-slicing flattens to DynArraySlice<ArrayT> -- the result is
    // no longer compile-time-bounded, since r is a runtime value.
    constexpr DynArraySlice<ArrayT> operator[](Range r) const {
        detail::subsequence_check(R, r);
        return DynArraySlice<ArrayT>(arr_, r);
    }

    template <detail::sized_input_range Rng>
        requires(!std::is_const_v<ArrayT>)
             && std::convertible_to<std::ranges::range_value_t<Rng>, value_type>
    constexpr ArraySlice const& operator=(Rng const& obj) const {
        if (std::ranges::size(obj) != R.length()) {
            throw std::invalid_argument(
                "Value of length " + std::to_string(std::ranges::size(obj))
                + " cannot be assigned to array slice of length "
                + std::to_string(R.length())
            );
        }
        std::ranges::copy(obj, begin());
        return *this;
    }

    template <typename T>
        requires(!std::is_const_v<ArrayT>) && std::convertible_to<T, value_type>
    constexpr ArraySlice const& operator=(std::initializer_list<T> init) const {
        if (init.size() != R.length()) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " cannot be assigned to array slice of length "
                + std::to_string(R.length())
            );
        }
        std::ranges::copy(init, begin());
        return *this;
    }

    constexpr iterator begin() const noexcept {
        // Null slices may carry bounds outside the parent's range (the
        // validity rule allows that). Pin them to the parent's begin so
        // begin()/end() form a well-formed empty iterator pair.
        if constexpr (R.length() == 0) {
            return arr_->begin();
        } else if constexpr (StaticRangedSequence<ArrayT>) {
            // Parent's range is a compile-time constant; fold the offset to
            // a constant instead of recomputing it via find()/distance().
            constexpr auto parent = static_range_of<std::remove_cvref_t<ArrayT>>::value;
            constexpr auto offset = parent.direction == Direction::TO
                                      ? R.left - parent.left
                                      : parent.left - R.left;
            return arr_->begin() + offset;
        } else {
            auto start = find(arr_->range(), R.left);
            assert(
                start != arr_->range().end()
                && "slice range not a sub-range of the owner's range"
            );
            return arr_->begin() + std::distance(arr_->range().begin(), start);
        }
    }
    constexpr iterator end() const noexcept { return begin() + R.length(); }
    constexpr auto rbegin() const noexcept { return std::reverse_iterator(end()); }
    constexpr auto rend() const noexcept { return std::reverse_iterator(begin()); }

  private:
    ArrayT* arr_;
};

namespace detail {

template <typename ArrayT>
struct is_array<DynArraySlice<ArrayT>> : std::true_type {};

template <typename ArrayT, Range R>
struct is_array<ArraySlice<ArrayT, R>> : std::true_type {};

// -- Formatter ----------------------------------------------------------------

// Walks a RangedSequence, emitting "[range]{elem, elem, ...}" via the formatter
// for each element type. Used by the generic Array/Slice formatter.
template <RangedSequence ArrayT, typename OutIt>
    requires Formattable<std::ranges::range_value_t<ArrayT>>
OutIt format_array(ArrayT const& arr, OutIt out) {
    out = std::format_to(out, "{}{{", range_of<std::remove_cvref_t<ArrayT>>::get(arr));
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
    requires coconext::types::ArrayType<T>
          && coconext::types::detail::Formattable<std::ranges::range_value_t<T>>
struct std::formatter<T> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("ArrayType formatter takes no format spec");
        }
        return it;
    }

    auto format(T const& arr, std::format_context& ctx) const {
        return coconext::types::detail::format_array(arr, ctx.out());
    }
};

#endif  // COCONEXT_ARRAY_BASE_HPP

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
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace coconext::types {

// -- Concepts -----------------------------------------------------------------

namespace detail {

template <typename R>
concept sized_input_range = std::ranges::sized_range<R> && std::ranges::input_range<R>;

// Opt-in trait for array types.
template <typename T>
struct is_array : std::false_type {};

// Helper to force a value into a constant-expression context.
template <auto V>
struct require_constant {};

}  // namespace detail

// A random-access range whose range is queryable via `obj.range()`. The
// optional `Elem` template parameter constrains the element type: `Elem = void`
// (the default) matches any element type; a concrete type requires the element
// to match exactly. So `RangedSequence T` matches any ranged sequence,
// `RangedSequence<Logic> T` matches only ranged sequences of Logic. Concepts
// can't be passed as template arguments in C++20, so element-concept
// constraints (e.g. "any LogicType element") still need a composed concept or
// a separate requires clause.
template <typename T, typename Elem = void>
concept RangedSequence = std::ranges::random_access_range<T> && requires(T const& t) {
    { t.range() } -> std::convertible_to<Range>;
} && (std::is_void_v<Elem> || std::same_as<std::ranges::range_value_t<T>, Elem>);

// A RangedSequence whose range is also a compile-time constant -- i.e., T
// exposes its Range as a static constexpr `static_range` member.
// Refines RangedSequence so overload resolution prefers it when both match.
template <typename T, typename Elem = void>
concept StaticRangedSequence = RangedSequence<T, Elem> && requires {
    typename detail::require_constant<std::remove_cvref_t<T>::static_range>;
};

// Any type that opts into the array machinery via is_array. Element-type
// constraints (formattability, etc.) live on the consumers that need them,
// not on this concept.
template <typename T>
concept ArrayType = detail::is_array<std::remove_cvref_t<T>>::value;

// -- Slice types --------------------------------------------------------------

namespace detail {

constexpr void subsequence_check(Range parent, Range child) {
    if (!child.is_subsequence_of(parent)) {
        throw std::invalid_argument("Range is not a valid sub-range of the parent");
    }
}

// Convert a 0-based offset into the data buffer (iteration order) to the
// corresponding HDL coordinate for the given range's direction.
constexpr Range::value_type offset_to_hdl_coord(
    Range r, size_t offset_from_begin
) noexcept {
    auto const off = static_cast<Range::value_type>(offset_from_begin);
    return r.direction == Direction::TO ? r.left + off : r.left - off;
}

}  // namespace detail

// First HDL coordinate (from the left in iteration order) whose element equals
// `v`, or nullopt if not found.
template <RangedSequence S>
constexpr std::optional<Range::value_type> index_of(
    S const& s, std::ranges::range_value_t<S> const& v
) {
    auto const it = std::ranges::find(s, v);
    if (it == s.end()) {
        return std::nullopt;
    }
    return detail::offset_to_hdl_coord(
        s.range(), static_cast<size_t>(std::ranges::distance(s.begin(), it))
    );
}

// First HDL coordinate from the right (i.e. the last matching element in
// iteration order), or nullopt if not found.
template <RangedSequence S>
constexpr std::optional<Range::value_type> rindex_of(
    S const& s, std::ranges::range_value_t<S> const& v
) {
    auto const rit = std::find(s.rbegin(), s.rend(), v);
    if (rit == s.rend()) {
        return std::nullopt;
    }
    auto const off_from_end = static_cast<size_t>(std::distance(s.rbegin(), rit));
    return detail::offset_to_hdl_coord(s.range(), s.range().length() - 1 - off_from_end);
}

template <typename ArrayT>
class ArraySlice;
template <typename ArrayT, Range R>
class StaticArraySlice;

namespace detail {

// We split out the implementations into a base class so that we can reuse then in the
// Logic/Bit-aware specializations in logic_array.hpp. LogicArray is just an Array with
// extra members. BitArray is too, for now...

template <typename ArrayT>
class ArraySliceImpl {
  public:
    using value_type = std::ranges::range_value_t<ArrayT>;
    using reference = std::ranges::range_reference_t<ArrayT>;
    using index_type = Range::value_type;
    using iterator = std::ranges::iterator_t<ArrayT>;
    static_assert(!std::is_reference_v<value_type>);

    constexpr ArraySliceImpl() = delete;
    constexpr ArraySliceImpl(ArraySliceImpl const&) = default;
    constexpr ArraySliceImpl(ArraySliceImpl&&) = default;
    constexpr ArraySliceImpl(ArrayT* arr, Range range) noexcept
        : arr_(arr), range_(range), begin_(compute_begin(arr, range)) {}

    constexpr Range const& range() const noexcept { return range_; }
    constexpr size_t size() const noexcept { return range_.length(); }

    // Element access bounds-checks against this slice's range_, not the owner's range.
    constexpr reference operator[](index_type idx) const {
        if (range_.direction == Direction::TO) {
            if (idx < range_.left || idx > range_.right) {
                throw std::out_of_range("Index out of bounds");
            }
            return *(begin_ + (idx - range_.left));
        } else {
            if (idx > range_.left || idx < range_.right) {
                throw std::out_of_range("Index out of bounds");
            }
            return *(begin_ + (range_.left - idx));
        }
    }

    // Slicing a slice flattens to a new ArraySlice of the same underlying array.
    // Validity is checked against the slice's range, not the underlying owner's range. This
    // returns the outer ArraySlice type so that we pick up the Logic and Bit
    // specializations.
    constexpr ArraySlice<ArrayT> operator[](Range r) const {
        detail::subsequence_check(range_, r);
        return ArraySlice<ArrayT>(arr_, r);
    }
#if __cplusplus >= 202302L
    constexpr ArraySlice<ArrayT> operator[](
        Range::value_type left, Range::value_type right
    ) const {
        return (*this)[Range{left, range_.direction, right}];
    }
    constexpr ArraySlice<ArrayT> operator[](
        Range::value_type left, Direction dir, Range::value_type right
    ) const {
        return (*this)[Range{left, dir, right}];
    }
#endif

    template <Range R>
    constexpr StaticArraySlice<ArrayT, R> slice() const {
        detail::subsequence_check(range_, R);
        return StaticArraySlice<ArrayT, R>(arr_);
    }

    template <index_type I>
    constexpr reference index() const {
        return (*this)[I];
    }

    // Have to override this since the implicitly generated copy assignment acts as a
    // variable assignment rather than writing through to the underlying storage.
    constexpr ArraySliceImpl const& operator=(ArraySliceImpl const& other) const
        requires(!std::is_const_v<ArrayT>)
    {
        if (other.range_.length() != range_.length()) {
            throw std::invalid_argument(
                "Value of length " + std::to_string(other.range_.length())
                + " cannot be assigned to array slice of length "
                + std::to_string(range_.length())
            );
        }
        std::ranges::copy(other, begin());
        return *this;
    }

    template <detail::sized_input_range R>
        requires(!std::is_const_v<ArrayT>)
             && std::convertible_to<std::ranges::range_value_t<R>, value_type>
    constexpr ArraySliceImpl const& operator=(R const& obj) const {
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
    constexpr ArraySliceImpl const& operator=(std::initializer_list<T> init) const {
        if (init.size() != range_.length()) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " cannot be assigned to array slice of length "
                + std::to_string(range_.length())
            );
        }
        std::ranges::copy(init, begin());
        return *this;
    }

    constexpr iterator begin() const noexcept { return begin_; }
    constexpr iterator end() const noexcept { return begin_ + range_.length(); }
    constexpr auto rbegin() const noexcept { return std::reverse_iterator(end()); }
    constexpr auto rend() const noexcept { return std::reverse_iterator(begin()); }

  private:
    // Cache the begin pointer. This is just one stack pointer for a temporary object, and
    // it saves recomputing the offset on every element access.
    static constexpr iterator compute_begin(ArrayT* arr, Range range) noexcept {
        // Null slices may carry bounds outside the parent's range (the
        // validity rule allows that). Pin them to the parent's begin so
        // begin()/end() form a well-formed empty iterator pair.
        if (range.length() == 0) {
            return arr->begin();
        }
        auto start = find(arr->range(), range.left);
        assert(
            start != arr->range().end()
            && "slice range not a sub-range of the owner's range"
        );
        return arr->begin() + std::distance(arr->range().begin(), start);
    }

    ArrayT* arr_;
    Range range_;
    iterator begin_;
};

template <typename ArrayT, Range R>
class StaticArraySliceImpl {
  public:
    using value_type = std::ranges::range_value_t<ArrayT>;
    using reference = std::ranges::range_reference_t<ArrayT>;
    using index_type = Range::value_type;
    using iterator = std::ranges::iterator_t<ArrayT>;
    static_assert(!std::is_reference_v<value_type>);

    constexpr StaticArraySliceImpl() = delete;
    constexpr StaticArraySliceImpl(StaticArraySliceImpl const&) = default;
    constexpr StaticArraySliceImpl(StaticArraySliceImpl&&) = default;
    constexpr explicit StaticArraySliceImpl(ArrayT* arr) noexcept : arr_(arr) {}

    // The range, exposed two ways: `static_range` for type-level access
    // (`T::static_range`, used by the StaticRangedSequence concept), and a
    // plain constexpr `range()` member matching ArraySlice's instance
    // accessor so generic code can write `obj.range()` uniformly across both.
    static constexpr Range static_range = R;
    constexpr Range range() const noexcept { return static_range; }
    constexpr size_t size() const noexcept { return static_range.length(); }

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

    template <Range R2>
    constexpr StaticArraySlice<ArrayT, R2> slice() const {
        static_assert(
            R2.is_subsequence_of(R),
            "static sub-slice range is not a sub-range of the parent slice"
        );
        return StaticArraySlice<ArrayT, R2>(arr_);
    }

    template <index_type I>
    constexpr reference index() const {
        static_assert(find(R, I) != R.end(), "index is out of range");
        return (*this)[I];
    }

    constexpr ArraySlice<ArrayT> operator[](Range r) const {
        detail::subsequence_check(R, r);
        return ArraySlice<ArrayT>(arr_, r);
    }
#if __cplusplus >= 202302L
    constexpr ArraySlice<ArrayT> operator[](
        Range::value_type left, Range::value_type right
    ) const {
        // StaticArraySliceImpl has a compile-time range R, not a runtime range_ member,
        // so the direction comes from the template parameter.
        return (*this)[Range{left, R.direction, right}];
    }
    constexpr ArraySlice<ArrayT> operator[](
        Range::value_type left, Direction dir, Range::value_type right
    ) const {
        return (*this)[Range{left, dir, right}];
    }
#endif

    // Have to override this since the implicitly generated copy assignment acts as a
    // variable assignment rather than writing through to the underlying storage.
    constexpr StaticArraySliceImpl const& operator=(StaticArraySliceImpl const& other) const
        requires(!std::is_const_v<ArrayT>)
    {
        std::ranges::copy(other, begin());
        return *this;
    }

    template <detail::sized_input_range Rng>
        requires(!std::is_const_v<ArrayT>)
             && std::convertible_to<std::ranges::range_value_t<Rng>, value_type>
    constexpr StaticArraySliceImpl const& operator=(Rng const& obj) const {
        if constexpr (StaticRangedSequence<Rng>) {
            static_assert(
                std::remove_cvref_t<Rng>::static_range.length() == R.length(),
                "static-length RHS does not match the slice length"
            );
        } else if (std::ranges::size(obj) != R.length()) {
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
    constexpr StaticArraySliceImpl const& operator=(std::initializer_list<T> init) const {
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
        // We don't cache the begin pointer here since we can conditionally leverage the
        // compile-time range to fold the offset to a constant.

        // Null slices may carry bounds outside the parent's range (the
        // validity rule allows that). Pin them to the parent's begin so
        // begin()/end() form a well-formed empty iterator pair.
        if constexpr (R.length() == 0) {
            return arr_->begin();
        } else if constexpr (StaticRangedSequence<ArrayT>) {
            // Parent's range is a compile-time constant; fold the offset to
            // a constant instead of recomputing it via find()/distance().
            constexpr auto parent = std::remove_cvref_t<ArrayT>::static_range;
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

}  // namespace detail

// A non-owning view (like std::span with dynamic extent) with a runtime range. Element
// mutability is determined by ArrayT's constness:
// - ArraySlice<ArrayT>          -- mutable view, can write elements.
// - ArraySlice<const ArrayT>    -- read-only view.
// - const ArraySlice<X>         -- pointer/range fixed; element access
//                                     follows X's mutability rules.
template <typename ArrayT>
class ArraySlice : public detail::ArraySliceImpl<ArrayT> {
  public:
    using detail::ArraySliceImpl<ArrayT>::ArraySliceImpl;
    using detail::ArraySliceImpl<ArrayT>::operator=;
};

// A non-owning view (like std::span) with a compile-time range. Element mutability is
// determined by ArrayT's constness:
// - StaticArraySlice<ArrayT>          -- mutable view, can write elements.
// - StaticArraySlice<const ArrayT>    -- read-only view.
// - const StaticArraySlice<X>         -- pointer/range fixed; element access
//                                  follows X's mutability rules.
template <typename ArrayT, Range R>
class StaticArraySlice : public detail::StaticArraySliceImpl<ArrayT, R> {
  public:
    using detail::StaticArraySliceImpl<ArrayT, R>::StaticArraySliceImpl;
    using detail::StaticArraySliceImpl<ArrayT, R>::operator=;
};

namespace detail {

template <typename ArrayT>
struct is_array<ArraySlice<ArrayT>> : std::true_type {};

template <typename ArrayT, Range R>
struct is_array<StaticArraySlice<ArrayT, R>> : std::true_type {};

// Emit "<prefix>[range]{e0, e1, ...}". Used by the per-type std::formatter
// specializations for Array/Vector/ArraySlice/StaticArraySlice on non-logic
// element types. Logic/Bit arrays use a quoted-bit-string body instead; see
// logic_array.hpp.
template <RangedSequence ArrayT, typename OutIt>
    requires Formattable<std::ranges::range_value_t<ArrayT>>
OutIt format_array(std::string_view prefix, ArrayT const& arr, OutIt out) {
    out = std::format_to(out, "{}{}{{", prefix, arr.range());
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

// Formatters for ArraySlice and StaticArraySlice. Both print with the
// "ArraySlice" prefix; the static-vs-runtime distinction isn't useful to a
// reader of the printed output. The Logic/Bit specializations in
// logic_array.hpp are more-specialized partial specs and win when both
// headers are visible.
#define COCONEXT_DEFINE_ARRAY_SLICE_FORMATTER(...)                                         \
    struct std::formatter<__VA_ARGS__> {                                                   \
        constexpr auto parse(std::format_parse_context& ctx) {                             \
            auto it = ctx.begin();                                                         \
            if (it != ctx.end() && *it != '}') {                                           \
                throw std::format_error("ArraySlice formatter takes no format spec");      \
            }                                                                              \
            return it;                                                                     \
        }                                                                                  \
        auto format(__VA_ARGS__ const& s, std::format_context& ctx) const {                \
            return coconext::types::detail::format_array("ArraySlice", s, ctx.out());      \
        }                                                                                  \
    }

template <typename ArrayT>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
COCONEXT_DEFINE_ARRAY_SLICE_FORMATTER(coconext::types::ArraySlice<ArrayT>);

template <typename ArrayT, coconext::types::Range R>
    requires coconext::types::detail::Formattable<std::ranges::range_value_t<ArrayT>>
COCONEXT_DEFINE_ARRAY_SLICE_FORMATTER(coconext::types::StaticArraySlice<ArrayT, R>);

#undef COCONEXT_DEFINE_ARRAY_SLICE_FORMATTER

#endif  // COCONEXT_ARRAY_BASE_HPP

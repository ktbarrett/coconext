#ifndef COCONEXT_DETAIL_ARRAY_BASE_HPP
#define COCONEXT_DETAIL_ARRAY_BASE_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include <coconext/types/detail/array_fwd.hpp>
#include <coconext/types/detail/concepts.hpp>
#include <coconext/types/detail/hash.hpp>
#include <coconext/types/range.hpp>

namespace coconext::types {

// -- Concepts -----------------------------------------------------------------

namespace detail {

template <typename R>
concept sized_input_range = std::ranges::sized_range<R> && std::ranges::input_range<R>;

// Helper to force a value into a constant-expression context.
template <auto V>
struct require_constant {};

}  // namespace detail

// A random-access range whose range is queryable via `obj.range()`. The
// optional `Elem` template parameter constrains the element type: `Elem = void`
// (the default) matches any element type; a concrete type requires the element
// to match exactly. So `RangedSequence T` matches any ranged sequence,
// `RangedSequence<Logic> T` matches only ranged sequences of Logic.
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

// -- Slice types --------------------------------------------------------------

namespace detail {

constexpr void subsequence_check(Range parent, Range child) {
    if (!child.is_subsequence_of(parent)) {
        throw std::invalid_argument("Range is not a valid sub-range of the parent");
    }
}

}  // namespace detail

namespace detail {

// We split out the implementations into a base class so that we can reuse then in the
// Logic/Bit-aware specializations. LogicArray is just an Array with
// extra members. BitArray is too, for now...

template <typename ArrayT>
class ArraySliceImpl {
  public:
    auto reverse() const;

    using value_type = std::ranges::range_value_t<ArrayT>;
    using reference = std::ranges::range_reference_t<ArrayT>;
    using index_type = Range::value_type;
    using iterator = std::ranges::iterator_t<ArrayT>;
    static_assert(!std::is_reference_v<value_type>);

    constexpr ArraySliceImpl() = delete;
    constexpr ArraySliceImpl(ArraySliceImpl const&) = default;
    constexpr ArraySliceImpl(ArraySliceImpl&&) = default;
    constexpr ArraySliceImpl(ArrayT* arr, Range range) noexcept
        : arr_(arr), range_(range) {}

    constexpr Range const& range() const noexcept { return range_; }
    constexpr size_t size() const noexcept { return range_.length(); }

    // Element access bounds-checks against this slice's range_, not the owner's range.
    constexpr std::optional<Range::value_type> index_of(value_type const& v) const {
        auto const it = std::ranges::find(*this, v);
        if (it == end()) {
            return std::nullopt;
        }
        return range()[static_cast<size_t>(std::ranges::distance(begin(), it))];
    }

    constexpr std::optional<Range::value_type> rindex_of(value_type const& v) const {
        auto const it = std::find(rbegin(), rend(), v);
        if (it == rend()) {
            return std::nullopt;
        }
        return range()
            [size() - 1 - static_cast<size_t>(std::ranges::distance(rbegin(), it))];
    }

    constexpr reference operator[](index_type idx) const {
        auto const offset = range_.offset_of(idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        return *(begin() + offset.value());
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

    constexpr iterator begin() const noexcept {
        if (range_.length() == 0) {
            return arr_->begin();
        }
        return arr_->begin() + arr_->range().offset_of(range_.left).value();
    }
    constexpr iterator end() const noexcept {
        if (range_.length() == 0) {
            return arr_->begin();
        }
        return arr_->begin() + arr_->range().offset_of(range_.right).value() + 1;
    }
    constexpr auto rbegin() const noexcept { return std::reverse_iterator(end()); }
    constexpr auto rend() const noexcept { return std::reverse_iterator(begin()); }

  private:
    ArrayT* arr_;
    Range range_;
};

template <typename ArrayT, Range R>
class StaticArraySliceImpl {
  public:
    constexpr auto reverse() const;

    using value_type = std::ranges::range_value_t<ArrayT>;
    using reference = std::ranges::range_reference_t<ArrayT>;
    using index_type = Range::value_type;
    using iterator = std::ranges::iterator_t<ArrayT>;
    static_assert(!std::is_reference_v<value_type>);

    constexpr StaticArraySliceImpl() = delete;
    constexpr StaticArraySliceImpl(StaticArraySliceImpl const&) = default;
    constexpr StaticArraySliceImpl(StaticArraySliceImpl&&) = default;
    constexpr explicit StaticArraySliceImpl(ArrayT* arr) noexcept : arr_(arr) {}

    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return static_range; }
    static constexpr size_t size() noexcept { return static_range.length(); }

    constexpr std::optional<Range::value_type> index_of(value_type const& v) const {
        auto const it = std::ranges::find(*this, v);
        if (it == end()) {
            return std::nullopt;
        }
        return range()[static_cast<size_t>(std::ranges::distance(begin(), it))];
    }

    constexpr std::optional<Range::value_type> rindex_of(value_type const& v) const {
        auto const it = std::find(rbegin(), rend(), v);
        if (it == rend()) {
            return std::nullopt;
        }
        return range()
            [size() - 1 - static_cast<size_t>(std::ranges::distance(rbegin(), it))];
    }

    constexpr reference operator[](index_type idx) const {
        auto const offset = R.offset_of(idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        return *(begin() + offset.value());
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
        static_assert(R.contains(I), "index is out of range");
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
        if constexpr (R.length() == 0) {
            return arr_->begin();
        } else if constexpr (StaticRangedSequence<ArrayT>) {
            constexpr auto parent = std::remove_cvref_t<ArrayT>::static_range;
            constexpr auto offset = parent.offset_of(R.left).value();
            return arr_->begin() + offset;
        } else {
            return arr_->begin() + arr_->range().offset_of(R.left).value();
        }
    }
    constexpr iterator end() const noexcept {
        if constexpr (R.length() == 0) {
            return arr_->begin();
        } else if constexpr (StaticRangedSequence<ArrayT>) {
            constexpr auto parent = std::remove_cvref_t<ArrayT>::static_range;
            constexpr auto offset = parent.offset_of(R.right).value();
            return arr_->begin() + offset + 1;
        } else {
            return arr_->begin() + arr_->range().offset_of(R.right).value() + 1;
        }
    }
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

template <typename ArrayT>
auto detail::ArraySliceImpl<ArrayT>::reverse() const {
    Vector<std::ranges::range_value_t<ArrayT>> result(range().reverse());
    std::ranges::reverse_copy(*this, result.begin());
    return result;
}

template <typename ArrayT, Range R>
constexpr auto detail::StaticArraySliceImpl<ArrayT, R>::reverse() const {
    detail::Array<std::ranges::range_value_t<ArrayT>, R.reverse()> result;
    std::ranges::reverse_copy(*this, result.begin());
    return result;
}

namespace detail {

// Equality and hashing apply to every Array and Vector specialization.
template <typename T, Range R>
    requires std::equality_comparable<T>
constexpr bool operator==(Array<T, R> const& lhs, Array<T, R> const& rhs) noexcept {
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <auto V>
constexpr bool fits_range_value_type = [] {
    if constexpr (NativeInteger<decltype(V)>) {
        return std::in_range<Range::value_type>(V);
    } else {
        return false;
    }
}();

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
            std::is_same_v<First, Range> || fits_range_value_type<get<0>(t)>,
            "single template arg must be a Range value or an integer length that fits in "
            "Range::value_type (int64_t)"
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
            "Array<T, L, H>: L must be an integer that fits in Range::value_type (int64_t)"
        );
        static_assert(
            fits_range_value_type<get<1>(t)>,
            "Array<T, L, H>: H must be an integer that fits in Range::value_type (int64_t)"
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
            "Array<T, L, D, H>: L must be an integer that fits in Range::value_type "
            "(int64_t)"
        );
        static_assert(
            fits_range_value_type<get<2>(t)>,
            "Array<T, L, D, H>: H must be an integer that fits in Range::value_type "
            "(int64_t)"
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

template <typename ValueT>
    requires std::equality_comparable<ValueT>
constexpr bool operator==(Vector<ValueT> const& lhs, Vector<ValueT> const& rhs) noexcept {
    if (lhs.range() != rhs.range()) {
        return false;
    }
    for (auto it1 = lhs.begin(), it2 = rhs.begin(); it1 != lhs.end(); ++it1, ++it2) {
        if (!(*it1 == *it2)) {
            return false;
        }
    }
    return true;
}

namespace detail {

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

template <typename T, coconext::types::Range R>
struct std::hash<coconext::types::detail::Array<T, R>> {
    size_t operator()(coconext::types::detail::Array<T, R> const& arr) const noexcept {
        size_t seed = typeid(coconext::types::detail::Array<T, R>).hash_code();
        for (auto const& elem : arr) {
            seed = coconext::types::detail::hash_combine(seed, elem);
        }
        return seed;
    }
};

template <typename T>
struct std::hash<coconext::types::Vector<T>> {
    size_t operator()(coconext::types::Vector<T> const& arr) const noexcept {
        size_t seed = typeid(coconext::types::Vector<T>).hash_code();
        seed = coconext::types::detail::hash_mix(
            seed, hash<coconext::types::Range>{}(arr.range())
        );
        for (auto const& elem : arr) {
            seed = coconext::types::detail::hash_combine(seed, elem);
        }
        return seed;
    }
};

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

#endif  // COCONEXT_DETAIL_ARRAY_BASE_HPP

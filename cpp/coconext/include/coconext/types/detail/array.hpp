#ifndef COCONEXT_DETAIL_ARRAY_HPP
#define COCONEXT_DETAIL_ARRAY_HPP

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <coconext/types/detail/array_base.hpp>
#include <coconext/types/detail/concepts.hpp>
#include <coconext/types/detail/hash.hpp>
#include <coconext/types/range.hpp>

namespace coconext::types {

namespace detail {

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

    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return static_range; }
    static constexpr size_t size() noexcept { return static_range.length(); }

    constexpr auto reverse() const {
        Array<T, R.reverse()> result;
        std::ranges::reverse_copy(*this, result.begin());
        return result;
    }

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

    constexpr reference operator[](index_type idx) {
        auto offset = R.offset_of(idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        return data_[offset.value()];
    }
    constexpr const_reference operator[](index_type idx) const {
        auto offset = R.offset_of(idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        return data_[offset.value()];
    }

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
        static_assert(R.contains(I), "index is out of range");
        return (*this)[I];
    }
    template <index_type I>
    constexpr const_reference index() const {
        static_assert(R.contains(I), "index is out of range");
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
    std::array<value_type, R.length()> data_;
};

// Local-allocated, compile-time-bounded, type-generic array indexed according to its Range.
// This is the canonical "Array" implementation. Stuck here so coconext::types::Array can be
// the variadic template.
template <typename T, Range R>
class Array : public ArrayImpl<T, R> {
  public:
    using ArrayImpl<T, R>::ArrayImpl;
    using ArrayImpl<T, R>::operator=;
};

}  // namespace detail

}  // namespace coconext::types

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

#endif  // COCONEXT_DETAIL_ARRAY_HPP

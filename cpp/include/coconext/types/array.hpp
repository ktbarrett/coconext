#ifndef COCONEXT_ARRAY_HPP
#define COCONEXT_ARRAY_HPP

#include <algorithm>
#include <coconext/types/concepts.hpp>
#include <coconext/types/range.hpp>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

// std::unique_ptr's constexpr support landed in C++23 (P2273R3); under C++20
// the constexpr keyword on Array's members is still legal but evaluating those
// members in a constant expression fails. Gate accordingly.
#if __cplusplus >= 202302L
#define COCONEXT_ARRAY_CONSTEXPR constexpr
#else
#define COCONEXT_ARRAY_CONSTEXPR
#endif

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
    // - ArraySlice<Array<T>>          -- mutable view, can write elements.
    // - ArraySlice<const Array<T>>    -- read-only view.
    // - const ArraySlice<X>           -- pointer/range fixed; element access
    //                                    follows X's mutability rules.
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

template <typename ValueT>
class Array {
  public:
    using value_type = ValueT;
    static_assert(!std::is_reference_v<value_type>);
    static_assert(!std::is_const_v<value_type>);
    using index_type = Range::value_type;
    using reference = value_type&;
    using const_reference = value_type const&;
    using iterator = value_type*;
    using const_iterator = value_type const*;

    constexpr Array() = delete;  // no default constructor

    COCONEXT_ARRAY_CONSTEXPR Array(Array&& other) noexcept = default;

    COCONEXT_ARRAY_CONSTEXPR Array(Array const& other)
        : data_(std::make_unique_for_overwrite<value_type[]>(other.range_.length())),
          range_(other.range_) {
        std::ranges::copy(other, data_.get());
    }

    // Weird but valid. const on members is semantically different than const on
    // the whole object. Assignment is not valid on const variables (storage),
    // but const on members (not storage, semantics) really just describes the
    // behavioral intent and derived constness when operating on those fields,
    // so assignment is still sound and the language lets us do this. The
    // const_cast is required because libc++ insists on a non-const pointer
    // for std::destroy_at / std::construct_at.
    COCONEXT_ARRAY_CONSTEXPR Array& operator=(Array const& other) {
        if (this != &other) {
            auto buf = std::make_unique_for_overwrite<value_type[]>(other.range_.length());
            std::ranges::copy(other, buf.get());
            data_ = std::move(buf);
            std::destroy_at(const_cast<Range*>(&range_));
            std::construct_at(const_cast<Range*>(&range_), other.range_);
        }
        return *this;
    }

    COCONEXT_ARRAY_CONSTEXPR Array& operator=(Array&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            std::destroy_at(const_cast<Range*>(&range_));
            std::construct_at(const_cast<Range*>(&range_), other.range_);
        }
        return *this;
    }

    explicit COCONEXT_ARRAY_CONSTEXPR Array(Range range)
        : data_(std::make_unique<value_type[]>(range.length())), range_(range) {}

    COCONEXT_ARRAY_CONSTEXPR Array(std::initializer_list<value_type> init)
        : data_(std::make_unique_for_overwrite<value_type[]>(init.size())),
          range_(init.size()) {
        std::ranges::copy(init, data_.get());
    }

    COCONEXT_ARRAY_CONSTEXPR Array(std::initializer_list<value_type> init, Range range)
        : range_(range) {
        if (init.size() != range.length()) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " does not match range length " + std::to_string(range.length())
            );
        }
        data_ = std::make_unique_for_overwrite<value_type[]>(range.length());
        std::ranges::copy(init, data_.get());
    }

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, value_type>
                  && (!std::same_as<std::remove_cvref_t<R>, Array>)
    explicit COCONEXT_ARRAY_CONSTEXPR Array(R const& obj)
        : data_(std::make_unique_for_overwrite<value_type[]>(std::ranges::size(obj))),
          range_(std::ranges::size(obj)) {
        std::ranges::copy(obj, data_.get());
    }

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, value_type>
    COCONEXT_ARRAY_CONSTEXPR Array(R const& obj, Range range) : range_(range) {
        if (std::ranges::size(obj) != range.length()) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(std::ranges::size(obj))
                + " does not match range length " + std::to_string(range.length())
            );
        }
        data_ = std::make_unique_for_overwrite<value_type[]>(range.length());
        std::ranges::copy(obj, data_.get());
    }

    constexpr Range const& range() const noexcept { return range_; }

    COCONEXT_ARRAY_CONSTEXPR reference operator[](index_type idx) {
        auto it = find(range_, idx);
        if (it == range_.end()) {
            throw std::out_of_range("Index out of bounds");
        }
        return *(data_.get() + std::distance(range_.begin(), it));
    }
    COCONEXT_ARRAY_CONSTEXPR const_reference operator[](index_type idx) const {
        auto it = find(range_, idx);
        if (it == range_.end()) {
            throw std::out_of_range("Index out of bounds");
        }
        return *(data_.get() + std::distance(range_.begin(), it));
    }
    COCONEXT_ARRAY_CONSTEXPR ArraySlice<Array> operator[](Range r) {
        detail::subsequence_check(range_, r);
        return ArraySlice<Array>(this, r);
    }
    COCONEXT_ARRAY_CONSTEXPR ArraySlice<Array const> operator[](Range r) const {
        detail::subsequence_check(range_, r);
        return ArraySlice<Array const>(this, r);
    }

    COCONEXT_ARRAY_CONSTEXPR iterator begin() noexcept { return data_.get(); }
    COCONEXT_ARRAY_CONSTEXPR const_iterator begin() const noexcept { return data_.get(); }
    COCONEXT_ARRAY_CONSTEXPR iterator end() noexcept {
        return data_.get() + range_.length();
    }
    COCONEXT_ARRAY_CONSTEXPR const_iterator end() const noexcept {
        return data_.get() + range_.length();
    }
    COCONEXT_ARRAY_CONSTEXPR auto rbegin() noexcept { return std::reverse_iterator(end()); }
    COCONEXT_ARRAY_CONSTEXPR auto rbegin() const noexcept {
        return std::reverse_iterator(end());
    }
    COCONEXT_ARRAY_CONSTEXPR auto rend() noexcept { return std::reverse_iterator(begin()); }
    COCONEXT_ARRAY_CONSTEXPR auto rend() const noexcept {
        return std::reverse_iterator(begin());
    }

  private:
    std::unique_ptr<value_type[]> data_;
    Range const range_;
};

template <typename ValueT>
    requires std::equality_comparable<ValueT>
constexpr bool operator==(Array<ValueT> const& lhs, Array<ValueT> const& rhs) noexcept {
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

template <RangedSequence ArrayT>
    requires detail::Stringifiable<std::ranges::range_value_t<ArrayT>>
std::string to_string(ArrayT const& arr) {
    using std::to_string;  // make std::to_string and ADL candidates co-visible
    std::string result = "Array([";
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        result += to_string(*it);
        if (std::next(it) != arr.end()) {
            result += ", ";
        }
    }
    result += "], ";
    result += to_string(arr.range());
    result += ")";
    return result;
}

static_assert(RangedSequence<Array<int>>);
static_assert(RangedSequence<Array<int> const>);
static_assert(RangedSequence<ArraySlice<Array<int>>>);
static_assert(RangedSequence<ArraySlice<Array<int> const>>);

}  // namespace coconext::types

namespace std {
template <typename T>
struct hash<coconext::types::Array<T>> {
    size_t operator()(coconext::types::Array<T> const& arr) const noexcept {
        size_t seed = hash<coconext::types::Range>{}(arr.range());
        for (auto const& elem : arr) {
            seed = coconext::types::detail::hash_combine(seed, elem);
        }
        return seed;
    }
};
}  // namespace std

#undef COCONEXT_ARRAY_CONSTEXPR

#endif  // COCONEXT_ARRAY_HPP

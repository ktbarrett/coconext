#ifndef COCONEXT_ARRAY_HPP
#define COCONEXT_ARRAY_HPP

#include <algorithm>
#include <coconext/types/concepts.hpp>
#include <coconext/types/range.hpp>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace coconext::types {

namespace detail {

template <typename R>
concept sized_input_range =
    std::ranges::sized_range<R> && std::ranges::input_range<R>;

}  // namespace detail

// The minimum a type must expose so that index() and slice() can be
// implemented for it externally.
template <typename T>
concept RangedSequence = std::ranges::random_access_range<T> && requires(T& t) {
    { t.range() } -> std::convertible_to<Range>;
};

template <typename ArrayT>
class ArraySlice;

template <RangedSequence ArrayT>
constexpr std::ranges::range_reference_t<ArrayT> index(ArrayT& arr,
                                                       Range::value_type idx) {
    auto find_idx = find(arr.range(), idx);
    if (find_idx == arr.range().end()) {
        throw std::out_of_range("Index out of bounds");
    }
    auto offset = std::distance(arr.range().begin(), find_idx);
    return *(arr.begin() + offset);
}

template <RangedSequence ArrayT>
constexpr auto slice(ArrayT& arr, Range::value_type start,
                     Range::value_type end) {
    auto left_it = find(arr.range(), start);
    if (left_it == arr.range().end()) {
        throw std::out_of_range("slice start out of bounds");
    }
    auto right_it = find(arr.range(), end);
    if (right_it == arr.range().end()) {
        throw std::out_of_range("slice end out of bounds");
    }
    if (std::distance(arr.range().begin(), left_it) >
        std::distance(arr.range().begin(), right_it)) {
        throw std::invalid_argument(
            "Slice direction does not match array range direction");
    }
    return ArraySlice(&arr, Range(start, arr.range().direction(), end));
}

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

public:  // constructor
    constexpr ArraySlice() = delete;
    constexpr ArraySlice(const ArraySlice&) = default;
    constexpr ArraySlice(ArraySlice&&) = default;

    constexpr ArraySlice(ArrayT* arr, Range range) noexcept
        : arr_(arr), range_(range) {}

public:  // attributes
    constexpr const Range& range() const noexcept { return range_; }

public:  // indexing and slicing
    constexpr reference operator[](index_type idx) const {
        return index(*arr_, idx);
    }

    constexpr ArraySlice operator()(index_type start, index_type end) const {
        // This is different than slice() since we want to flatten nested slices
        // to prevent a bunch of chained accesses and lifetime issues.
        auto left_it = find(range_, start);
        if (left_it == range_.end()) {
            throw std::out_of_range("slice start out of bounds");
        }
        auto right_it = find(range_, end);
        if (right_it == range_.end()) {
            throw std::out_of_range("slice end out of bounds");
        }
        if (std::distance(range_.begin(), left_it) >
            std::distance(range_.begin(), right_it)) {
            throw std::invalid_argument(
                "Slice direction does not match array range direction");
        }
        return ArraySlice(arr_, Range(start, range_.direction(), end));
    }
#if __cplusplus >= 202302L
    constexpr ArraySlice operator[](index_type start, index_type end) const {
        return this->operator()(start, end);
    }
#endif

public:  // assignment
    template <detail::sized_input_range R>
        requires(!std::is_const_v<ArrayT>) &&
                std::convertible_to<std::ranges::range_value_t<R>, value_type>
    constexpr const ArraySlice& operator=(const R& obj) const {
        if (std::ranges::size(obj) != range_.length()) {
            throw std::invalid_argument(
                "Value of length " + std::to_string(std::ranges::size(obj)) +
                " cannot be assigned to array slice of length " +
                std::to_string(range_.length()));
        }
        std::ranges::copy(obj, begin());
        return *this;
    }

    template <typename T>
        requires(!std::is_const_v<ArrayT>) && std::convertible_to<T, value_type>
    constexpr const ArraySlice& operator=(std::initializer_list<T> init) const {
        if (init.size() != static_cast<size_t>(range_.length())) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size()) +
                " cannot be assigned to array slice of length " +
                std::to_string(range_.length()));
        }
        std::ranges::copy(init, begin());
        return *this;
    }

public:  // iterators
    constexpr iterator begin() const noexcept {
        auto start = find(arr_->range(), range_.left());
        assert(start != arr_->range().end() &&
               "slice range not a sub-range of the owner's range");
        return arr_->begin() + std::distance(arr_->range().begin(), start);
    }
    constexpr iterator end() const noexcept {
        return begin() + range_.length();
    }
    constexpr auto rbegin() const noexcept {
        return std::reverse_iterator(end());
    }
    constexpr auto rend() const noexcept {
        return std::reverse_iterator(begin());
    }

private:
    ArrayT* arr_;
    Range range_;
};

template <typename ValueT>
class Array {
public:
    using value_type = ValueT;
    static_assert(!std::is_reference_v<value_type>);
    static_assert(!std::is_const_v<value_type>,
                  "Array<const T> is not supported (std::vector forbids const "
                  "elements). Use `const Array<T>` for an immutable array.");
    using index_type = Range::value_type;

public:
    constexpr Array() = delete;  // no default constructor

public:  // ensure these are constexpr
    constexpr Array(Array&& other) = default;
    constexpr Array(const Array& other) = default;
    constexpr Array& operator=(const Array& other) = default;
    constexpr Array& operator=(Array&& other) = default;

public:  // construct with just range
    explicit constexpr Array(Range range)
        requires std::default_initializable<value_type>
        : data_(), range_(range) {
        data_.resize(range.length());
    }

    explicit constexpr Array(size_t length)
        requires std::default_initializable<value_type>
        : Array(Range(length)) {}

public:  // constructor from initializer list
    template <typename T>
        requires std::convertible_to<T, value_type>
    explicit constexpr Array(std::initializer_list<T> init)
        : data_(init), range_(data_.size()) {}

public:  // move from vector
    template <typename T>
        requires std::convertible_to<T, value_type>
    explicit constexpr Array(std::vector<T>&& vec)
        : data_(std::move(vec)), range_(data_.size()) {}

public:  // construct from range
    template <std::ranges::input_range T>
        requires std::convertible_to<std::ranges::range_value_t<T>, value_type>
    explicit constexpr Array(const T& obj, Range range)
        : data_(), range_(range) {
        data_.reserve(range.length());
        data_.assign(std::ranges::begin(obj), std::ranges::end(obj));
        if (data_.size() != range.length()) {
            throw std::invalid_argument("Input of size " +
                                        std::to_string(data_.size()) +
                                        " does not match range length " +
                                        std::to_string(range.length()));
        }
    }
    template <std::ranges::input_range T>
        requires std::convertible_to<std::ranges::range_value_t<T>, value_type>
    explicit constexpr Array(const T& obj, size_t length)
        : data_(), range_(length) {
        data_.reserve(length);
        data_.assign(std::ranges::begin(obj), std::ranges::end(obj));
        if (data_.size() != length) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(data_.size()) +
                " does not match range length " + std::to_string(length));
        }
    }

public:  // construct from iterator
    template <std::input_iterator It>
        requires std::convertible_to<
                     typename std::iterator_traits<It>::value_type, value_type>
    explicit constexpr Array(It begin, It end, Range range)
        : data_(), range_(range) {
        data_.reserve(range.length());
        data_.assign(begin, end);
        if (data_.size() != range.length()) {
            throw std::invalid_argument("Iterator of size " +
                                        std::to_string(data_.size()) +
                                        " does not match range length " +
                                        std::to_string(range.length()));
        }
    }
    template <std::input_iterator It>
        requires std::convertible_to<
                     typename std::iterator_traits<It>::value_type, value_type>
    explicit constexpr Array(It begin, It end, size_t length)
        : data_(), range_(length) {
        data_.reserve(length);
        data_.assign(begin, end);
        if (data_.size() != length) {
            throw std::invalid_argument(
                "Iterator of size " + std::to_string(data_.size()) +
                " does not match range length " + std::to_string(length));
        }
    }

public:  // attributes
    constexpr const Range& range() const noexcept { return range_; }
    constexpr void set_range(const Range& range) {
        if (range.length() != this->range().length()) {
            throw std::invalid_argument(
                "New range length " + std::to_string(range.length()) +
                " does not match current range length " +
                std::to_string(this->range().length()));
        }
        range_ = range;
    }

public:  // indexing and slicing
    constexpr value_type& operator[](index_type idx) {
        return index(*this, idx);
    }
    constexpr const value_type& operator[](index_type idx) const {
        return index(*this, idx);
    }
    constexpr auto operator()(index_type start, index_type end) {
        return slice(*this, start, end);
    }
    constexpr auto operator()(index_type start, index_type end) const {
        return slice(*this, start, end);
    }
#if __cplusplus >= 202302L
    constexpr auto operator[](index_type start, index_type stop) {
        return slice(*this, start, stop);
    }
    constexpr auto operator[](index_type start, index_type stop) const {
        return slice(*this, start, stop);
    }
#endif

public:  // iterators
    constexpr auto begin() noexcept { return data_.begin(); }
    constexpr auto begin() const noexcept { return data_.begin(); }
    constexpr auto end() noexcept { return data_.end(); }
    constexpr auto end() const noexcept { return data_.end(); }
    constexpr auto rbegin() noexcept { return data_.rbegin(); }
    constexpr auto rbegin() const noexcept { return data_.rbegin(); }
    constexpr auto rend() noexcept { return data_.rend(); }
    constexpr auto rend() const noexcept { return data_.rend(); }

private:
    std::vector<value_type> data_;
    Range range_;
};

template <typename ValueT>
    requires std::equality_comparable<ValueT>
constexpr bool operator==(const Array<ValueT>& lhs,
                          const Array<ValueT>& rhs) noexcept {
    if (lhs.range() != rhs.range()) {
        return false;
    }
    for (auto it1 = lhs.begin(), it2 = rhs.begin(); it1 != lhs.end();
         ++it1, ++it2) {
        if (!(*it1 == *it2)) {
            return false;
        }
    }
    return true;
}

// Specialization to use the slice flattening logic in operator(). The logic was
// put in the class instead of here to prevent duplicating the code. We need the
// non-const overload to prevent it from dispatching non-const versions to the
// generic slice impl.
template <typename ArrayT>
constexpr ArraySlice<ArrayT> slice(ArraySlice<ArrayT>& arr,
                                   Range::value_type start,
                                   Range::value_type end) {
    return arr(start, end);
}
template <typename ArrayT>
constexpr ArraySlice<ArrayT> slice(const ArraySlice<ArrayT>& arr,
                                   Range::value_type start,
                                   Range::value_type end) {
    return arr(start, end);
}

template <RangedSequence ArrayT>
    requires detail::Stringifiable<std::ranges::range_value_t<ArrayT>>
std::string to_string(const ArrayT& arr) {
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
static_assert(RangedSequence<const Array<int>>);
static_assert(RangedSequence<ArraySlice<Array<int>>>);
static_assert(RangedSequence<ArraySlice<const Array<int>>>);

}  // namespace coconext::types

namespace std {
template <typename T>
struct hash<coconext::types::Array<T>> {
    size_t operator()(const coconext::types::Array<T>& arr) const noexcept {
        size_t seed = hash<coconext::types::Range>{}(arr.range());
        for (const auto& elem : arr) {
            seed = coconext::types::detail::hash_combine(seed, elem);
        }
        return seed;
    }
};
}  // namespace std

#endif  // COCONEXT_ARRAY_HPP

#ifndef COCONEXT_ARRAY_HPP
#define COCONEXT_ARRAY_HPP

#include <coconext/types/range.hpp>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace coconext::types {

namespace {

template <typename R>
concept sized_input_range =
    std::ranges::sized_range<R> && std::ranges::input_range<R>;

}  // namespace

template <typename ArrayT>
class ArraySlice {
public:
    using value_type = typename ArrayT::value_type;
    static_assert(!std::is_reference_v<value_type>);
    using index_type = Range::value_type;

public:                               // constructor
    constexpr ArraySlice() = delete;  // no default constructor
    constexpr ArraySlice(const ArraySlice&) = default;
    constexpr ArraySlice(ArraySlice&&) = default;

    constexpr ArraySlice(ArrayT* arr, Range range) noexcept
        : arr_(arr), range_(range) {}

public:  // attributes
    constexpr const Range& range() const noexcept { return range_; }

public:  // indexing and slicing
    constexpr const value_type& operator[](index_type index) const {
        return index(*this, index);
    }
    constexpr value_type& operator[](index_type index) {
        return index(*this, index);
    }
    constexpr auto operator()(index_type start, index_type end) const {
        return slice(*this, start, end);
    }
    constexpr auto operator()(index_type start, index_type end) {
        return slice(*this, start, end);
    }
#if __cplusplus >= 202302L
    constexpr auto operator[](index_type start, index_type stop) const {
        return slice(*this, start, stop);
    }
    constexpr auto operator[](index_type start, index_type stop) {
        return slice(*this, start, stop);
    }
#endif

public:  // assignment
    template <sized_input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, value_type>
    constexpr auto& operator=(const R& obj) {
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
        requires std::convertible_to<T, value_type>
    constexpr auto& operator=(std::initializer_list<T> init) {
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
    constexpr auto begin() const noexcept {
        // we can make the assumption that find will always return a valid
        // offset since the slice range is guaranteed to be a subrange of the
        // array range by construction.
        return arr_->begin() + *find(range_, range_.left());
    }
    constexpr auto begin() noexcept {
        return arr_->begin() + *find(range_, range_.left());
    }
    constexpr auto end() const noexcept { return begin() + range_.length(); }
    constexpr auto end() noexcept { return begin() + range_.length(); }
    constexpr auto rbegin() const noexcept {
        return std::reverse_iterator(end());
    }
    constexpr auto rbegin() noexcept { return std::reverse_iterator(end()); }
    constexpr auto rend() const noexcept {
        return std::reverse_iterator(begin());
    }
    constexpr auto rend() noexcept { return std::reverse_iterator(begin()); }

private:
    ArrayT* arr_;
    Range range_;
};

template <typename ValueT>
    requires requires { !std::is_reference_v<ValueT>; }
class Array {
public:
    using value_type = ValueT;
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
        : data_(),
          range_(0, Direction::TO, static_cast<index_type>(length) - 1) {
        data_.resize(length);
    }

public:  // constructor from initializer list
    template <typename T>
        requires std::convertible_to<T, value_type>
    constexpr Array(std::initializer_list<T> init)
        : data_(init), range_(0, Direction::TO, data_.size() - 1) {}

public:  // move from vector
    template <typename T>
        requires std::convertible_to<T, value_type>
    explicit constexpr Array(std::vector<T>&& vec)
        : data_(vec), range_(0, Direction::TO, data_.size() - 1) {}

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
        : data_(), range_(0, Direction::TO, length - 1) {
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
        : data_(),
          range_(0, Direction::TO, static_cast<index_type>(length) - 1) {
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
    constexpr auto operator[](index_type idx);
    constexpr auto operator[](index_type idx) const;
    constexpr auto operator()(index_type start, index_type end);
    constexpr auto operator()(index_type start, index_type end) const;
#if __cplusplus >= 202302L
    constexpr auto operator[](index_type start, index_type stop);
    constexpr auto operator[](index_type start, index_type stop);
#endif

public:  // iterators
    constexpr auto begin() const noexcept { return data_.begin(); }
    constexpr auto end() const noexcept { return data_.end(); }
    constexpr auto rbegin() const noexcept { return data_.rbegin(); }
    constexpr auto rend() const noexcept { return data_.rend(); }

private:
    // Consider using std::unique_ptr<value_type[]> instead. We don't need to
    // store the size of the array separately since it's determined by the
    // range. This is only possible if the type is default initializable. This
    // might be best implemented as a specialization.
    std::vector<value_type> data_;
    Range range_;
};

template <typename ArrayT>
    requires requires {
        {
            std::declval<typename ArrayT::value_type>() ==
                std::declval<typename ArrayT::value_type>()
        } -> std::convertible_to<bool>;
    }
constexpr bool operator==(const ArrayT& lhs, const ArrayT& rhs) noexcept {
    if ((lhs.range().length() == 0) && (rhs.range().length() == 0)) {
        return true;
    }
    if (lhs.range().length() != rhs.range().length()) {
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

template <typename ArrayT>
auto index(ArrayT& arr, typename ArrayT::index_type idx) {
    auto find_idx = find(arr.range(), idx);
    if (find_idx == arr.range().end()) {
        throw std::out_of_range("Index out of bounds");
    }
    auto offset = std::distance(arr.range().begin(), find_idx);
    return *(arr.begin() + offset);
}

template <typename ArrayT>
auto slice(ArrayT& arr, typename ArrayT::index_type start,
           typename ArrayT::index_type end) {
    auto left_offset = *find(arr.range(), start);
    if (left_offset < 0 || left_offset >= arr.range().length()) {
        throw std::out_of_range("slice start out of bounds");
    }
    auto right_offset = *find(arr.range(), end);
    if (right_offset < 0 || right_offset >= arr.range().length()) {
        throw std::out_of_range("slice end out of bounds");
    }
    if (left_offset > right_offset) {
        throw std::invalid_argument(
            "Slice direction does not match array range direction");
    }
    return ArraySlice(&arr, Range(start, arr.range().direction(), end));
}

template <typename ValueT>
    requires requires { !std::is_reference_v<ValueT>; }
constexpr auto Array<ValueT>::operator[](
    typename Array<ValueT>::index_type idx) {
    return index(*this, idx);
}

template <typename ValueT>
    requires requires { !std::is_reference_v<ValueT>; }
constexpr auto Array<ValueT>::operator[](
    typename Array<ValueT>::index_type idx) const {
    return index(*this, idx);
}

template <typename ValueT>
    requires requires { !std::is_reference_v<ValueT>; }
constexpr auto Array<ValueT>::operator()(
    typename Array<ValueT>::index_type start,
    typename Array<ValueT>::index_type end) {
    return slice(*this, start, end);
}

template <typename ValueT>
    requires requires { !std::is_reference_v<ValueT>; }
constexpr auto Array<ValueT>::operator()(
    typename Array<ValueT>::index_type start,
    typename Array<ValueT>::index_type end) const {
    return slice(*this, start, end);
}

#if __cplusplus >= 202302L
template <typename ValueT>
    requires requires { !std::is_reference_v<ValueT>; }
constexpr auto Array<ValueT>::operator[](
    typename Array<ValueT>::index_type start,
    typename Array<ValueT>::index_type stop) {
    return slice(*this, start, stop);
}

template <typename ValueT>
    requires requires { !std::is_reference_v<ValueT>; }
constexpr auto Array<ValueT>::operator[](
    typename Array<ValueT>::index_type start,
    typename Array<ValueT>::index_type stop) const {
    return slice(*this, start, stop);
}
#endif

template <typename ValueT>
    requires requires(ValueT val) {
        { std::to_string(val) } -> std::convertible_to<std::string>;
    }
std::string to_string(const Array<ValueT>& arr) {
    std::string result = "Array([";
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        result += std::to_string(*it);
        if (std::next(it) != arr.end()) {
            result += ", ";
        }
    }
    result += "], ";
    result += to_string(arr.range());
    result += ")";
    return result;
}

static_assert(std::ranges::random_access_range<Array<int>>);
static_assert(std::ranges::random_access_range<const Array<int>>);
static_assert(std::ranges::random_access_range<ArraySlice<Array<int>>>);
static_assert(std::ranges::random_access_range<ArraySlice<const Array<int>>>);

}  // namespace coconext::types

#endif  // COCONEXT_ARRAY_HPP

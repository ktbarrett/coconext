#ifndef COCONEXT_NANOBIND_TYPES_HPP
#define COCONEXT_NANOBIND_TYPES_HPP

#include <nanobind/nanobind.h>

#include <coconext/types/array.hpp>
#include <coconext/types/range.hpp>
#include <iterator>

namespace nb = nanobind;

namespace coconext::types {

class PyListRandomAccessIterator {
public:
    using value_type = nb::object;
    using difference_type = std::ptrdiff_t;

public:  // constructors
    PyListRandomAccessIterator() = default;
    PyListRandomAccessIterator(nb::list arr, size_t pos)
        : arr_(arr), pos_(pos) {}

    auto operator*() const { return (*arr_)[pos_]; }
    auto operator*() { return (*arr_)[pos_]; }
    PyListRandomAccessIterator& operator++() {
        ++pos_;
        return *this;
    }
    PyListRandomAccessIterator operator++(int) {
        PyListRandomAccessIterator tmp = *this;
        ++(*this);
        return tmp;
    }
    PyListRandomAccessIterator& operator--() {
        --pos_;
        return *this;
    }
    PyListRandomAccessIterator operator--(int) {
        PyListRandomAccessIterator tmp = *this;
        --(*this);
        return tmp;
    }
    PyListRandomAccessIterator operator+(difference_type n) const {
        return PyListRandomAccessIterator(arr_, pos_ + n);
    }
    PyListRandomAccessIterator operator-(difference_type n) const {
        return PyListRandomAccessIterator(arr_, pos_ - n);
    }
    PyListRandomAccessIterator& operator+=(difference_type n) {
        pos_ += n;
        return *this;
    }
    PyListRandomAccessIterator& operator-=(difference_type n) {
        pos_ -= n;
        return *this;
    }
    difference_type operator-(const PyListRandomAccessIterator& other) const {
        return static_cast<difference_type>(pos_) -
               static_cast<difference_type>(other.pos_);
    }
    bool operator==(const PyListRandomAccessIterator& other) const {
        return pos_ == other.pos_;
    }
    bool operator!=(const PyListRandomAccessIterator& other) const {
        return pos_ != other.pos_;
    }
    bool operator<(const PyListRandomAccessIterator& other) const {
        return pos_ < other.pos_;
    }
    bool operator>(const PyListRandomAccessIterator& other) const {
        return pos_ > other.pos_;
    }
    bool operator<=(const PyListRandomAccessIterator& other) const {
        return pos_ <= other.pos_;
    }
    bool operator>=(const PyListRandomAccessIterator& other) const {
        return pos_ >= other.pos_;
    }
    auto operator[](difference_type n) const { return (*arr_)[pos_ + n]; }

private:
    nb::list arr_;
    size_t pos_ = 0;
};

inline PyListRandomAccessIterator operator+(
    typename PyListRandomAccessIterator::difference_type n,
    const PyListRandomAccessIterator& it) {
    return it + n;
}

template <>
class Array<nb::object> {
public:
    using value_type = nb::object;
    using index_type = Range::value_type;

public:
    Array() = delete;
    Array(const Array<nb::object>& arr)
        : data_(arr.data_), range_(arr.range_) {}
    template <typename T>
    Array(const Array<T>& arr) : data_(), range_(arr.range()) {
        for (const auto& item : arr) {
            data_.append(item);
        }
    }
    Array(Array<nb::object>&& arr) noexcept
        : data_(std::move(arr.data_)), range_(arr.range_) {}
    Array& operator=(const Array<nb::object>& arr) {
        if (this != &arr) {
            data_ = arr.data_;
            range_ = arr.range_;
        }
        return *this;
    }
    template <typename T>
    Array& operator=(const Array<T>& arr) {
        if (this != &arr) {
            data_.clear();
            for (const auto& item : arr) {
                data_.append(item);
            }
            range_ = arr.range_;
        }
        return *this;
    }
    Array& operator=(Array<nb::object>&& arr) noexcept {
        if (this != &arr) {
            data_ = std::move(arr.data_);
            range_ = arr.range_;
        }
        return *this;
    }

public:  // constructor from initializer list
    template <typename T>
        requires std::convertible_to<T, value_type>
    Array(std::initializer_list<T> init)
        : range_(0, Direction::TO, static_cast<index_type>(init.size()) - 1) {
        for (const auto& item : init) {
            data_.append(item);
        }
    }

public:  // constructor from range
    template <std::ranges::input_range R>
    explicit Array(const R& obj) {
        size_t count = 0;
        for (const auto& item : obj) {
            data_.append(item);
            count++;
        }
        range_ = Range(0, Direction::TO, static_cast<index_type>(count) - 1);
    }
    template <std::ranges::input_range R>
    explicit Array(const R& obj, Range range) : range_(range) {
        size_t count = 0;
        for (const auto& item : obj) {
            data_.append(item);
            count++;
        }
        if (count != range.length()) {
            throw std::invalid_argument("Input of size " +
                                        std::to_string(count) +
                                        " does not match range length " +
                                        std::to_string(range.length()));
        }
    }
    template <std::ranges::input_range R>
    explicit Array(const R& obj, size_t length)
        : range_(0, Direction::TO, length - 1) {
        size_t count = 0;
        for (const auto& item : obj) {
            data_.append(item);
            count++;
        }
        if (count != length) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(count) +
                " does not match range length " + std::to_string(length));
        }
    }

public:  // constructor from iterator
    template <std::input_iterator It>
    explicit Array(It begin, It end) {
        size_t count = 0;
        for (auto it = begin; it != end; ++it) {
            data_.append(*it);
            count++;
        }
        range_ = Range(0, Direction::TO, static_cast<index_type>(count) - 1);
    }
    template <std::input_iterator It>
    explicit Array(It begin, It end, Range range) : range_(range) {
        size_t count = 0;
        for (auto it = begin; it != end; ++it) {
            data_.append(*it);
            count++;
        }
        if (count != range.length()) {
            throw std::invalid_argument("Input of size " +
                                        std::to_string(count) +
                                        " does not match range length " +
                                        std::to_string(range.length()));
        }
    }
    template <std::input_iterator It>
    explicit Array(It begin, It end, size_t length)
        : range_(0, Direction::TO, length - 1) {
        size_t count = 0;
        for (auto it = begin; it != end; ++it) {
            data_.append(*it);
            count++;
        }
        if (count != length) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(count) +
                " does not match range length " + std::to_string(length));
        }
    }

public:  // nb::iterable constructors
    explicit Array(nb::iterable values)
        : data_(values), range_(0, Direction::TO, data_.size() - 1) {}
    explicit Array(nb::iterable values, Range range)
        : data_(values), range_(range) {
        if (data_.size() != range.length()) {
            throw std::invalid_argument("Input of size " +
                                        std::to_string(data_.size()) +
                                        " does not match range length " +
                                        std::to_string(range.length()));
        }
    }
    explicit Array(nb::iterable values, size_t length)
        : data_(values), range_(0, Direction::TO, length - 1) {
        if (data_.size() != length) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(data_.size()) +
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

public:  // iterators
    auto begin() noexcept { return PyListRandomAccessIterator(data_, 0); };
    auto begin() const noexcept {
        return PyListRandomAccessIterator(data_, 0);
    };
    auto end() noexcept {
        return PyListRandomAccessIterator(data_, data_.size());
    };
    auto end() const noexcept {
        return PyListRandomAccessIterator(data_, data_.size());
    };
    auto rbegin() noexcept { return std::make_reverse_iterator(end()); };
    auto rbegin() const noexcept { return std::make_reverse_iterator(end()); };
    auto rend() noexcept { return std::make_reverse_iterator(begin()); };
    auto rend() const noexcept { return std::make_reverse_iterator(begin()); };

public:  // indexing and slicing
    auto operator[](index_type idx) { return index(*this, idx); }
    auto operator[](index_type idx) const { return index(*this, idx); }
    auto operator()(index_type start, index_type end) {
        return slice(*this, start, end);
    }
    auto operator()(index_type start, index_type end) const {
        return slice(*this, start, end);
    }
#if __cplusplus >= 202302L
    auto operator[](index_type start, index_type stop) {
        return slice(start, stop);
    }
    auto operator[](index_type start, index_type stop) const {
        return slice(start, stop);
    }
#endif

private:  // attrs
    nb::list data_;
    Range range_;
};

template <typename RangeT>
    requires std::is_same_v<
        std::remove_cv_t<std::ranges::range_value_t<RangeT>>, nb::object>
bool operator==(const RangeT& lhs, const RangeT& rhs) noexcept {
    if ((lhs.range().length() == 0) && (rhs.range().length() == 0)) {
        return true;
    }
    if (lhs.range().length() != rhs.range().length()) {
        return false;
    }
    for (auto it1 = lhs.begin(), it2 = rhs.begin(); it1 != lhs.end();
         ++it1, ++it2) {
        if (!(*it1).equal(*it2)) {
            return false;
        }
    }
    return true;
}

template <typename RangeT>
    requires std::is_same_v<
        std::remove_cv_t<std::ranges::range_value_t<RangeT>>, nb::object>
auto find(const RangeT& arr, const nb::object& value) {
    auto it = std::ranges::begin(arr);
    for (; it != std::ranges::end(arr); ++it) {
        if ((*it).equal(value)) {
            break;
        }
    }
    return it;
}

inline std::string to_string(const Array<nb::object>& arr) {
    std::string repr = "Array([";
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        repr += nb::cast<std::string>(nb::repr(*it));
        if (std::next(it) != arr.end()) {
            repr += ", ";
        }
    }
    repr += "], " + to_string(arr.range()) + ")";
    return repr;
}

static_assert(std::ranges::random_access_range<Array<nb::object>>);
static_assert(std::ranges::random_access_range<const Array<nb::object>>);

}  // namespace coconext::types
#endif

#ifndef COCONEXT_VECTOR_HPP
#define COCONEXT_VECTOR_HPP

#include <algorithm>
#include <coconext/types/array_base.hpp>
#include <coconext/types/concepts.hpp>
#include <coconext/types/hash.hpp>
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
// the constexpr keyword on Vector's members is still legal but evaluating
// those members in a constant expression fails. Gate accordingly.
#if __cplusplus >= 202302L
#define COCONEXT_VECTOR_CONSTEXPR constexpr
#else
#define COCONEXT_VECTOR_CONSTEXPR
#endif

namespace coconext::types {

template <typename ValueT>
class Vector;

namespace detail {

template <typename ValueT>

// The actual Vector implementation. Separated out so it can be reused for the
// LogicVector specialization.
class VectorImpl {
  public:
    using value_type = ValueT;
    static_assert(!std::is_reference_v<value_type>);
    static_assert(!std::is_const_v<value_type>);
    using index_type = Range::value_type;
    using reference = value_type&;
    using const_reference = value_type const&;
    using iterator = value_type*;
    using const_iterator = value_type const*;

    constexpr VectorImpl() = delete;  // no default constructor

    COCONEXT_VECTOR_CONSTEXPR VectorImpl(VectorImpl&& other) noexcept = default;

    COCONEXT_VECTOR_CONSTEXPR VectorImpl(VectorImpl const& other)
        : data_(std::make_unique_for_overwrite<value_type[]>(other.range_.length())),
          range_(other.range_) {
        std::ranges::copy(other, data_.get());
    }

    COCONEXT_VECTOR_CONSTEXPR VectorImpl& operator=(VectorImpl const& other) {
        if (this != &other) {
            auto buf = std::make_unique_for_overwrite<value_type[]>(other.range_.length());
            std::ranges::copy(other, buf.get());
            data_ = std::move(buf);
            range_ = other.range_;
        }
        return *this;
    }

    COCONEXT_VECTOR_CONSTEXPR VectorImpl& operator=(VectorImpl&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            range_ = other.range_;
        }
        return *this;
    }

    explicit COCONEXT_VECTOR_CONSTEXPR VectorImpl(Range range)
        : data_(std::make_unique<value_type[]>(range.length())), range_(range) {}

    COCONEXT_VECTOR_CONSTEXPR VectorImpl(std::initializer_list<value_type> init)
        : data_(std::make_unique_for_overwrite<value_type[]>(init.size())),
          range_(init.size()) {
        std::ranges::copy(init, data_.get());
    }

    COCONEXT_VECTOR_CONSTEXPR VectorImpl(
        std::initializer_list<value_type> init, Range range
    )
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
                  && (!std::derived_from<std::remove_cvref_t<R>, VectorImpl>)
    explicit COCONEXT_VECTOR_CONSTEXPR VectorImpl(R const& obj)
        : data_(std::make_unique_for_overwrite<value_type[]>(std::ranges::size(obj))),
          range_(std::ranges::size(obj)) {
        std::ranges::copy(obj, data_.get());
    }

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, value_type>
    COCONEXT_VECTOR_CONSTEXPR VectorImpl(R const& obj, Range range) : range_(range) {
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

    COCONEXT_VECTOR_CONSTEXPR reference operator[](index_type idx) {
        return access_(*this, idx);
    }
    COCONEXT_VECTOR_CONSTEXPR const_reference operator[](index_type idx) const {
        return access_(*this, idx);
    }

    // Slices route through the outer `Vector<ValueT>` (or const variant)
    // so they pick up the Logic/Bit slice spec when applicable.
    COCONEXT_VECTOR_CONSTEXPR DynArraySlice<Vector<ValueT>> operator[](Range r) {
        detail::subsequence_check(range_, r);
        return DynArraySlice<Vector<ValueT>>(static_cast<Vector<ValueT>*>(this), r);
    }
#if __cplusplus >= 202302L
    COCONEXT_VECTOR_CONSTEXPR DynArraySlice<Vector<ValueT>> operator[](
        Range::value_type left, Range::value_type right
    ) {
        return operator[](Range{left, right});
    }
    COCONEXT_VECTOR_CONSTEXPR DynArraySlice<Vector<ValueT>> operator[](
        Range::value_type left, Direction dir, Range::value_type right
    ) {
        return operator[](Range{left, dir, right});
    }
#endif
    COCONEXT_VECTOR_CONSTEXPR DynArraySlice<Vector<ValueT> const> operator[](
        Range r
    ) const {
        detail::subsequence_check(range_, r);
        return DynArraySlice<Vector<ValueT> const>(
            static_cast<Vector<ValueT> const*>(this), r
        );
    }
#if __cplusplus >= 202302L
    COCONEXT_VECTOR_CONSTEXPR DynArraySlice<Vector<ValueT> const> operator[](
        Range::value_type left, Range::value_type right
    ) {
        return operator[](Range{left, right});
    }
    COCONEXT_VECTOR_CONSTEXPR DynArraySlice<Vector<ValueT> const> operator[](
        Range::value_type left, Direction dir, Range::value_type right
    ) {
        return operator[](Range{left, dir, right});
    }
#endif

    template <Range R>
    COCONEXT_VECTOR_CONSTEXPR ArraySlice<Vector<ValueT>, R> slice() {
        detail::subsequence_check(range_, R);
        return ArraySlice<Vector<ValueT>, R>(static_cast<Vector<ValueT>*>(this));
    }
    template <Range R>
    COCONEXT_VECTOR_CONSTEXPR ArraySlice<Vector<ValueT> const, R> slice() const {
        detail::subsequence_check(range_, R);
        return ArraySlice<Vector<ValueT> const, R>(
            static_cast<Vector<ValueT> const*>(this)
        );
    }

    COCONEXT_VECTOR_CONSTEXPR iterator begin() noexcept { return data_.get(); }
    COCONEXT_VECTOR_CONSTEXPR const_iterator begin() const noexcept { return data_.get(); }
    COCONEXT_VECTOR_CONSTEXPR iterator end() noexcept {
        return data_.get() + range_.length();
    }
    COCONEXT_VECTOR_CONSTEXPR const_iterator end() const noexcept {
        return data_.get() + range_.length();
    }
    COCONEXT_VECTOR_CONSTEXPR auto rbegin() noexcept {
        return std::reverse_iterator(end());
    }
    COCONEXT_VECTOR_CONSTEXPR auto rbegin() const noexcept {
        return std::reverse_iterator(end());
    }
    COCONEXT_VECTOR_CONSTEXPR auto rend() noexcept {
        return std::reverse_iterator(begin());
    }
    COCONEXT_VECTOR_CONSTEXPR auto rend() const noexcept {
        return std::reverse_iterator(begin());
    }

  private:
    template <typename Self>
    static COCONEXT_VECTOR_CONSTEXPR auto& access_(Self& self, index_type idx) {
        auto it = find(self.range_, idx);
        if (it == self.range_.end()) {
            throw std::out_of_range("Index out of bounds");
        }
        return *(self.data_.get() + std::distance(self.range_.begin(), it));
    }

    std::unique_ptr<value_type[]> data_;
    Range range_;
};

}  // namespace detail

// Heap-allocated, runtime-bounded array indexed according to its Range.
template <typename ValueT>
class Vector : public detail::VectorImpl<ValueT> {
  public:
    using detail::VectorImpl<ValueT>::VectorImpl;
    using detail::VectorImpl<ValueT>::operator=;
};

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

// Opt into array-specific features such as formatting.
template <typename T>
struct is_array<Vector<T>> : std::true_type {};

}  // namespace detail

static_assert(RangedSequence<Vector<int>>);
static_assert(RangedSequence<Vector<int> const>);
static_assert(RangedSequence<DynArraySlice<Vector<int>>>);
static_assert(RangedSequence<DynArraySlice<Vector<int> const>>);
static_assert(RangedSequence<ArraySlice<Vector<int>, Range{0, Direction::TO, 3}>>);
static_assert(RangedSequence<ArraySlice<Vector<int> const, Range{0, Direction::TO, 3}>>);

static_assert(!StaticRangedSequence<Vector<int>>);
static_assert(!StaticRangedSequence<DynArraySlice<Vector<int>>>);
static_assert(StaticRangedSequence<ArraySlice<Vector<int>, Range{0, Direction::TO, 3}>>);

}  // namespace coconext::types

template <typename T>
struct std::hash<coconext::types::Vector<T>> {
    size_t operator()(coconext::types::Vector<T> const& arr) const noexcept {
        size_t seed = hash<coconext::types::Range>{}(arr.range());
        for (auto const& elem : arr) {
            seed = coconext::types::detail::hash_combine(seed, elem);
        }
        return seed;
    }
};

#undef COCONEXT_VECTOR_CONSTEXPR

#endif  // COCONEXT_VECTOR_HPP

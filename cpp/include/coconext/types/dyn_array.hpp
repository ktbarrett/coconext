#ifndef COCONEXT_DYN_ARRAY_HPP
#define COCONEXT_DYN_ARRAY_HPP

#include <algorithm>
#include <coconext/types/array_base.hpp>
#include <coconext/types/concepts.hpp>
#include <coconext/types/hash.hpp>
#include <coconext/types/range.hpp>
#include <concepts>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

// std::unique_ptr's constexpr support landed in C++23 (P2273R3); under C++20
// the constexpr keyword on DynArray's members is still legal but evaluating
// those members in a constant expression fails. Gate accordingly.
#if __cplusplus >= 202302L
#define COCONEXT_DYN_ARRAY_CONSTEXPR constexpr
#else
#define COCONEXT_DYN_ARRAY_CONSTEXPR
#endif

namespace coconext::types {

template <typename ValueT>
class DynArray {
  public:
    using value_type = ValueT;
    static_assert(!std::is_reference_v<value_type>);
    static_assert(!std::is_const_v<value_type>);
    using index_type = Range::value_type;
    using reference = value_type&;
    using const_reference = value_type const&;
    using iterator = value_type*;
    using const_iterator = value_type const*;

    constexpr DynArray() = delete;  // no default constructor

    COCONEXT_DYN_ARRAY_CONSTEXPR DynArray(DynArray&& other) noexcept = default;

    COCONEXT_DYN_ARRAY_CONSTEXPR DynArray(DynArray const& other)
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
    COCONEXT_DYN_ARRAY_CONSTEXPR DynArray& operator=(DynArray const& other) {
        if (this != &other) {
            auto buf = std::make_unique_for_overwrite<value_type[]>(other.range_.length());
            std::ranges::copy(other, buf.get());
            data_ = std::move(buf);
            std::destroy_at(const_cast<Range*>(&range_));
            std::construct_at(const_cast<Range*>(&range_), other.range_);
        }
        return *this;
    }

    COCONEXT_DYN_ARRAY_CONSTEXPR DynArray& operator=(DynArray&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            std::destroy_at(const_cast<Range*>(&range_));
            std::construct_at(const_cast<Range*>(&range_), other.range_);
        }
        return *this;
    }

    explicit COCONEXT_DYN_ARRAY_CONSTEXPR DynArray(Range range)
        : data_(std::make_unique<value_type[]>(range.length())), range_(range) {}

    COCONEXT_DYN_ARRAY_CONSTEXPR DynArray(std::initializer_list<value_type> init)
        : data_(std::make_unique_for_overwrite<value_type[]>(init.size())),
          range_(init.size()) {
        std::ranges::copy(init, data_.get());
    }

    COCONEXT_DYN_ARRAY_CONSTEXPR DynArray(
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
                  && (!std::same_as<std::remove_cvref_t<R>, DynArray>)
    explicit COCONEXT_DYN_ARRAY_CONSTEXPR DynArray(R const& obj)
        : data_(std::make_unique_for_overwrite<value_type[]>(std::ranges::size(obj))),
          range_(std::ranges::size(obj)) {
        std::ranges::copy(obj, data_.get());
    }

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, value_type>
    COCONEXT_DYN_ARRAY_CONSTEXPR DynArray(R const& obj, Range range) : range_(range) {
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

    COCONEXT_DYN_ARRAY_CONSTEXPR reference operator[](index_type idx) {
        return access_(*this, idx);
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR const_reference operator[](index_type idx) const {
        return access_(*this, idx);
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR ArraySlice<DynArray> operator[](Range r) {
        detail::subsequence_check(range_, r);
        return ArraySlice<DynArray>(this, r);
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR ArraySlice<DynArray const> operator[](Range r) const {
        detail::subsequence_check(range_, r);
        return ArraySlice<DynArray const>(this, r);
    }

    COCONEXT_DYN_ARRAY_CONSTEXPR iterator begin() noexcept { return data_.get(); }
    COCONEXT_DYN_ARRAY_CONSTEXPR const_iterator begin() const noexcept {
        return data_.get();
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR iterator end() noexcept {
        return data_.get() + range_.length();
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR const_iterator end() const noexcept {
        return data_.get() + range_.length();
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR auto rbegin() noexcept {
        return std::reverse_iterator(end());
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR auto rbegin() const noexcept {
        return std::reverse_iterator(end());
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR auto rend() noexcept {
        return std::reverse_iterator(begin());
    }
    COCONEXT_DYN_ARRAY_CONSTEXPR auto rend() const noexcept {
        return std::reverse_iterator(begin());
    }

  private:
    // Shared body for the two operator[](index_type) overloads. Self is
    // deduced as DynArray for the non-const caller and DynArray
    // const for the const caller; the return type follows Self's constness
    // implicitly (via the public overload's signature).
    template <typename Self>
    static COCONEXT_DYN_ARRAY_CONSTEXPR auto& access_(Self& self, index_type idx) {
        auto it = find(self.range_, idx);
        if (it == self.range_.end()) {
            throw std::out_of_range("Index out of bounds");
        }
        return *(self.data_.get() + std::distance(self.range_.begin(), it));
    }

    std::unique_ptr<value_type[]> data_;
    Range const range_;
};

template <typename ValueT>
    requires std::equality_comparable<ValueT>
constexpr bool operator==(
    DynArray<ValueT> const& lhs, DynArray<ValueT> const& rhs
) noexcept {
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

template <typename T>
struct is_array<DynArray<T>> : std::true_type {};

}  // namespace detail

static_assert(RangedSequence<DynArray<int>>);
static_assert(RangedSequence<DynArray<int> const>);
static_assert(RangedSequence<ArraySlice<DynArray<int>>>);
static_assert(RangedSequence<ArraySlice<DynArray<int> const>>);

}  // namespace coconext::types

template <typename T>
struct std::hash<coconext::types::DynArray<T>> {
    size_t operator()(coconext::types::DynArray<T> const& arr) const noexcept {
        size_t seed = hash<coconext::types::Range>{}(arr.range());
        for (auto const& elem : arr) {
            seed = coconext::types::detail::hash_combine(seed, elem);
        }
        return seed;
    }
};

#undef COCONEXT_DYN_ARRAY_CONSTEXPR

#endif  // COCONEXT_DYN_ARRAY_HPP

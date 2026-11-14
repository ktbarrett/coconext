#ifndef COCONEXT_BIT_VECTOR_HPP
#define COCONEXT_BIT_VECTOR_HPP

#include <coconext/types/array_base.hpp>          // subsequence_check
#include <coconext/types/dyn_bigint.hpp>          // DynUInt, adopt_storage, reinterpreted
#include <coconext/types/logic.hpp>               // Bit
#include <coconext/types/logic_array_common.hpp>  // logic_downto_range
#include <coconext/types/vector.hpp>              // Vector
#include <concepts>                               // std::convertible_to, std::same_as
#include <initializer_list>                       // std::initializer_list
#include <ranges>       // std::ranges::copy, std::ranges::size, std::ranges::sized_range
#include <stdexcept>    // std::invalid_argument, std::out_of_range
#include <string>       // std::to_string
#include <string_view>  // std::string_view
#include <utility>      // std::move

namespace coconext::types {

template <>
class Vector<Bit> {
  public:
    using value_type = Bit;
    using index_type = Range::value_type;
    using reference = detail::DynUInt::BitReference;
    using const_reference = Bit;
    using iterator = detail::DynUInt::IteratorImpl<false>;
    using const_iterator = detail::DynUInt::IteratorImpl<true>;

    Vector() = delete;
    Vector(Vector const&) = default;
    Vector(Vector&&) noexcept = default;
    Vector& operator=(Vector const&) = default;
    Vector& operator=(Vector&&) noexcept = default;

    explicit Vector(Range range) : value_(range.length()), range_(range) {}

    explicit Vector(size_t length) : Vector(detail::logic_downto_range(length)) {}

    Vector(std::initializer_list<Bit> init)
        : Vector(init, detail::logic_downto_range(init.size())) {}

    Vector(std::initializer_list<Bit> init, Range range) : Vector(range) {
        if (init.size() != range.length()) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " does not match range length " + std::to_string(range.length())
            );
        }
        std::ranges::copy(init, begin());
    }

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, Bit>
              && (!std::same_as<std::remove_cvref_t<R>, Vector>)
    explicit Vector(R const& obj)
        : Vector(obj, detail::logic_downto_range(std::ranges::size(obj))) {}

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, Bit>
    Vector(R const& obj, Range range) : Vector(range) {
        if (std::ranges::size(obj) != range.length()) {
            throw std::invalid_argument(
                "Input of size " + std::to_string(std::ranges::size(obj))
                + " does not match range length " + std::to_string(range.length())
            );
        }
        std::ranges::copy(obj, begin());
    }

    template <bool SignedRepresentation>
    explicit Vector(detail::DynInt<SignedRepresentation>&& value)
        : value_(std::move(value)), range_(detail::logic_downto_range(value_.size())) {}

    template <bool SignedRepresentation>
    Vector(detail::DynInt<SignedRepresentation>&& value, Range range)
        : value_(std::move(value)), range_(range) {
        if (value_.size() != range_.length()) {
            throw std::invalid_argument("Packed value width does not match Vector range");
        }
    }

    explicit Vector(std::string_view s) : Vector(s, detail::logic_downto_range(s.size())) {}
    explicit Vector(char const* s) : Vector(std::string_view(s)) {}

    Vector(std::string_view s, Range r) : Vector(r) {
        // TODO Consider supporting separators like `_`
        if (s.size() != r.length()) {
            throw std::invalid_argument(
                "String of length " + std::to_string(s.size())
                + " does not match Range length " + std::to_string(r.length())
            );
        }
        auto out = this->begin();
        for (char c : s) {
            *out++ = Bit(c);
        }
    }
    Vector(char const* s, Range r) : Vector(std::string_view(s), r) {}

    template <detail::HasDynamicStorage Target>
    [[nodiscard]] Target as() && {
        return detail::adopt_storage<Target>(range_, std::move(value_));
    }

    [[nodiscard]] auto as() && noexcept {
        return detail::reinterpreted<Vector>(std::move(*this));
    }

    Range const& range() const noexcept { return range_; }
    size_t size() const noexcept { return range_.length(); }

    reference operator[](index_type idx) {
        auto const offset = detail::offset_of(range_, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        return value_[size() - 1 - offset.value()];
    }
    const_reference operator[](index_type idx) const {
        auto const offset = detail::offset_of(range_, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        return value_[size() - 1 - offset.value()];
    }

    ArraySlice<Vector<Bit>> operator[](Range r) {
        detail::subsequence_check(range_, r);
        return ArraySlice<Vector<Bit>>(this, r);
    }
    ArraySlice<Vector<Bit> const> operator[](Range r) const {
        detail::subsequence_check(range_, r);
        return ArraySlice<Vector<Bit> const>(this, r);
    }
#if __cplusplus >= 202302L
    ArraySlice<Vector<Bit>> operator[](index_type left, index_type right) {
        return (*this)[Range{left, range_.direction, right}];
    }
    ArraySlice<Vector<Bit>> operator[](index_type left, Direction dir, index_type right) {
        return (*this)[Range{left, dir, right}];
    }
    ArraySlice<Vector<Bit> const> operator[](index_type left, index_type right) const {
        return (*this)[Range{left, range_.direction, right}];
    }
    ArraySlice<Vector<Bit> const> operator[](
        index_type left, Direction dir, index_type right
    ) const {
        return (*this)[Range{left, dir, right}];
    }
#endif

    template <Range R>
    StaticArraySlice<Vector<Bit>, R> slice() {
        detail::subsequence_check(range_, R);
        return StaticArraySlice<Vector<Bit>, R>(this);
    }
    template <Range R>
    StaticArraySlice<Vector<Bit> const, R> slice() const {
        detail::subsequence_check(range_, R);
        return StaticArraySlice<Vector<Bit> const, R>(this);
    }

    template <index_type I>
    reference index() {
        return (*this)[I];
    }
    template <index_type I>
    const_reference index() const {
        return (*this)[I];
    }

    iterator begin() noexcept { return value_.begin(); }
    const_iterator begin() const noexcept { return value_.begin(); }
    iterator end() noexcept { return value_.end(); }
    const_iterator end() const noexcept { return value_.end(); }
    auto rbegin() noexcept { return value_.rbegin(); }
    auto rbegin() const noexcept { return value_.rbegin(); }
    auto rend() noexcept { return value_.rend(); }
    auto rend() const noexcept { return value_.rend(); }

  private:
    friend struct detail::storage_fn;

    detail::DynUInt value_;
    Range range_;
};

using BitVector = Vector<Bit>;

}  // namespace coconext::types

#endif  // COCONEXT_BIT_VECTOR_HPP

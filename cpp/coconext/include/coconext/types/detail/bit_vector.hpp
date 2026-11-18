#ifndef COCONEXT_DETAIL_BIT_VECTOR_HPP
#define COCONEXT_DETAIL_BIT_VECTOR_HPP

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <coconext/types/detail/array_base.hpp>
#include <coconext/types/detail/concepts.hpp>
#include <coconext/types/detail/dyn_bigint.hpp>
#include <coconext/types/detail/logic.hpp>
#include <coconext/types/detail/logic_array_base.hpp>
#include <coconext/types/range.hpp>

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
              && (!is_fixed<std::remove_cvref_t<R>>)
    explicit Vector(R const& obj)
        : Vector(obj, detail::logic_downto_range(std::ranges::size(obj))) {}

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, Bit>
              && (!is_fixed<std::remove_cvref_t<R>>)
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

    Vector reverse() const {
        Vector result(range().reverse());
        std::ranges::reverse_copy(*this, result.begin());
        return result;
    }

    Vector& inplace_not() & {
        for (auto&& v : *this) {
            v = ~v;
        }
        return *this;
    }

    Vector&& inplace_not() && {
        inplace_not();
        return std::move(*this);
    }

    Bit and_reduce() const { return value_.popcount() == size() ? Bit::_1 : Bit::_0; }

    Bit or_reduce() const { return value_.popcount() != 0 ? Bit::_1 : Bit::_0; }

    Bit xor_reduce() const { return value_.popcount() % 2 == 1 ? Bit::_1 : Bit::_0; }

    std::optional<Vector> resolve(ResolveMethod = ResolveMethod::WEAK) const {
        return *this;
    }

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
        auto const offset = range_.offset_of(idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        return value_[size() - 1 - offset.value()];
    }
    const_reference operator[](index_type idx) const {
        auto const offset = range_.offset_of(idx);
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

    std::optional<Range::value_type> index_of(Bit const& v) const {
        auto const it = std::ranges::find(*this, v);
        if (it == end()) {
            return std::nullopt;
        }
        return range()[static_cast<size_t>(std::ranges::distance(begin(), it))];
    }

    std::optional<Range::value_type> rindex_of(Bit const& v) const {
        auto const it = std::find(rbegin(), rend(), v);
        if (it == rend()) {
            return std::nullopt;
        }
        return range()
            [size() - 1 - static_cast<size_t>(std::ranges::distance(rbegin(), it))];
    }

  private:
    friend struct detail::storage_fn;

    detail::DynUInt value_;
    Range range_;
};

using BitVector = Vector<Bit>;

}  // namespace coconext::types

template <>
struct std::formatter<coconext::types::Vector<coconext::types::Bit>>
    : coconext::types::detail::LogicArrayFormatter<"BitVector"> {};

#endif  // COCONEXT_DETAIL_BIT_VECTOR_HPP

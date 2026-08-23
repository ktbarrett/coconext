#ifndef COCONEXT_BIT_ARRAY_HPP
#define COCONEXT_BIT_ARRAY_HPP

#include <coconext/types/array.hpp>
#include <coconext/types/bits.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/string_literal.hpp>

namespace coconext::types {

namespace detail {
template <typename T, Range R>
class Array;
}

// all reductions are better off without a loop
// All bits set iff every one of the R.length() bits is counted.
template <Range R>
Bit and_reduce(detail::Array<Bit, R> const& s) {
    return (detail::bits(s).popcount() == R.length()) ? Bit::_1 : Bit::_0;
}

template <Range R>
auto or_reduce(detail::Array<Bit, R> const& s) {
    return (detail::bits(s) != detail::Bits<R.length()>{}) ? Bit::_1 : Bit::_0;
}

template <Range R>
Bit xor_reduce(detail::Array<Bit, R> const& s) {
    return (detail::bits(s).popcount() % 2 == 1) ? Bit::_1 : Bit::_0;
}

namespace detail {

template <Range R>
class Array<Bit, R> {
  public:
    static constexpr Range static_range = R;
    static constexpr Range range() noexcept { return static_range; }

    constexpr Array() = default;

    explicit constexpr Array(std::string_view s) {
        if (s.size() != R.length()) {
            throw std::invalid_argument(
                "String of length " + std::to_string(s.size())
                + " does not match Array length " + std::to_string(R.length())
            );
        }
        auto out = this->begin();
        for (char c : s) {
            *out++ = Bit(c);
        }
    }

    explicit constexpr Array(char const* s) : Array(std::string_view(s)) {}

    template <typename U>
        requires std::convertible_to<U, Bit>
    constexpr Array(std::initializer_list<U> init) : value_(init) {}

    constexpr auto begin() { return value_.begin(); }
    constexpr auto rbegin() { return value_.rbegin(); }
    constexpr auto begin() const { return value_.begin(); }
    constexpr auto rbegin() const { return value_.rbegin(); }

    constexpr auto end() { return value_.end(); }
    constexpr auto rend() { return value_.rend(); }
    constexpr auto end() const { return value_.end(); }
    constexpr auto rend() const { return value_.rend(); }

    constexpr auto operator[](Range::value_type idx) {
        auto const offset = offset_of(R, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        size_t bit_pos = R.length() - 1 - offset.value();

        return value_[bit_pos];
    }

    constexpr auto operator[](Range::value_type idx) const {
        auto const offset = offset_of(R, idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        size_t bit_pos = R.length() - 1 - offset.value();

        return value_[bit_pos];
    }

    constexpr auto operator[](Range r) { return ArraySlice<Array<Bit, R>>(this, r); }

    constexpr auto operator[](Range r) const {
        return ArraySlice<Array<Bit, R> const>(this, r);
    }

    template <size_t N>
    constexpr auto index() {
        return value_[N];
    }

    template <size_t N>
    constexpr auto index() const {
        return value_.get_bit(N) ? Bit::_1 : Bit::_0;
    }

    template <Range R2>
    constexpr auto slice() {
        return StaticArraySlice<Array<Bit, R>, R2>(this);
    }

    template <Range R2>
    constexpr auto slice() const {
        return StaticArraySlice<Array<Bit, R> const, R2>(this);
    }

    constexpr Array(Bits<R.length()> const& packed_val) : value_(packed_val) {}

  private:
    friend struct bits_fn;

    Bits<R.length()> value_;
};

constexpr Range::value_type offset_to_hdl_coord(
    Range r, size_t offset_from_begin
) noexcept {
    auto const off = static_cast<Range::value_type>(offset_from_begin);
    return r.direction == Direction::TO ? r.left + off : r.left - off;
}

}  // namespace detail

// countl_zero and countr_zero also assembles to one
// asm instruction on most of the architectures
template <Range R>
constexpr std::optional<Range::value_type> index_of(
    detail::Array<Bit, R> const& s, Bit const& v
) {
    auto packed = detail::bits(s);
    if (!static_cast<bool>(v)) {
        packed = ~packed;
    }

    size_t clz = packed.count_leading_zeros();
    if (clz == R.length()) {
        return std::nullopt;
    }

    return detail::offset_to_hdl_coord(R, clz);
}

template <Range R>
constexpr std::optional<Range::value_type> rindex_of(
    detail::Array<Bit, R> const& s, Bit const& v
) {
    auto packed = detail::bits(s);
    if (!static_cast<bool>(v)) {
        packed = ~packed;
    }

    size_t ctz = packed.count_trailing_zeros();
    if (ctz == R.length()) {
        return std::nullopt;
    }
    size_t offset_from_begin = R.length() - 1 - ctz;

    return detail::offset_to_hdl_coord(R, offset_from_begin);
}

}  // namespace coconext::types

namespace coconext::literals {

template <coconext::types::StringLiteral S>
constexpr auto operator""_b() {
    constexpr auto N = coconext::types::detail::count_non_underscore<S>();
    static_assert(
        N <= static_cast<size_t>(
            std::numeric_limits<coconext::types::Range::value_type>::max()
        ),
        "bit literal too long for Range::value_type"
    );
    constexpr coconext::types::Range R{
        static_cast<coconext::types::Range::value_type>(N) - 1,
        coconext::types::Direction::DOWNTO,
        0
    };

    ::coconext::types::detail::Bits<N> packed_val{};
    if constexpr (N > 0) {
        ::coconext::types::detail::Bits<N> one(1);
        size_t bit_pos = N - 1;
        for (auto in = S.data; in != S.data + S.size; ++in) {
            if (*in != '_') {
                if (static_cast<bool>(::coconext::types::Bit(*in))) {
                    packed_val = packed_val | (one << bit_pos);
                }
                bit_pos--;
            }
        }
    }

    return ::coconext::types::detail::Array<::coconext::types::Bit, R>(packed_val);
}

}  // namespace coconext::literals

#endif  // COCONEXT_BIT_ARRAY_HPP

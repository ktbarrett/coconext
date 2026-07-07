#ifndef COCONEXT_BIT_ARRAY_HPP
#define COCONEXT_BIT_ARRAY_HPP

#include <bit>
#include <coconext/types/int_base.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/string_literal.hpp>

namespace coconext::types {

namespace detail {
template <typename T, Range R>
class Array;
}

// all reductions are better off without a loop
// popcount is one asm instruction so we get O(1)
template <Range R>
auto and_reduce(detail::Array<Bit, R> const& s) {
    auto bits = s.value_;
    detail::Bits<R.length()> zero(0);
    return (~bits == zero) ? '1'_b : '0'_b;
}

template <Range R>
auto or_reduce(detail::Array<Bit, R> const& s) {
    auto bits = s.value_;
    detail::Bits<R.length()> zero(0);
    return (bits != zero) ? '1'_b : '0'_b;
}

template <Range R>
auto xor_reduce(detail::Array<Bit, R> const& s) {
    auto bits = s.value_;

    if constexpr (!decltype(bits)::is_not_native_int) {
        uint64_t val = static_cast<uint64_t>(bits.raw());
        return (std::popcount(val) % 2 == 1) ? '1'_b : '0'_b;
    } else {
        return (bits.popcount() % 2 == 1) ? '1'_b : '0'_b;
    }
}

namespace detail {

template <Range R>
class Array<Bit, R> {
  public:
    constexpr Array() : value_(0) {}

    template <typename U>
        requires std::convertible_to<U, Bit>
    constexpr Array(std::initializer_list<U> init) : value_(0) {
        if (init.size() != R.length()) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " does not match Array length " + std::to_string(R.length())
            );
        }

        Bits<R.length()> zero(0);
        Bits<R.length()> one(1);

        size_t i = 0;  // position in initializer_list
        bool downto = R.direction == Direction::DOWNTO;

        for (Bit const& val : init) {
            size_t bit_pos = downto ? (R.length() - 1 - i) : i;

            auto raw_bit = static_cast<bool>(val) ? one : zero;
            value_ = value_ | (raw_bit << bit_pos);

            i++;
        }
    }

    template <Range R2>
    friend constexpr std::optional<Range::value_type> coconext::types::index_of(
        detail::Array<Bit, R2> const&, Bit const&
    );

    template <Range R2>
    friend constexpr std::optional<Range::value_type> coconext::types::rindex_of(
        detail::Array<Bit, R2> const&, Bit const&
    );

    template <Range R2>
    friend auto coconext::types::and_reduce(detail::Array<Bit, R2> const& s);
    template <Range R2>
    friend auto coconext::types::or_reduce(detail::Array<Bit, R2> const& s);
    template <Range R2>
    friend auto coconext::types::xor_reduce(detail::Array<Bit, R2> const& s);

    template <StringLiteral S>
    friend constexpr auto coconext::types::operator""_b();

  private:
    constexpr Array(Bits<R.length()> const& packed_val) : value_(packed_val) {}

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
    auto bits = s.value_;

    if (!static_cast<bool>(v)) {
        bits = ~bits;
    }

    size_t offset_from_begin;
    if constexpr (R.direction == Direction::DOWNTO) {
        size_t clz = bits.count_leading_zeros();
        if (clz == R.length()) {
            return std::nullopt;
        }
        offset_from_begin = clz;
    } else {
        size_t ctz = bits.count_trailing_zeros();
        if (ctz == R.length()) {
            return std::nullopt;
        }
        offset_from_begin = ctz;
    }

    return detail::offset_to_hdl_coord(R, offset_from_begin);
}

template <Range R>
constexpr std::optional<Range::value_type> rindex_of(
    detail::Array<Bit, R> const& s, Bit const& v
) {
    auto bits = s.value_;

    if (!static_cast<bool>(v)) {
        bits = ~bits;
    }

    size_t offset_from_begin;
    if constexpr (R.direction == Direction::DOWNTO) {
        size_t ctz = bits.count_trailing_zeros();
        if (ctz == R.length()) {
            return std::nullopt;
        }
        offset_from_begin = R.length() - 1 - ctz;
    } else {
        size_t clz = bits.count_leading_zeros();
        if (clz == R.length()) {
            return std::nullopt;
        }
        offset_from_begin = R.length() - 1 - clz;
    }

    return detail::offset_to_hdl_coord(R, offset_from_begin);
}

template <StringLiteral S>
constexpr auto operator""_b() {
    constexpr auto N = detail::count_non_underscore<S>();
    static_assert(
        N <= static_cast<size_t>(std::numeric_limits<Range::value_type>::max()),
        "bit literal too long for Range::value_type"
    );
    constexpr Range R{static_cast<Range::value_type>(N) - 1, Direction::DOWNTO, 0};

    detail::Bits<N> packed_val(0);
    detail::Bits<N> one(1);

    size_t bit_pos = (N > 0) ? (N - 1) : 0;

    for (auto in = S.data; in != S.data + S.size; ++in) {
        if (*in != '_') {
            if (static_cast<bool>(Bit(*in))) {
                packed_val = packed_val | (one << bit_pos);
            }
            bit_pos--;
        }
    }

    Array<Bit, R> result(packed_val);
    return result;
}

}  // namespace coconext::types

#endif  // COCONEXT_BIT_ARRAY_HPP

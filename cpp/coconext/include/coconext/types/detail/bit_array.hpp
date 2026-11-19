#ifndef COCONEXT_DETAIL_BIT_ARRAY_HPP
#define COCONEXT_DETAIL_BIT_ARRAY_HPP

#include <optional>

#include <coconext/types/detail/array_base.hpp>
#include <coconext/types/detail/bigint.hpp>
#include <coconext/types/detail/logic.hpp>
#include <coconext/types/detail/logic_array_base.hpp>
#include <coconext/types/detail/string_literal.hpp>
#include <coconext/types/range.hpp>

namespace coconext::types {

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

    constexpr auto reverse() const { return Array<Bit, R.reverse()>(value_.reverse()); }

    constexpr Array& inplace_not() & {
        value_ = ~value_;
        return *this;
    }

    constexpr Array&& inplace_not() && {
        inplace_not();
        return std::move(*this);
    }

    constexpr std::optional<Range::value_type> index_of(Bit const& v) const {
        auto packed = value_;
        if (!static_cast<bool>(v)) {
            packed = ~packed;
        }
        size_t const offset = packed.count_leading_zeros();
        if (offset == range().length()) {
            return std::nullopt;
        }
        return range()[offset];
    }

    constexpr std::optional<Range::value_type> rindex_of(Bit const& v) const {
        auto packed = value_;
        if (!static_cast<bool>(v)) {
            packed = ~packed;
        }
        size_t const offset = packed.count_trailing_zeros();
        if (offset == range().length()) {
            return std::nullopt;
        }
        return range()[range().length() - 1 - offset];
    }

    constexpr Bit and_reduce() const {
        return value_.popcount() == R.length() ? Bit::_1 : Bit::_0;
    }

    constexpr Bit or_reduce() const { return value_.popcount() != 0 ? Bit::_1 : Bit::_0; }

    constexpr Bit xor_reduce() const {
        return value_.popcount() % 2 == 1 ? Bit::_1 : Bit::_0;
    }

    constexpr std::optional<Array> resolve(ResolveMethod = ResolveMethod::WEAK) const {
        return *this;
    }

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
        auto const offset = R.offset_of(idx);
        if (!offset.has_value()) {
            throw std::out_of_range("Index out of bounds");
        }
        size_t bit_pos = R.length() - 1 - offset.value();

        return value_[bit_pos];
    }

    constexpr auto operator[](Range::value_type idx) const {
        auto const offset = R.offset_of(idx);
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

    template <Range::value_type I>
    constexpr auto index() {
        static_assert(R.contains(I), "index is out of range");
        return (*this)[I];
    }

    template <Range::value_type I>
    constexpr auto index() const {
        static_assert(R.contains(I), "index is out of range");
        return (*this)[I];
    }

    template <Range R2>
    constexpr auto slice() {
        return StaticArraySlice<Array<Bit, R>, R2>(this);
    }

    template <Range R2>
    constexpr auto slice() const {
        return StaticArraySlice<Array<Bit, R> const, R2>(this);
    }

    template <bool IsSigned>
    constexpr Array(Int<R.length(), IsSigned> const& packed_val) : value_(packed_val) {}

    template <NativeInteger T>
    constexpr Array(T packed_val) : value_(packed_val) {}

    template <HasStaticStorage Target>
    [[nodiscard]] constexpr Target as() && noexcept {
        static_assert(
            Target::static_range.length() == R.length(), "as() requires equal widths."
        );
        return Target(value_);
    }

    [[nodiscard]] constexpr auto as() && noexcept {
        return reinterpreted<Array>(std::move(*this));
    }

  private:
    friend struct storage_fn;

    UInt<R.length()> value_;
};

}  // namespace detail

template <auto... Args>
using BitArray = detail::Array<Bit, detail::make_logic_static_range<Args...>()>;

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

    ::coconext::types::detail::UInt<N> packed_val{};
    if constexpr (N > 0) {
        ::coconext::types::detail::UInt<N> one(1);
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

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Array<coconext::types::Bit, R>>
    : coconext::types::detail::LogicArrayFormatter<"BitArray"> {};

#endif  // COCONEXT_DETAIL_BIT_ARRAY_HPP

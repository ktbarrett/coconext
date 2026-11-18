#ifndef COCONEXT_DETAIL_LOGIC_ARRAY_HPP
#define COCONEXT_DETAIL_LOGIC_ARRAY_HPP

#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <coconext/types/detail/array.hpp>
#include <coconext/types/detail/bit_array.hpp>
#include <coconext/types/detail/logic.hpp>
#include <coconext/types/detail/logic_array_base.hpp>
#include <coconext/types/detail/string_literal.hpp>
#include <coconext/types/direction.hpp>
#include <coconext/types/range.hpp>

namespace coconext::types {

namespace detail {

template <Range R>
class Array<Logic, R> : public ArrayImpl<Logic, R> {
  public:
    using ArrayImpl<Logic, R>::ArrayImpl;
    using ArrayImpl<Logic, R>::operator=;

    explicit constexpr Array(std::string_view s) : ArrayImpl<Logic, R>() {
        if (s.size() != R.length()) {
            throw std::invalid_argument(
                "String of length " + std::to_string(s.size())
                + " does not match Array length " + std::to_string(R.length())
            );
        }
        auto out = this->begin();
        for (char c : s) {
            *out++ = Logic(c);
        }
    }
    explicit constexpr Array(char const* s) : Array(std::string_view(s)) {}

    constexpr Array& inplace_not() & {
        for (auto&& v : *this) {
            v = ~v;
        }
        return *this;
    }

    constexpr Array&& inplace_not() && {
        inplace_not();
        return std::move(*this);
    }

    constexpr auto and_reduce() const {
        using Elem = Logic;
        Elem result{Elem::_1};
        for (Elem v : *this) {
            result = result & v;
        }
        return result;
    }

    constexpr auto or_reduce() const {
        using Elem = Logic;
        Elem result{Elem::_0};
        for (Elem v : *this) {
            result = result | v;
        }
        return result;
    }

    constexpr auto xor_reduce() const {
        using Elem = Logic;
        Elem result{Elem::_0};
        for (Elem v : *this) {
            result = result ^ v;
        }
        return result;
    }

    std::optional<Array<Bit, R>> resolve(ResolveMethod method = ResolveMethod::WEAK) const {
        std::optional<Array<Bit, R>> result{std::in_place};
        if constexpr (R.length() != 0) {
            auto out = result->begin();
            for (auto const& v : *this) {
                auto r = v.resolve(method);
                if (!r) {
                    return std::nullopt;
                }
                *out++ = *r;
            }
        }
        return result;
    }
};

}  // namespace detail

template <auto... Args>
using LogicArray = detail::Array<Logic, detail::make_logic_static_range<Args...>()>;

}  // namespace coconext::types

namespace coconext::literals {

template <coconext::types::StringLiteral S>
consteval auto operator""_l() {
    constexpr auto N = coconext::types::detail::count_non_underscore<S>();
    static_assert(
        N <= static_cast<size_t>(
            std::numeric_limits<coconext::types::Range::value_type>::max()
        ),
        "logic literal too long for Range::value_type"
    );
    constexpr coconext::types::Range R{
        static_cast<coconext::types::Range::value_type>(N) - 1,
        coconext::types::Direction::DOWNTO,
        0
    };
    coconext::types::LogicArray<R> result{};
    auto out = result.begin();
    for (auto in = S.data; in != S.data + S.size; ++in) {
        if (*in != '_') {
            *out++ = coconext::types::Logic(*in);
        }
    }
    return result;
}

}  // namespace coconext::literals

template <coconext::types::Range R>
struct std::formatter<coconext::types::detail::Array<coconext::types::Logic, R>>
    : coconext::types::detail::LogicArrayFormatter<"LogicArray"> {};

#endif  // COCONEXT_DETAIL_LOGIC_ARRAY_HPP

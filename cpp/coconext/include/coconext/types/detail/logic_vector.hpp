#ifndef COCONEXT_DETAIL_LOGIC_VECTOR_HPP
#define COCONEXT_DETAIL_LOGIC_VECTOR_HPP

#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <coconext/types/detail/bit_vector.hpp>
#include <coconext/types/detail/logic.hpp>
#include <coconext/types/detail/logic_array_base.hpp>
#include <coconext/types/detail/vector.hpp>
#include <coconext/types/range.hpp>

// The LogicVector ctors below delegate to VectorImpl ctors that are only constexpr in
// C++23. BitVector uses DynUInt storage instead.
#if __cplusplus >= 202302L
#define COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR constexpr
#else
#define COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR
#endif

namespace coconext::types {

template <>
class Vector<Logic> : public detail::VectorImpl<Logic> {
  public:
    using detail::VectorImpl<Logic>::VectorImpl;
    using detail::VectorImpl<Logic>::operator=;

    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(size_t length)
        : detail::VectorImpl<Logic>(detail::logic_downto_range(length)) {}

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::initializer_list<Logic> init)
        : detail::VectorImpl<Logic>(init, detail::logic_downto_range(init.size())) {}

    template <std::ranges::sized_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, Logic>
              && (!std::derived_from<std::remove_cvref_t<R>, detail::VectorImpl<Logic>>)
    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(R const& obj)
        : detail::VectorImpl<Logic>(
              obj, detail::logic_downto_range(std::ranges::size(obj))
          ) {}

    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::string_view s)
        : Vector(s, detail::logic_downto_range(s.size())) {}
    explicit COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(char const* s)
        : Vector(std::string_view(s)) {}

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(std::string_view s, Range r)
        : detail::VectorImpl<Logic>(r) {
        if (s.size() != r.length()) {
            throw std::invalid_argument(
                "String of length " + std::to_string(s.size())
                + " does not match Range length " + std::to_string(r.length())
            );
        }
        auto out = this->begin();
        for (char c : s) {
            *out++ = Logic(c);
        }
    }
    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector(char const* s, Range r)
        : Vector(std::string_view(s), r) {}

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector& inplace_not() & {
        for (auto&& v : *this) {
            v = ~v;
        }
        return *this;
    }

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR Vector&& inplace_not() && {
        inplace_not();
        return std::move(*this);
    }

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR auto and_reduce() const {
        using Elem = Logic;
        Elem result{Elem::_1};
        for (Elem v : *this) {
            result = result & v;
        }
        return result;
    }

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR auto or_reduce() const {
        using Elem = Logic;
        Elem result{Elem::_0};
        for (Elem v : *this) {
            result = result | v;
        }
        return result;
    }

    COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR auto xor_reduce() const {
        using Elem = Logic;
        Elem result{Elem::_0};
        for (Elem v : *this) {
            result = result ^ v;
        }
        return result;
    }

    std::optional<Vector<Bit>> resolve(ResolveMethod method = ResolveMethod::WEAK) const;
};

inline std::optional<Vector<Bit>> Vector<Logic>::resolve(ResolveMethod method) const {
    std::optional<Vector<Bit>> result{std::in_place, range()};
    auto out = result->begin();
    for (auto const& v : *this) {
        auto r = v.resolve(method);
        if (!r) {
            return std::nullopt;
        }
        *out++ = *r;
    }
    return result;
}

using LogicVector = Vector<Logic>;

}  // namespace coconext::types

template <>
struct std::formatter<coconext::types::Vector<coconext::types::Logic>>
    : coconext::types::detail::LogicArrayFormatter<"LogicVector"> {};

#undef COCONEXT_DYN_LOGIC_ARRAY_CONSTEXPR

#endif  // COCONEXT_DETAIL_LOGIC_VECTOR_HPP

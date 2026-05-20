#ifndef COCONEXT_LOGIC_ARRAY_HPP
#define COCONEXT_LOGIC_ARRAY_HPP

#include <coconext/types/array.hpp>
#include <coconext/types/dynamic_array.hpp>
#include <coconext/types/logic.hpp>
#include <format>

namespace coconext::types::detail {

template <LogicType T>
constexpr std::string_view logic_type_name() {
    if constexpr (std::same_as<T, Logic>) {
        return "Logic";
    } else {
        return "Bit";
    }
}

// Variant of format_array for arrays of types with a bare to_string form
// (Logic, Bit). Prints "TypeName[range]{e1, e2, ...}" with elements rendered
// via to_string instead of std::formatter, suppressing the per-element type
// tag.
template <RangedSequence ArrayT, typename OutIt>
OutIt format_typed_array(std::string_view type_name, ArrayT const& arr, OutIt out) {
    out = std::format_to(out, "{}{}{{", type_name, arr.range());
    bool first = true;
    for (auto const& elem : arr) {
        if (!first) {
            out = std::format_to(out, ", ");
        }
        out = std::format_to(out, "{}", to_string(elem));
        first = false;
    }
    *out++ = '}';
    return out;
}

}  // namespace coconext::types::detail

namespace coconext::types {

using DynamicLogicArray = DynamicArray<Logic>;

}  // namespace coconext::types

// One LogicType-constrained formatter for every array type that opts into
// is_array (DynamicArray, Array, ArraySlice). The
// constraint is a conjunction of the generic ArrayLike constraint plus a
// LogicType check on the element type, so it subsumes the generic
// std::formatter<ArrayLike T> in array_base.hpp via C++20 partial
// specialization ordering with constraints. Result: arrays of Logic/Bit
// print as "Logic[range]{0, 1, X}" instead of "[range]{Logic{0}, Logic{1},
// Logic{X}}".
template <typename T>
    requires coconext::types::detail::ArrayLike<T>
          && coconext::types::LogicType<std::ranges::range_value_t<T>>
struct std::formatter<T> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("ArrayLike<Logic/Bit> formatter takes no format spec");
        }
        return it;
    }

    auto format(T const& arr, std::format_context& ctx) const {
        return coconext::types::detail::format_typed_array(
            coconext::types::detail::logic_type_name<std::ranges::range_value_t<T>>(),
            arr,
            ctx.out()
        );
    }
};

#endif  // COCONEXT_LOGIC_ARRAY_HPP

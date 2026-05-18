#ifndef COCONEXT_LOGIC_ARRAY_HPP
#define COCONEXT_LOGIC_ARRAY_HPP

#include <coconext/types/dynamic_array.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/static_array.hpp>
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

// Variant for arrays of types with a bare to_string form (Logic, Bit). Prints
// "TypeName[range]{e1, e2, ...}" with elements rendered via to_string instead
// of std::formatter, suppressing the per-element type tag.
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

template <coconext::types::LogicType T>
    requires coconext::types::detail::Formattable<T>
struct std::formatter<coconext::types::DynamicArray<T>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error(
                "DynamicArray<Logic/Bit> formatter takes no format spec"
            );
        }
        return it;
    }

    auto format(
        coconext::types::DynamicArray<T> const& arr, std::format_context& ctx
    ) const {
        return coconext::types::detail::format_typed_array(
            coconext::types::detail::logic_type_name<T>(), arr, ctx.out()
        );
    }
};

template <coconext::types::LogicType T>
    requires coconext::types::detail::Formattable<T>
struct std::formatter<coconext::types::ArraySlice<coconext::types::DynamicArray<T>>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error(
                "ArraySlice<DynamicArray<Logic/Bit>> formatter takes no spec"
            );
        }
        return it;
    }

    auto format(
        coconext::types::ArraySlice<coconext::types::DynamicArray<T>> const& arr,
        std::format_context& ctx
    ) const {
        return coconext::types::detail::format_typed_array(
            coconext::types::detail::logic_type_name<T>(), arr, ctx.out()
        );
    }
};

template <coconext::types::LogicType T>
    requires coconext::types::detail::Formattable<T>
struct std::formatter<coconext::types::ArraySlice<coconext::types::DynamicArray<T> const>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error(
                "ArraySlice<DynamicArray<Logic/Bit>> formatter takes no spec"
            );
        }
        return it;
    }

    auto format(
        coconext::types::ArraySlice<coconext::types::DynamicArray<T> const> const& arr,
        std::format_context& ctx
    ) const {
        return coconext::types::detail::format_typed_array(
            coconext::types::detail::logic_type_name<T>(), arr, ctx.out()
        );
    }
};

template <coconext::types::LogicType T, coconext::types::Range R>
    requires coconext::types::detail::Formattable<T>
struct std::formatter<coconext::types::StaticArray<T, R>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error(
                "StaticArray<Logic/Bit, R> formatter takes no format spec"
            );
        }
        return it;
    }

    auto format(
        coconext::types::StaticArray<T, R> const& arr, std::format_context& ctx
    ) const {
        return coconext::types::detail::format_typed_array(
            coconext::types::detail::logic_type_name<T>(), arr, ctx.out()
        );
    }
};

template <coconext::types::LogicType T, coconext::types::Range R>
    requires coconext::types::detail::Formattable<T>
struct std::formatter<coconext::types::ArraySlice<coconext::types::StaticArray<T, R>>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error(
                "ArraySlice<StaticArray<Logic/Bit, R>> formatter takes no spec"
            );
        }
        return it;
    }

    auto format(
        coconext::types::ArraySlice<coconext::types::StaticArray<T, R>> const& arr,
        std::format_context& ctx
    ) const {
        return coconext::types::detail::format_typed_array(
            coconext::types::detail::logic_type_name<T>(), arr, ctx.out()
        );
    }
};

template <coconext::types::LogicType T, coconext::types::Range R>
    requires coconext::types::detail::Formattable<T>
struct std::formatter<
    coconext::types::ArraySlice<coconext::types::StaticArray<T, R> const>> {
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error(
                "ArraySlice<StaticArray<Logic/Bit, R>> formatter takes no spec"
            );
        }
        return it;
    }

    auto format(
        coconext::types::ArraySlice<coconext::types::StaticArray<T, R> const> const& arr,
        std::format_context& ctx
    ) const {
        return coconext::types::detail::format_typed_array(
            coconext::types::detail::logic_type_name<T>(), arr, ctx.out()
        );
    }
};

#endif  // COCONEXT_LOGIC_ARRAY_HPP

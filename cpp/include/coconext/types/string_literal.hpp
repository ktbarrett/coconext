#ifndef COCONEXT_STRING_LITERAL_HPP
#define COCONEXT_STRING_LITERAL_HPP

#include <algorithm>
#include <cstddef>

namespace coconext::types {

template <size_t N>
struct StringLiteral {
    static_assert(N >= 1, "StringLiteral requires at least the NUL terminator");
    char data[N]{};
    static constexpr size_t size = N - 1;
    constexpr StringLiteral(char const (&str)[N]) { std::copy_n(str, N, data); }
};

namespace detail {

template <StringLiteral S>
constexpr size_t count_non_underscore() {
    size_t n = 0;
    for (size_t i = 0; i < S.size; ++i) {
        if (S.data[i] != '_') {
            ++n;
        }
    }
    return n;
}

}  // namespace detail

}  // namespace coconext::types

#endif  // COCONEXT_STRING_LITERAL_HPP

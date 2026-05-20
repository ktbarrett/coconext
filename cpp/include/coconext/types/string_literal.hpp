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

}  // namespace coconext::types

#endif  // COCONEXT_STRING_LITERAL_HPP

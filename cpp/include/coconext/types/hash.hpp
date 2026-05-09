#ifndef COCONEXT_HASH_HPP
#define COCONEXT_HASH_HPP

#include <cstddef>
#include <functional>

namespace coconext::types::detail {

constexpr size_t hash_mix(size_t seed, size_t value) noexcept {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

template <typename... Args>
constexpr size_t hash_combine(Args const&... args) noexcept {
    size_t seed = 0;
    ((seed = hash_mix(seed, std::hash<Args>{}(args))), ...);
    return seed;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_HASH_HPP

#ifndef COCONEXT_TESTS_CPP_FORCE_RUNTIME_HPP
#define COCONEXT_TESTS_CPP_FORCE_RUNTIME_HPP

namespace coconext::test {

template <typename T>
[[nodiscard]] T force_runtime(T value) {
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : "+m"(value) : : "memory");
#endif
    return value;
}

}  // namespace coconext::test

#endif  // COCONEXT_TESTS_CPP_FORCE_RUNTIME_HPP

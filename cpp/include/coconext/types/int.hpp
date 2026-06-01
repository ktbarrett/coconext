#ifndef COCONEXT_INT_HPP
#define COCONEXT_INT_HPP

#include "int_base.hpp"
#include <coconext/types/int_base.hpp>

// TODO these macros must explicitly
// deny unimplemented operations for BigInts.

// +, -, *, /, &, |, ^, <<, >>
#define COCONEXT_DEFINE_BINARY_OP(CLASS_TYPE, OP)                                          \
    constexpr CLASS_TYPE operator OP(const CLASS_TYPE& rhs) const                          \
        requires(!detail::using_llvm_apint)                                                \
    {                                                                                      \
        auto native_result = this->storage.raw() OP rhs.storage.raw();                     \
        return CLASS_TYPE(detail::Storage<Bits, is_signed>(native_result));                \
    }

// ~
#define COCONEXT_DEFINE_UNARY_OP(CLASS_TYPE, OP)                                           \
    constexpr CLASS_TYPE operator OP() const                                               \
        requires(!detail::using_llvm_apint)                                                \
    {                                                                                      \
        auto native_result = OP this->storage.raw();                                       \
        return CLASS_TYPE(detail::Storage<Bits, is_signed>(native_result));                \
    }

// ==, !=, <, >, <=, >=
#define COCONEXT_DEFINE_COMPARE_OP(CLASS_TYPE, OP)                                         \
    constexpr bool operator OP(const CLASS_TYPE& rhs) const                                \
        requires(!detail::using_llvm_apint)                                                \
    {                                                                                      \
        return this->storage.raw() OP rhs.storage.raw();                                   \
    }

namespace coconext::types {

template <size_t Bits>
class UInt {
  public:
    static constexpr size_t num_bits = Bits;
    static constexpr bool is_signed = false;

  private:
    [[no_unique_address]] detail::Storage<Bits, is_signed> storage;

  public:
    constexpr UInt() = default;

    constexpr UInt(detail::Storage<Bits, is_signed> const& val) : storage(val) {}

    COCONEXT_DEFINE_BINARY_OP(UInt, +)
    COCONEXT_DEFINE_BINARY_OP(UInt, -)
    COCONEXT_DEFINE_BINARY_OP(UInt, *)
    COCONEXT_DEFINE_BINARY_OP(UInt, /)
    COCONEXT_DEFINE_BINARY_OP(UInt, %)
    COCONEXT_DEFINE_BINARY_OP(UInt, &)
    COCONEXT_DEFINE_BINARY_OP(UInt, |)
    COCONEXT_DEFINE_BINARY_OP(UInt, ^)
    COCONEXT_DEFINE_BINARY_OP(UInt, <<)
    COCONEXT_DEFINE_BINARY_OP(UInt, >>)

    COCONEXT_DEFINE_UNARY_OP(UInt, ~)

    COCONEXT_DEFINE_COMPARE_OP(UInt, ==)
    COCONEXT_DEFINE_COMPARE_OP(UInt, !=)
    COCONEXT_DEFINE_COMPARE_OP(UInt, <)
    COCONEXT_DEFINE_COMPARE_OP(UInt, >)
    COCONEXT_DEFINE_COMPARE_OP(UInt, <=)
    COCONEXT_DEFINE_COMPARE_OP(UInt, >=)

    // All common features for BitArray, Unsigned, Signed, Ufixed, and Sfixed
    // TODO
};

template <size_t Bits>
class SInt {
  public:
    static constexpr size_t num_bits = Bits;
    static constexpr bool is_signed = true;

  private:
    [[no_unique_address]] detail::Storage<Bits, is_signed> storage;

  public:
    constexpr SInt() = default;

    constexpr SInt(detail::Storage<Bits, is_signed> const& val) : storage(val) {}

    COCONEXT_DEFINE_BINARY_OP(SInt, +)
    COCONEXT_DEFINE_BINARY_OP(SInt, -)
    COCONEXT_DEFINE_BINARY_OP(SInt, *)
    COCONEXT_DEFINE_BINARY_OP(SInt, /)
    COCONEXT_DEFINE_BINARY_OP(SInt, %)
    COCONEXT_DEFINE_BINARY_OP(SInt, &)
    COCONEXT_DEFINE_BINARY_OP(SInt, |)
    COCONEXT_DEFINE_BINARY_OP(SInt, ^)
    COCONEXT_DEFINE_BINARY_OP(SInt, <<)
    COCONEXT_DEFINE_BINARY_OP(SInt, >>)

    COCONEXT_DEFINE_UNARY_OP(SInt, ~)

    COCONEXT_DEFINE_COMPARE_OP(SInt, ==)
    COCONEXT_DEFINE_COMPARE_OP(SInt, !=)
    COCONEXT_DEFINE_COMPARE_OP(SInt, <)
    COCONEXT_DEFINE_COMPARE_OP(SInt, >)
    COCONEXT_DEFINE_COMPARE_OP(SInt, <=)
    COCONEXT_DEFINE_COMPARE_OP(SInt, >=)

    // All common features for BitArray, Unsigned, Signed, Ufixed, and Sfixed
    // TODO
};

}  // namespace coconext::types

#undef COCONEXT_DEFINE_BINARY_OP
#undef COCONEXT_DEFINE_UNARY_OP
#undef COCONEXT_DEFINE_COMPARE_OP

#endif

// Next work:
// SInt and UInt only support all operation for bitwdith < 64/128.
// We must support a subset of operations for APInts.
// We need to add shifting and bitwise operators, equality
// and formatting (decimal, hex, binary, octal).
// Shifting shouldn't override >> operator, but instead we should have free
// functions so we can choose between signed extending or 0 extending shifts.

#ifndef COCONEXT_INT_BASE_HPP
#define COCONEXT_INT_BASE_HPP

#include <cocoint/cocoint_lib.hpp>

// +, -, *, /, &, |, ^, <<, >>
#define COCONEXT_DEFINE_BINARY_OP(CLASS_TYPE, OP)                                          \
    constexpr CLASS_TYPE operator OP(const CLASS_TYPE& rhs) const                          \
        requires(!std::is_same_v<                                                          \
                 typename cocoint::Storage<Bits, is_signed>::StorageType,                  \
                 cocoint::detail::APInt>)                                                  \
    {                                                                                      \
        auto native_result = this->storage.raw() OP rhs.storage.raw();                     \
        return CLASS_TYPE(cocoint::Storage<Bits, is_signed>(native_result));               \
    }

// ~
#define COCONEXT_DEFINE_UNARY_OP(CLASS_TYPE, OP)                                           \
    constexpr CLASS_TYPE operator OP() const                                               \
        requires(!std::is_same_v<                                                          \
                 typename cocoint::Storage<Bits, is_signed>::StorageType,                  \
                 cocoint::detail::APInt>)                                                  \
    {                                                                                      \
        auto native_result = OP this->storage.raw();                                       \
        return CLASS_TYPE(cocoint::Storage<Bits, is_signed>(native_result));               \
    }

// ==, !=, <, >, <=, >=
#define COCONEXT_DEFINE_COMPARE_OP(CLASS_TYPE, OP)                                         \
    constexpr bool operator OP(const CLASS_TYPE& rhs) const                                \
        requires(!std::is_same_v<                                                          \
                 typename cocoint::Storage<Bits, is_signed>::StorageType,                  \
                 cocoint::detail::APInt>)                                                  \
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
    [[no_unique_address]] cocoint::Storage<Bits, is_signed> storage;

    constexpr UInt(cocoint::Storage<Bits, is_signed> const& result_storage)
        : storage(result_storage) {}

  public:
    constexpr UInt() = default;

    constexpr UInt(typename cocoint::Storage<Bits, is_signed>::StorageType val)
        requires(!std::is_same_v<
                 typename cocoint::Storage<Bits, is_signed>::StorageType,
                 cocoint::detail::APInt>)
        : storage(val) {}

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
    [[no_unique_address]] cocoint::Storage<Bits, is_signed> storage;

    constexpr SInt(cocoint::Storage<Bits, is_signed> const& result_storage)
        : storage(result_storage) {}

  public:
    constexpr SInt() = default;

    constexpr SInt(typename cocoint::Storage<Bits, is_signed>::StorageType val)
        requires(!std::is_same_v<
                 typename cocoint::Storage<Bits, is_signed>::StorageType,
                 cocoint::detail::APInt>)
        : storage(val) {}

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

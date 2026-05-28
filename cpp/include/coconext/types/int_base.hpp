#ifndef COCONEXT_INT_BASE
#define COCONEXT_INT_BASE

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifdef COCONEXT_USE_APINT
#include <llvm/ADT/APInt.h>
#endif

namespace coconext::types::detail {

#ifdef COCONEXT_USE_APINT

using BigIntType = llvm::APInt;
static constexpr bool using_llvm_apint = true;

#else

// This class can handle arbitrary precision integer's
// basic essential arithmetic E.g. shifting, bitwise operations,
// formatting(decimal, hex, binary, octal), equality e.t.c. For
// complete arithmetic operations, we fall back to APInt

// TODO
// Shifting shouldn't override >> operator, but instead we should have free
// functions so we can choose between signed extending or 0 extending shifts.
class BigInt {
  public:
    using WordType = uint64_t;

    unsigned BitWidth;
    bool is_signed;

    BigInt() {}
};

using BigIntType = BigInt;
static constexpr bool using_llvm_apint = false;

#endif  // COCONEXT_USE_APINT

struct EmptyStorage {};

// This helps deciding the data type to use at compile time
template <unsigned Bits, bool Signed>
struct StorageSelector {
    using type = std::conditional_t<
        Bits == 0,
        EmptyStorage,

        std::conditional_t<
            Bits <= 8,
            std::conditional_t<Signed, int8_t, uint8_t>,

            std::conditional_t<
                Bits <= 16,
                std::conditional_t<Signed, int16_t, uint16_t>,

                std::conditional_t<
                    Bits <= 32,
                    std::conditional_t<Signed, int32_t, uint32_t>,

                    std::conditional_t<
                        Bits <= 64,
                        std::conditional_t<Signed, int64_t, uint64_t>,

#if defined(__SIZEOF_INT128__)
                        std::conditional_t<
                            Bits <= 128,
                            std::conditional_t<Signed, __int128_t, __uint128_t>,

                            BigIntType>
#else
                        BigIntType
#endif
                        >>>>>;
};

// Unified wrapper to abstract away APInt semantics and switch between
// native ints, internal BigInt, or LLVM APInt.
template <unsigned _numBits, bool _is_signed>
class Storage {
  public:
    static constexpr unsigned num_bits = _numBits;
    static constexpr bool is_signed = _is_signed;

    using StorageType = typename StorageSelector<_numBits, _is_signed>::type;

  private:
    [[no_unique_address]] StorageType _storage;

  public:
    // Native ints
    constexpr Storage()
        requires(!std::is_same_v<StorageType, BigIntType>)
    = default;

    constexpr Storage(StorageType val)
        requires(!std::is_same_v<StorageType, BigIntType>)
        : _storage(val) {}

    // BigInt
    constexpr Storage()
        requires(std::is_same_v<StorageType, BigIntType> && !using_llvm_apint)
        : _storage() {}

    constexpr Storage(StorageType val)
        requires(std::is_same_v<StorageType, BigIntType> && !using_llvm_apint)
        : _storage() {}

    // APInt
    constexpr Storage()
        requires(std::is_same_v<StorageType, BigIntType> && using_llvm_apint)
        : _storage(_numBits, 0, _is_signed) {}

    constexpr Storage(StorageType val)
        requires(std::is_same_v<StorageType, BigIntType> && using_llvm_apint)
        : _storage(_numBits, val, _is_signed) {}

    constexpr StorageType const& raw() const { return _storage; }
};

}  // namespace coconext::types::detail

#endif

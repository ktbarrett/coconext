#ifndef COCOINT_LIB_HPP
#define COCOINT_LIB_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cocoint::detail {

// This class is designed according to the APInt llvm API.
// It is just a subset of APInt and doesn't support all operations.
class APInt {
  public:
    using WordType = uint64_t;

    unsigned BitWidth;

    APInt(unsigned numBits, uint64_t val, bool isSigned = false, bool implicitTrunc = false)
        : BitWidth(numBits) {
        if (!implicitTrunc) {
            if (isSigned) {
                if (BitWidth == 0) {
                    assert(
                        (val == 0 || val == uint64_t(-1))
                        && "Value must be 0 or -1 for signed 0-bit APInt"
                    );
                } else {
                    // assert(
                    //     llvm::isIntN(BitWidth, val) && "Value is not an N-bit signed
                    //     value"
                    // );
                }
            } else {
                if (BitWidth == 0) {
                    assert(val == 0 && "Value must be zero for unsigned 0-bit APInt");
                } else {
                    // assert(
                    //     llvm::isUIntN(BitWidth, val)
                    //     && "Value is not an N-bit unsigned value"
                    // );
                }
            }
        }
        // if (isSingleWord()) {
        //     U.VAL = val;
        //     if (implicitTrunc || isSigned) {
        //         clearUnusedBits();
        //     }
        // } else {
        //     initSlowCase(val, isSigned);
        // }
    }
};

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

                            APInt>
#else
                        APInt
#endif
                        >>>>>;
};

}  // namespace cocoint::detail

namespace cocoint {

// Unified wrapper to abstract away APInt semantics. When we replace our
// custom API with APInt, we should be able to directly use
// it by just changing our code from detail::APInt to llvm::APInt. And most
// of those changes will only had to be done in this wrapper class.
template <unsigned _numBits, bool _is_signed>
class Storage {
  public:
    static constexpr unsigned num_bits = _numBits;
    static constexpr bool is_signed = _is_signed;

    using StorageType = typename detail::StorageSelector<_numBits, _is_signed>::type;

  private:
    [[no_unique_address]] StorageType _storage;

  public:
    constexpr Storage()
        requires(!std::is_same_v<StorageType, detail::APInt>)
    = default;

    constexpr Storage()
        requires(std::is_same_v<StorageType, detail::APInt>)
        : _storage(_numBits, 0, _is_signed) {}

    constexpr Storage(StorageType val)
        requires(!std::is_same_v<StorageType, detail::APInt>)
        : _storage(val) {}

    constexpr Storage(StorageType val)
        requires(std::is_same_v<StorageType, detail::APInt>)
        : _storage(_numBits, val, _is_signed) {}

    constexpr StorageType const& raw() const { return _storage; }
};

}  // namespace cocoint

#endif

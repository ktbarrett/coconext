#ifndef COCONEXT_INT_BASE_HPP
#define COCONEXT_INT_BASE_HPP

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

// This class can handle arbitrary precision integer's basic essential
// arithmetic E.g. shifting, bitwise operations, equality e.t.c. For complete arithmetic
// operations, we fall back to APInt
class BigInt {
  public:
    using WordType = uint64_t;
    static constexpr unsigned word_width = 64;

    unsigned BitWidth;
    bool is_signed;
    unsigned num_of_words;
    std::vector<WordType> data;
    WordType last_word_mask;

    BigInt(unsigned num_bits, bool _signed)
        : BitWidth(num_bits), num_of_words((num_bits + word_width - 1) / word_width),
          is_signed(_signed), data(num_of_words, 0) {
        uint8_t valid_bits = BitWidth % 64;
        if (valid_bits == 0) {
            last_word_mask = ~WordType(0);
        } else {
            last_word_mask = (WordType(1) << valid_bits) - 1;
        }
    }

    BigInt(unsigned num_bits, bool _signed, WordType val) : BigInt(num_bits, _signed) {
        data[0] = val;
        if (is_signed && static_cast<int64_t>(val) < 0) {
            for (unsigned i = 1; i < num_of_words; ++i) {
                data[i] = ~WordType(0);
            }
        } else {
            for (unsigned i = 1; i < num_of_words; ++i) {
                data[i] = WordType(0);
            }
        }

        data.back() &= last_word_mask;
    }

    bool isNegative() const {
        unsigned sign_bit = (BitWidth - 1) % 64;
        return (data.back() >> sign_bit) & 1;
    }

    inline bool operator==(BigInt const& rhs) const {
        return BitWidth == rhs.BitWidth && is_signed == rhs.is_signed && data == rhs.data
            && last_word_mask == rhs.last_word_mask;
    }

    inline bool operator!=(BigInt const& rhs) const { return !(*this == rhs); }

    inline bool operator<(BigInt const& rhs) const {
        if (BitWidth != rhs.BitWidth) {
            throw std::invalid_argument("Bit widths must match for operations.");
        }

        bool lhs_neg = false;

        if (is_signed) {
            lhs_neg = isNegative();
            bool rhs_neg = rhs.isNegative();

            if (lhs_neg != rhs_neg) {
                return lhs_neg;
            }
        }

        for (int i = num_of_words - 1; i >= 0; --i) {
            if (data[i] != rhs.data[i]) {
                if (lhs_neg) {
                    return data[i] > rhs.data[i];
                } else {
                    return data[i] < rhs.data[i];
                }
            }
        }

        return false;
    }

    inline bool operator>(BigInt const& rhs) const { return rhs < *this; }
    inline bool operator<=(BigInt const& rhs) const { return !(rhs < *this); }
    inline bool operator>=(BigInt const& rhs) const { return !(*this < rhs); }

    inline BigInt operator&(BigInt const& rhs) const {
        if (BitWidth != rhs.BitWidth) {
            throw std::invalid_argument("Bit widths must match for operations.");
        }

        BigInt result(BitWidth, is_signed);

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] & rhs.data[i];
        }

        result.data.back() &= last_word_mask;
        return result;
    }

    inline BigInt operator|(BigInt const& rhs) const {
        if (BitWidth != rhs.BitWidth) {
            throw std::invalid_argument("Bit widths must match for operations.");
        }

        BigInt result(BitWidth, is_signed);

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] | rhs.data[i];
        }

        result.data.back() &= last_word_mask;
        return result;
    }

    inline BigInt operator^(BigInt const& rhs) const {
        if (BitWidth != rhs.BitWidth) {
            throw std::invalid_argument("Bit widths must match for operations.");
        }

        BigInt result(BitWidth, is_signed);

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] ^ rhs.data[i];
        }

        result.data.back() &= last_word_mask;
        return result;
    }

    inline BigInt operator~() const {
        BigInt result(*this);

        for (auto& word : result.data) {
            word = ~word;
        }

        result.data.back() &= last_word_mask;
        return result;
    }
};

using BigIntType = BigInt;
static constexpr bool using_llvm_apint = false;

inline BigInt shift_right_logical(BigInt const& val, size_t amount) {
    BigInt result = val;

    if (amount >= val.BitWidth) {
        std::fill(result.data.begin(), result.data.end(), 0);
        return result;
    }

    size_t const word_shift = amount / 64;
    size_t const bit_shift = amount % 64;
    std::vector<uint64_t> out(val.num_of_words, 0);

    for (size_t i = 0; i < val.num_of_words; ++i) {
        if (i + word_shift >= val.num_of_words) {
            continue;
        }

        out[i] = val.data[i + word_shift] >> bit_shift;

        if (bit_shift && i + word_shift + 1 < val.num_of_words) {
            out[i] |= val.data[i + word_shift + 1] << (64 - bit_shift);
        }
    }

    result.data = std::move(out);
    return result;
}

inline bool sign_bit(BigInt const& val) {
    unsigned const bit = (val.BitWidth - 1) % 64;
    unsigned const word = (val.BitWidth - 1) / 64;

    return (val.data[word] >> bit) & 1;
}

inline BigInt shift_right_arith(BigInt const& val, size_t amount) {
    bool sign = sign_bit(val);

    BigInt result = shift_right_logical(val, amount);

    if (!sign) {
        return result;
    }

    for (size_t i = 0; i < amount && i < result.BitWidth; ++i) {
        size_t pos = result.BitWidth - 1 - i;

        size_t word = pos / 64;
        size_t bit = pos % 64;

        result.data[word] |= (uint64_t(1) << bit);
    }

    return result;
}

inline BigInt shift_left(BigInt const& val, size_t amount) {
    BigInt result = val;

    if (amount >= val.BitWidth) {
        std::fill(result.data.begin(), result.data.end(), 0);
        return result;
    }

    size_t const word_shift = amount / 64;
    size_t const bit_shift = amount % 64;
    std::vector<uint64_t> out(val.num_of_words, 0);

    for (size_t i = 0; i < val.num_of_words; ++i) {
        if (i < word_shift) {
            continue;
        }

        out[i] = val.data[i - word_shift] << bit_shift;

        if (bit_shift && i > word_shift) {
            out[i] |= val.data[i - word_shift - 1] >> (64 - bit_shift);
        }
    }

    result.data = std::move(out);
    return result;
}

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

    static constexpr StorageType truncate(StorageType val) {
        if constexpr (!std::integral<StorageType>) {
            return val;
        } else {
            constexpr unsigned storage_bits = sizeof(StorageType) * 8;

            if constexpr (_numBits == storage_bits) {
                return val;
            } else {
                using UnsignedT = std::make_unsigned_t<StorageType>;
                constexpr UnsignedT mask = (static_cast<UnsignedT>(1) << _numBits) - 1;
                UnsignedT unsigned_val = static_cast<UnsignedT>(val) & mask;

                if constexpr (_is_signed) {
                    constexpr UnsignedT sign_bit = static_cast<UnsignedT>(1)
                                                << (_numBits - 1);
                    if (unsigned_val & sign_bit) {
                        constexpr UnsignedT sign_extend_mask = ~mask;
                        unsigned_val |= sign_extend_mask;
                    }
                }

                return static_cast<StorageType>(unsigned_val);
            }
        }
    }

  public:
    constexpr Storage(StorageType val) : _storage(truncate(val)) {}

    // Literal Int
    constexpr Storage()
        requires(!std::is_same_v<StorageType, BigIntType>)
    = default;

    // BigInt
    constexpr Storage()
        requires(std::is_same_v<StorageType, BigIntType> && !using_llvm_apint)
        : _storage(_numBits, _is_signed) {}

    template <typename T>
    constexpr Storage(T val)
        requires(
            std::is_same_v<StorageType, BigIntType> && !using_llvm_apint && std::integral<T>
        )
        : _storage(_numBits, _is_signed, static_cast<uint64_t>(val)) {}

    // APInt
    // TODO

    constexpr StorageType const& raw() const { return _storage; }
};

}  // namespace coconext::types::detail

#endif

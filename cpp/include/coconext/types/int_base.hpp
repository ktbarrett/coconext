#ifndef COCONEXT_INT_BASE_HPP
#define COCONEXT_INT_BASE_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#ifdef COCONEXT_USE_APINT
#include <llvm/ADT/APInt.h>
#endif

namespace coconext::types {

namespace detail {

#ifdef COCONEXT_USE_APINT

template <size_t Bits, bool Signed>
class IntBackend {
    // TODO
};

#else

// This class can handle arbitrary precision integer's basic essential
// arithmetic E.g. shifting, bitwise operations, equality e.t.c. For complete arithmetic
// operations, we fall back to APInt based IntBackend
template <size_t BitWidth, bool is_signed>
class IntBackend {
  public:
    using WordType = uint64_t;
    static constexpr unsigned word_width = 64;
    static constexpr unsigned num_of_words = (BitWidth + word_width - 1) / word_width;
    static constexpr bool is_single_word = num_of_words == 1;

  private:
    static constexpr WordType get_last_word_mask() {
        uint8_t valid_bits = BitWidth % word_width;
        if (valid_bits == 0) {
            return ~WordType(0);
        }
        return (WordType(1) << valid_bits) - 1;
    }

    static constexpr WordType last_word_mask = get_last_word_mask();
    std::array<WordType, num_of_words> data{};

    constexpr int64_t sign_extend_to_64(uint64_t val) const {
        static_assert(BitWidth <= 64, "sign_extend_to_64 only valid for single words");
        if constexpr (!is_signed) {
            return static_cast<int64_t>(val);
        }
        unsigned shift = 64 - BitWidth;
        return static_cast<int64_t>(val << shift) >> shift;
    }

    bool isNegative() const {
        if constexpr (!is_signed) {
            return false;
        }
        unsigned sign_bit = (BitWidth - 1) % 64;
        return (data.back() >> sign_bit) & 1;
    }

  public:
    WordType get_word(size_t index) const { return data[index]; }
    std::array<WordType, num_of_words> get_data() const { return data; }

    IntBackend() = default;

    IntBackend(WordType val) {
        data[0] = val;

        if constexpr (is_signed) {
            if (static_cast<int64_t>(val) < 0) {
                std::fill(data.begin() + 1, data.end(), ~WordType(0));
            }
        }

        data.back() &= last_word_mask;
    }

    explicit constexpr IntBackend(std::string_view str) {
        if (str.empty()) {
            return;
        }

        bool is_neg = false;
        size_t i = 0;

        if (str[i] == '-') {
            if constexpr (!is_signed) {
                throw std::invalid_argument(
                    "Cannot assign negative string to unsigned IntBackend"
                );
            }
            is_neg = true;
            i++;
        } else if (str[i] == '+') {
            i++;
        }

        bool is_hex = false;
        if (i + 1 < str.length() && str[i] == '0'
            && (str[i + 1] == 'x' || str[i + 1] == 'X'))
        {
            is_hex = true;
            i += 2;
        }

        if (is_hex) {
            if (is_neg) {
                throw std::invalid_argument("Hexadecimal value cannot be negative");
            }
            unsigned word_idx = 0;
            unsigned bit_shift = 0;

            for (int j = str.length() - 1; j >= static_cast<int>(i); --j) {
                if (word_idx >= num_of_words) {
                    break;
                }

                char c = str[j];
                uint64_t val = 0;

                if (c >= '0' && c <= '9') {
                    val = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    val = c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    val = c - 'A' + 10;
                } else if (c == '\'' || c == '_') {
                    continue;
                } else {
                    throw std::invalid_argument("Invalid hexadecimal character");
                }

                data[word_idx] |= (val << bit_shift);
                bit_shift += 4;

                if (bit_shift == 64) {
                    bit_shift = 0;
                    word_idx++;
                }
            }
        } else {
            for (; i < str.length(); ++i) {
                char c = str[i];
                if (c < '0' || c > '9') {
                    throw std::invalid_argument("Invalid base-10 character");
                }
                uint64_t digit = c - '0';

                uint64_t carry = digit;
                for (unsigned w = 0; w < num_of_words; ++w) {
                    uint64_t lower = (data[w] & 0xFFFFFFFF) * 10 + carry;
                    uint64_t upper = (data[w] >> 32) * 10 + (lower >> 32);

                    data[w] = (lower & 0xFFFFFFFF) | (upper << 32);
                    carry = upper >> 32;
                }
            }
        }

        if (is_neg) {
            uint64_t carry = 1;
            for (unsigned w = 0; w < num_of_words; ++w) {
                data[w] = ~data[w];
                uint64_t sum = data[w] + carry;
                carry = (sum < data[w]) ? 1 : 0;
                data[w] = sum;
            }
        }
        data.back() &= last_word_mask;
    }

    IntBackend& operator=(IntBackend const&) = default;
    IntBackend& operator=(IntBackend&&) noexcept = default;
    IntBackend(IntBackend const&) = default;
    IntBackend(IntBackend&&) noexcept = default;

    bool operator==(IntBackend const& rhs) const { return data == rhs.data; }

    bool operator!=(IntBackend const& rhs) const { return !(*this == rhs); }

    bool operator<(IntBackend const& rhs) const {
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
                return data[i] < rhs.data[i];
            }
        }

        return false;
    }

    bool operator>(IntBackend const& rhs) const { return rhs < *this; }
    bool operator<=(IntBackend const& rhs) const { return !(rhs < *this); }
    bool operator>=(IntBackend const& rhs) const { return !(*this < rhs); }

    IntBackend operator&(IntBackend const& rhs) const {
        IntBackend result;

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] & rhs.data[i];
        }

        return result;
    }

    IntBackend operator|(IntBackend const& rhs) const {
        IntBackend result;

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] | rhs.data[i];
        }

        return result;
    }

    IntBackend operator^(IntBackend const& rhs) const {
        IntBackend result;

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] ^ rhs.data[i];
        }

        return result;
    }

    IntBackend operator~() const {
        IntBackend result(*this);

        for (auto& word : result.data) {
            word = ~word;
        }

        result.data.back() &= last_word_mask;
        return result;
    }

    IntBackend operator+(IntBackend const& rhs) const {
        static_assert(
            is_single_word,
            "operator(+) is only supported for BitWidth within 64. Use APInt for larger "
            "widths."
        );

        IntBackend result;
        result.data[0] = data[0] + rhs.data[0];
        result.data[0] &= last_word_mask;
        return result;
    }

    IntBackend operator-(IntBackend const& rhs) const {
        static_assert(
            is_single_word,
            "operator(-) is only supported for BitWidth within 64. Use APInt for larger "
            "widths."
        );

        IntBackend result;
        result.data[0] = data[0] - rhs.data[0];
        result.data[0] &= last_word_mask;
        return result;
    }

    IntBackend operator*(IntBackend const& rhs) const {
        static_assert(
            is_single_word,
            "operator(*) is only supported for BitWidth within 64. Use APInt for larger "
            "widths."
        );

        IntBackend result;
        result.data[0] = data[0] * rhs.data[0];
        result.data[0] &= last_word_mask;
        return result;
    }

    IntBackend operator/(IntBackend const& rhs) const {
        static_assert(
            is_single_word,
            "operator(/) is only supported for BitWidth within 64. Use APInt for larger "
            "widths."
        );

        if (rhs.data[0] == 0) {
            throw std::domain_error("Division by zero not possible");
        }

        IntBackend result;
        if constexpr (is_signed) {
            int64_t lhs_val = sign_extend_to_64(data[0]);
            int64_t rhs_val = sign_extend_to_64(rhs.data[0]);
            result.data[0] = static_cast<uint64_t>(lhs_val / rhs_val);
        } else {
            result.data[0] = data[0] / rhs.data[0];
        }

        result.data[0] &= last_word_mask;
        return result;
    }

    IntBackend operator%(IntBackend const& rhs) const {
        static_assert(
            is_single_word,
            "operator(%) is only supported for BitWidth within 64. Use APInt for larger "
            "widths."
        );

        if (rhs.data[0] == 0) {
            throw std::domain_error("Modulo by zero not possible");
        }

        IntBackend result;
        if constexpr (is_signed) {
            int64_t lhs_val = sign_extend_to_64(data[0]);
            int64_t rhs_val = sign_extend_to_64(rhs.data[0]);
            result.data[0] = static_cast<uint64_t>(lhs_val % rhs_val);
        } else {
            result.data[0] = data[0] % rhs.data[0];
        }

        result.data[0] &= last_word_mask;
        return result;
    }

    template <size_t BW, bool S>
    friend void shift_right_logical(IntBackend<BW, S>& val, size_t amount);

    template <size_t BW, bool S>
    friend void shift_right_arith(IntBackend<BW, S>& val, size_t amount);

    template <size_t BW, bool S>
    friend void shift_left(IntBackend<BW, S>& val, size_t amount);
};

template <size_t BitWidth, bool is_signed>
inline void shift_right_logical(IntBackend<BitWidth, is_signed>& val, size_t amount) {
    if (amount == 0) {
        return;
    }

    if (amount >= BitWidth) {
        std::fill(val.data.begin(), val.data.end(), 0);
        return;
    }

    size_t const word_shift = amount / 64;
    size_t const bit_shift = amount % 64;
    size_t const size = val.num_of_words;

    if (word_shift > 0) {
        std::rotate(val.data.begin(), val.data.begin() + word_shift, val.data.end());
        std::fill(val.data.end() - word_shift, val.data.end(), 0);
    }

    if (bit_shift > 0) {
        size_t const active_words_end = size - word_shift;
        for (size_t i = 0; i < active_words_end - 1; ++i) {
            val.data[i] =
                (val.data[i] >> bit_shift) | (val.data[i + 1] << (64 - bit_shift));
        }
        val.data[active_words_end - 1] >>= bit_shift;
    }
}

template <size_t BitWidth, bool is_signed>
inline void shift_right_arith(IntBackend<BitWidth, is_signed>& val, size_t amount) {
    if (amount == 0) {
        return;
    }

    bool isNegative = val.isNegative();
    shift_right_logical(val, amount);

    if (!isNegative) {
        return;
    }

    size_t bits_to_set = (amount < BitWidth) ? amount : BitWidth;
    if (bits_to_set > 0) {
        size_t start_bit = BitWidth - bits_to_set;
        size_t end_bit = BitWidth - 1;

        size_t start_word = start_bit / 64;
        size_t end_word = end_bit / 64;

        size_t top_bits = (end_bit % 64) + 1;
        uint64_t top_mask =
            (top_bits == 64) ? ~uint64_t(0) : ((uint64_t(1) << top_bits) - 1);

        if (start_word == end_word) {
            val.data[start_word] |= top_mask & (~uint64_t(0) << (start_bit % 64));
        } else {
            val.data[end_word] |= top_mask;

            for (size_t w = start_word + 1; w < end_word; ++w) {
                val.data[w] |= ~uint64_t(0);
            }

            val.data[start_word] |= (~uint64_t(0) << (start_bit % 64));
        }
    }
}

template <size_t BitWidth, bool is_signed>
inline void shift_left(IntBackend<BitWidth, is_signed>& val, size_t amount) {
    if (amount == 0) {
        return;
    }

    if (amount >= BitWidth) {
        std::fill(val.data.begin(), val.data.end(), 0);
        return;
    }

    size_t const word_shift = amount / 64;
    size_t const bit_shift = amount % 64;
    size_t const size = val.num_of_words;

    if (word_shift > 0) {
        std::rotate(val.data.rbegin(), val.data.rbegin() + word_shift, val.data.rend());
        std::fill(val.data.begin(), val.data.begin() + word_shift, 0);
    }

    if (bit_shift > 0) {
        for (size_t i = size - 1; i > word_shift; --i) {
            val.data[i] =
                (val.data[i] << bit_shift) | (val.data[i - 1] >> (64 - bit_shift));
        }

        val.data[word_shift] <<= bit_shift;
    }

    val.data.back() &= val.last_word_mask;
}

#endif  // COCONEXT_USE_APINT

struct EmptyStorage {};

template <size_t Bits, bool Signed>
struct IntTypePicker {
    using type = IntBackend<Bits, Signed>;
};

template <bool Signed>
struct IntTypePicker<0, Signed> {
    using type = EmptyStorage;
};

template <bool Signed>
struct IntTypePicker<8, Signed> {
    using type = std::conditional_t<Signed, int8_t, uint8_t>;
};

template <bool Signed>
struct IntTypePicker<16, Signed> {
    using type = std::conditional_t<Signed, int16_t, uint16_t>;
};

template <bool Signed>
struct IntTypePicker<32, Signed> {
    using type = std::conditional_t<Signed, int32_t, uint32_t>;
};

template <bool Signed>
struct IntTypePicker<64, Signed> {
    using type = std::conditional_t<Signed, int64_t, uint64_t>;
};

#if defined(__SIZEOF_INT128__)
template <bool Signed>
struct IntTypePicker<128, Signed> {
    using type = std::conditional_t<Signed, __int128_t, __uint128_t>;
};
#endif

}  // namespace detail

template <size_t Bits>
using UInt = typename detail::IntTypePicker<Bits, false>::type;

template <size_t Bits>
using SInt = typename detail::IntTypePicker<Bits, true>::type;

}  // namespace coconext::types

#endif  // COCONEXT_INT_BASE_HPP

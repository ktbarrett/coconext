#ifndef COCONEXT_BIGINT_HPP
#define COCONEXT_BIGINT_HPP

// The multi-word arithmetic kernels below (the `tc*` word-array primitives, the
// Knuth division algorithm, and the division driver) are derived from LLVM's
// APInt implementation (llvm/lib/Support/APInt.cpp). They have been adapted to
// operate on fixed-size, compile-time-sized std::array storage so the whole
// type remains constexpr and never allocates.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "coconext/types/concepts.hpp"
#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace coconext::types::detail {

// ---------------------------------------------------------------------------
// Word-array kernels (derived from LLVM APInt tc* primitives)
//
// These operate on raw uint64_t words, `parts` of them, little-endian (word 0
// is least significant). They carry no notion of bit width; the owning BigInt
// masks the top word after each mutating op.
// ---------------------------------------------------------------------------

using Word = uint64_t;
inline constexpr unsigned word_bits = 64;

// DST += RHS + carry (carry is 0 or 1). Returns the carry out.
constexpr Word tc_add(Word* dst, Word const* rhs, Word carry, unsigned parts) {
    for (unsigned i = 0; i < parts; ++i) {
        Word l = dst[i];
        if (carry) {
            dst[i] += rhs[i] + 1;
            carry = (dst[i] <= l);
        } else {
            dst[i] += rhs[i];
            carry = (dst[i] < l);
        }
    }
    return carry;
}

// DST -= RHS + carry (carry is 0 or 1). Returns the borrow out.
constexpr Word tc_subtract(Word* dst, Word const* rhs, Word carry, unsigned parts) {
    for (unsigned i = 0; i < parts; ++i) {
        Word l = dst[i];
        if (carry) {
            dst[i] -= rhs[i] + 1;
            carry = (dst[i] >= l);
        } else {
            dst[i] -= rhs[i];
            carry = (dst[i] > l);
        }
    }
    return carry;
}

constexpr Word low_half(Word part) { return part & (~Word(0) >> (word_bits / 2)); }
constexpr Word high_half(Word part) { return part >> (word_bits / 2); }

// DST = SRC * MULTIPLIER + CARRY (add == false) or DST += ... (add == true).
// Mirrors APInt::tcMultiplyPart; emulates the 128-bit intermediate via 32-bit
// half-words so it needs no wider integer type.
constexpr int tc_multiply_part(
    Word* dst,
    Word const* src,
    Word multiplier,
    Word carry,
    unsigned src_parts,
    unsigned dst_parts,
    bool add
) {
    unsigned n = src_parts < dst_parts ? src_parts : dst_parts;

    for (unsigned i = 0; i < n; ++i) {
        Word src_part = src[i];
        Word low, mid, high;
        if (multiplier == 0 || src_part == 0) {
            low = carry;
            high = 0;
        } else {
            low = low_half(src_part) * low_half(multiplier);
            high = high_half(src_part) * high_half(multiplier);

            mid = low_half(src_part) * high_half(multiplier);
            high += high_half(mid);
            mid <<= word_bits / 2;
            if (low + mid < low) {
                high++;
            }
            low += mid;

            mid = high_half(src_part) * low_half(multiplier);
            high += high_half(mid);
            mid <<= word_bits / 2;
            if (low + mid < low) {
                high++;
            }
            low += mid;

            if (low + carry < low) {
                high++;
            }
            low += carry;
        }

        if (add) {
            if (low + dst[i] < low) {
                high++;
            }
            dst[i] += low;
        } else {
            dst[i] = low;
        }

        carry = high;
    }

    if (src_parts < dst_parts) {
        dst[src_parts] = carry;
        return 0;
    }

    if (carry) {
        return 1;
    }

    if (multiplier) {
        for (unsigned i = dst_parts; i < src_parts; ++i) {
            if (src[i]) {
                return 1;
            }
        }
    }

    return 0;
}

// DST = LHS * RHS, truncated to `parts` words. DST must be disjoint from both
// operands. Returns nonzero on overflow.
constexpr int tc_multiply(Word* dst, Word const* lhs, Word const* rhs, unsigned parts) {
    int overflow = 0;
    for (unsigned i = 0; i < parts; ++i) {
        overflow |= tc_multiply_part(&dst[i], lhs, rhs[i], 0, parts, parts - i, i != 0);
    }
    return overflow;
}

// Shift a bignum left `count` bits in-place; shifted-in bits are zero.
constexpr void tc_shift_left(Word* dst, unsigned words, unsigned count) {
    if (!count) {
        return;
    }

    unsigned word_shift = count / word_bits;
    if (word_shift > words) {
        word_shift = words;
    }
    unsigned bit_shift = count % word_bits;

    if (bit_shift == 0) {
        for (unsigned i = words; i-- > word_shift;) {
            dst[i] = dst[i - word_shift];
        }
    } else {
        for (unsigned i = words; i-- > word_shift;) {
            dst[i] = dst[i - word_shift] << bit_shift;
            if (i > word_shift) {
                dst[i] |= dst[i - word_shift - 1] >> (word_bits - bit_shift);
            }
        }
    }

    for (unsigned i = 0; i < word_shift; ++i) {
        dst[i] = 0;
    }
}

// Shift a bignum right `count` bits in-place (logical); shifted-in bits zero.
constexpr void tc_shift_right(Word* dst, unsigned words, unsigned count) {
    if (!count) {
        return;
    }

    unsigned word_shift = count / word_bits;
    if (word_shift > words) {
        word_shift = words;
    }
    unsigned bit_shift = count % word_bits;
    unsigned words_to_move = words - word_shift;

    if (bit_shift == 0) {
        for (unsigned i = 0; i < words_to_move; ++i) {
            dst[i] = dst[i + word_shift];
        }
    } else {
        for (unsigned i = 0; i != words_to_move; ++i) {
            dst[i] = dst[i + word_shift] >> bit_shift;
            if (i + 1 != words_to_move) {
                dst[i] |= dst[i + word_shift + 1] << (word_bits - bit_shift);
            }
        }
    }

    for (unsigned i = words_to_move; i < words; ++i) {
        dst[i] = 0;
    }
}

// Unsigned comparison of two bignums: -1, 0, or 1.
constexpr int tc_compare(Word const* lhs, Word const* rhs, unsigned parts) {
    while (parts) {
        --parts;
        if (lhs[parts] != rhs[parts]) {
            return (lhs[parts] > rhs[parts]) ? 1 : -1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Division (derived from LLVM APInt::divide + the file-static KnuthDiv)
//
// The algorithm is generic over the limb type; only the double-width limb used
// for the trial-quotient divide and the multiply-subtract is width-specific.
// That is the single #ifdef below: 64-bit limbs (needing __uint128_t) when
// available, else the portable 32-bit-limb form (needing only uint64_t).
// ---------------------------------------------------------------------------

template <typename Limb>
struct wider;

// Signed counterpart of the double-width limb type. std::make_signed does not
// accept __int128 under strict -std=c++20 in libstdc++, so select explicitly.
template <typename D>
struct as_signed {
    using type = std::make_signed_t<D>;
};

template <>
struct wider<uint32_t> {
    using type = uint64_t;
};

#if defined(__SIZEOF_INT128__)
template <>
struct wider<uint64_t> {
    using type = __uint128_t;
};
template <>
struct as_signed<__uint128_t> {
    using type = __int128_t;
};
using DivLimb = uint64_t;
#else
using DivLimb = uint32_t;
#endif

inline constexpr unsigned limb_bits = sizeof(DivLimb) * CHAR_BIT;
inline constexpr unsigned limbs_per_word = word_bits / limb_bits;

// Knuth's Algorithm D over base 2^limb_bits. `u` has m+n+1 limbs (the extra one
// for spill), `v` has n limbs, `q` receives m+1 quotient limbs, and `r` (if not
// null) receives n remainder limbs. Requires n > 1.
template <typename Limb>
constexpr void knuth_div(Limb* u, Limb* v, Limb* q, Limb* r, unsigned m, unsigned n) {
    using D = typename wider<Limb>::type;
    using SD = typename as_signed<D>::type;
    constexpr unsigned lb = sizeof(Limb) * CHAR_BIT;
    constexpr D b = D(1) << lb;

    auto lo = [](D x) -> Limb { return static_cast<Limb>(x); };
    auto hi = [](D x) -> Limb { return static_cast<Limb>(x >> lb); };
    auto make = [](Limb h, Limb l) -> D { return (D(h) << lb) | l; };

    // D1. Normalize so the divisor's high limb has its top bit set.
    unsigned shift = static_cast<unsigned>(std::countl_zero(v[n - 1]));
    Limb v_carry = 0;
    Limb u_carry = 0;
    if (shift) {
        for (unsigned i = 0; i < m + n; ++i) {
            Limb u_tmp = u[i] >> (lb - shift);
            u[i] = (u[i] << shift) | u_carry;
            u_carry = u_tmp;
        }
        for (unsigned i = 0; i < n; ++i) {
            Limb v_tmp = v[i] >> (lb - shift);
            v[i] = (v[i] << shift) | v_carry;
            v_carry = v_tmp;
        }
    }
    u[m + n] = u_carry;

    // D2-D7. Main loop over quotient limbs.
    int j = static_cast<int>(m);
    do {
        // D3. Estimate the quotient limb qp.
        D dividend = make(u[j + n], u[j + n - 1]);
        D qp = dividend / v[n - 1];
        D rp = dividend % v[n - 1];
        if (qp == b || qp * v[n - 2] > b * rp + u[j + n - 2]) {
            qp--;
            rp += v[n - 1];
            if (rp < b && (qp == b || qp * v[n - 2] > b * rp + u[j + n - 2])) {
                qp--;
            }
        }

        // D4. Multiply and subtract. `borrow` is a non-negative propagated
        // borrow in [0, 2^lb): the high-limb difference is taken in unsigned
        // limb arithmetic (wrapping) exactly as in LLVM's 32-bit-limb form.
        SD borrow = 0;
        for (unsigned i = 0; i < n; ++i) {
            D p = qp * D(v[i]);
            SD subres = SD(u[j + i]) - borrow - SD(lo(p));
            u[j + i] = lo(static_cast<D>(subres));
            borrow = static_cast<SD>(static_cast<Limb>(hi(p) - hi(static_cast<D>(subres))));
        }
        bool is_neg = SD(u[j + n]) < borrow;
        u[j + n] -= lo(static_cast<D>(borrow));

        // D5. Set quotient limb; D6. add back on the rare negative case.
        q[j] = lo(qp);
        if (is_neg) {
            q[j]--;
            bool carry = false;
            for (unsigned i = 0; i < n; ++i) {
                Limb limit = u[j + i] < v[i] ? u[j + i] : v[i];
                u[j + i] += v[i] + carry;
                carry = u[j + i] < limit || (carry && u[j + i] == limit);
            }
            u[j + n] += carry;
        }
    } while (--j >= 0);

    // D8. Unnormalize to recover the remainder.
    if (r) {
        if (shift) {
            Limb carry = 0;
            for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
                r[i] = (u[i] >> shift) | carry;
                carry = u[i] << (lb - shift);
            }
        } else {
            for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
                r[i] = u[i];
            }
        }
    }
}

// Split a uint64_t word into limbs (little-endian), storing into out[0..].
constexpr void word_to_limbs(Word w, DivLimb* out) {
    if constexpr (limbs_per_word == 1) {
        out[0] = static_cast<DivLimb>(w);
    } else {
        for (unsigned k = 0; k < limbs_per_word; ++k) {
            out[k] = static_cast<DivLimb>(w >> (k * limb_bits));
        }
    }
}

// Reassemble a uint64_t word from limbs (little-endian).
constexpr Word limbs_to_word(DivLimb const* in) {
    if constexpr (limbs_per_word == 1) {
        return static_cast<Word>(in[0]);
    } else {
        Word w = 0;
        for (unsigned k = 0; k < limbs_per_word; ++k) {
            w |= static_cast<Word>(in[k]) << (k * limb_bits);
        }
        return w;
    }
}

// Division driver: computes quotient and/or remainder of LHS / RHS, each of
// `num_words` uint64_t words. `lhs_words`/`rhs_words` are the significant
// (non-zero-high) word counts. quotient/remainder may be null. Caller has
// already handled the degenerate cases (zero, single-word, lhs < rhs, equal).
template <size_t NumWords>
constexpr void divide(
    Word const* lhs,
    unsigned lhs_words,
    Word const* rhs,
    unsigned rhs_words,
    Word* quotient,
    Word* remainder
) {
    // Widen into limbs. n = divisor limbs, m = dividend excess.
    unsigned n = rhs_words * limbs_per_word;
    unsigned m = (lhs_words * limbs_per_word) - n;

    // Scratch sized for the worst case at compile time.
    constexpr unsigned max_limbs = NumWords * limbs_per_word;
    std::array<DivLimb, max_limbs + max_limbs + 1> U{};
    std::array<DivLimb, max_limbs> V{};
    std::array<DivLimb, max_limbs + max_limbs> Q{};
    std::array<DivLimb, max_limbs> R{};

    for (unsigned i = 0; i < lhs_words; ++i) {
        word_to_limbs(lhs[i], &U[i * limbs_per_word]);
    }
    U[m + n] = 0;
    for (unsigned i = 0; i < rhs_words; ++i) {
        word_to_limbs(rhs[i], &V[i * limbs_per_word]);
    }

    // Trim leading zero limbs the widening may have introduced.
    for (unsigned i = n; i > 0 && V[i - 1] == 0; --i) {
        n--;
        m++;
    }
    for (unsigned i = m + n; i > 0 && U[i - 1] == 0; --i) {
        m--;
    }

    if (n == 1) {
        // Short division in base 2^limb_bits.
        using D = typename wider<DivLimb>::type;
        DivLimb divisor = V[0];
        DivLimb rem = 0;
        for (int i = static_cast<int>(m); i >= 0; --i) {
            D partial = (D(rem) << limb_bits) | U[i];
            if (partial == 0) {
                Q[i] = 0;
                rem = 0;
            } else if (partial < divisor) {
                Q[i] = 0;
                rem = static_cast<DivLimb>(partial);
            } else if (partial == divisor) {
                Q[i] = 1;
                rem = 0;
            } else {
                Q[i] = static_cast<DivLimb>(partial / divisor);
                rem = static_cast<DivLimb>(partial - (D(Q[i]) * divisor));
            }
        }
        R[0] = rem;
    } else {
        knuth_div<DivLimb>(
            U.data(), V.data(), Q.data(), remainder ? R.data() : nullptr, m, n
        );
    }

    if (quotient) {
        for (unsigned i = 0; i < lhs_words; ++i) {
            quotient[i] = limbs_to_word(&Q[i * limbs_per_word]);
        }
        for (unsigned i = lhs_words; i < NumWords; ++i) {
            quotient[i] = 0;
        }
    }
    if (remainder) {
        for (unsigned i = 0; i < rhs_words; ++i) {
            remainder[i] = limbs_to_word(&R[i * limbs_per_word]);
        }
        for (unsigned i = rhs_words; i < NumWords; ++i) {
            remainder[i] = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// BigInt<BitWidth>: fixed-width, std::array-backed, constexpr big integer.
// Backs the wide (W > 128) storage tier of detail::Bits.
// ---------------------------------------------------------------------------

template <size_t BitWidth>
class BigInt {
  public:
    using WordType = uint64_t;
    static constexpr unsigned word_width = 64;
    static constexpr unsigned num_of_words = (BitWidth + word_width - 1) / word_width;

  private:
    static constexpr WordType get_last_word_mask() {
        unsigned valid_bits = BitWidth % word_width;
        if (valid_bits == 0) {
            return ~WordType(0);
        }
        return (WordType(1) << valid_bits) - 1;
    }

    static constexpr WordType last_word_mask = get_last_word_mask();
    std::array<WordType, num_of_words> data{};

    constexpr void clear_unused_bits() { data.back() &= last_word_mask; }

    // Count of significant (non-zero-high) words; 0 for a zero value.
    constexpr unsigned active_words() const {
        for (unsigned i = num_of_words; i > 0; --i) {
            if (data[i - 1] != 0) {
                return i;
            }
        }
        return 0;
    }

  public:
    constexpr bool is_negative() const {
        unsigned sign_bit = (BitWidth - 1) % word_width;
        return (data.back() >> sign_bit) & 1;
    }
    constexpr WordType get_word(size_t index) const { return data[index]; }
    constexpr std::array<WordType, num_of_words> const& get_data() const { return data; }

    constexpr BigInt() = default;

    template <NativeInteger T>
        requires(sizeof(T) <= sizeof(WordType))
    constexpr BigInt(T val) {
        data[0] = static_cast<WordType>(val);
        if constexpr (std::is_signed_v<T>) {
            if (val < 0) {
                for (unsigned i = 1; i < num_of_words; ++i) {
                    data[i] = ~WordType(0);
                }
            }
        }
        clear_unused_bits();
    }

#if defined(__SIZEOF_INT128__)
    constexpr BigInt(__int128_t val) {
        data[0] = static_cast<WordType>(val);
        data[1] = static_cast<WordType>(val >> 64);
        if (val < 0) {
            for (unsigned i = 2; i < num_of_words; ++i) {
                data[i] = ~WordType(0);
            }
        }
        clear_unused_bits();
    }
    constexpr BigInt(__uint128_t val) {
        data[0] = static_cast<WordType>(val);
        data[1] = static_cast<WordType>(val >> 64);
        for (unsigned i = 2; i < num_of_words; ++i) {
            data[i] = 0;
        }
        clear_unused_bits();
    }
#endif

    explicit constexpr BigInt(std::string_view str) {
        if (str.empty()) {
            return;
        }

        bool is_neg = false;
        size_t i = 0;

        if (str[i] == '-') {
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

            for (int j = static_cast<int>(str.length()) - 1; j >= static_cast<int>(i); --j)
            {
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

                // Throw if any set bit of this nibble lands at bit index >= W.
                unsigned abs_bit = word_idx * word_width + bit_shift;
                if (val != 0
                    && (abs_bit >= BitWidth || (abs_bit + std::bit_width(val)) > BitWidth))
                {
                    throw std::out_of_range("Hexadecimal literal exceeds BigInt width");
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
                if (c == '\'' || c == '_') {
                    continue;
                }
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
                if (carry != 0 || (data.back() & ~last_word_mask) != 0) {
                    throw std::out_of_range("Decimal literal exceeds BigInt width");
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
        clear_unused_bits();
    }

    constexpr BigInt& operator=(BigInt const&) = default;
    constexpr BigInt& operator=(BigInt&&) noexcept = default;
    constexpr BigInt(BigInt const&) = default;
    constexpr BigInt(BigInt&&) noexcept = default;

    constexpr explicit operator bool() const { return active_words() != 0; }

    constexpr bool operator==(BigInt const& rhs) const { return data == rhs.data; }
    constexpr bool operator!=(BigInt const& rhs) const { return !(*this == rhs); }

    // Unsigned magnitude comparison over the raw bit pattern.
    constexpr int ucompare(BigInt const& rhs) const {
        return tc_compare(data.data(), rhs.data.data(), num_of_words);
    }

    // Signed (two's-complement) comparison under the W-bit interpretation.
    constexpr int scompare(BigInt const& rhs) const {
        bool lhs_neg = is_negative();
        bool rhs_neg = rhs.is_negative();
        if (lhs_neg != rhs_neg) {
            return lhs_neg ? -1 : 1;
        }
        return ucompare(rhs);
    }

    constexpr BigInt operator&(BigInt const& rhs) const {
        BigInt result;
        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] & rhs.data[i];
        }
        return result;
    }

    constexpr BigInt operator|(BigInt const& rhs) const {
        BigInt result;
        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] | rhs.data[i];
        }
        return result;
    }

    constexpr BigInt operator^(BigInt const& rhs) const {
        BigInt result;
        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] ^ rhs.data[i];
        }
        return result;
    }

    constexpr BigInt operator~() const {
        BigInt result(*this);
        for (auto& word : result.data) {
            word = ~word;
        }
        result.clear_unused_bits();
        return result;
    }

    constexpr BigInt operator+(BigInt const& rhs) const {
        BigInt result(*this);
        tc_add(result.data.data(), rhs.data.data(), 0, num_of_words);
        result.clear_unused_bits();
        return result;
    }

    constexpr BigInt operator-(BigInt const& rhs) const {
        BigInt result(*this);
        tc_subtract(result.data.data(), rhs.data.data(), 0, num_of_words);
        result.clear_unused_bits();
        return result;
    }

    constexpr BigInt operator*(BigInt const& rhs) const {
        BigInt result;
        tc_multiply(result.data.data(), data.data(), rhs.data.data(), num_of_words);
        result.clear_unused_bits();
        return result;
    }

    // Unsigned division; caller guarantees rhs != 0.
    constexpr BigInt udiv(BigInt const& rhs) const {
        unsigned lhs_words = active_words();
        unsigned rhs_words = rhs.active_words();

        if (lhs_words == 0 || ucompare(rhs) < 0) {
            return BigInt{};  // lhs < rhs (covers lhs == 0)
        }
        if (*this == rhs) {
            return BigInt(WordType{1});
        }
        BigInt result;
        divide<num_of_words>(
            data.data(), lhs_words, rhs.data.data(), rhs_words, result.data.data(), nullptr
        );
        result.clear_unused_bits();
        return result;
    }

    // Unsigned remainder; caller guarantees rhs != 0.
    constexpr BigInt umod(BigInt const& rhs) const {
        unsigned lhs_words = active_words();
        unsigned rhs_words = rhs.active_words();

        if (lhs_words == 0 || ucompare(rhs) < 0) {
            return *this;  // lhs < rhs (covers lhs == 0)
        }
        if (*this == rhs) {
            return BigInt{};
        }
        BigInt result;
        divide<num_of_words>(
            data.data(), lhs_words, rhs.data.data(), rhs_words, nullptr, result.data.data()
        );
        result.clear_unused_bits();
        return result;
    }

    // Signed division (truncating toward zero); caller guarantees rhs != 0.
    constexpr BigInt sdiv(BigInt const& rhs) const {
        bool lhs_neg = is_negative();
        bool rhs_neg = rhs.is_negative();
        BigInt a = lhs_neg ? -(*this) : *this;
        BigInt b = rhs_neg ? -rhs : rhs;
        BigInt q = a.udiv(b);
        return (lhs_neg ^ rhs_neg) ? -q : q;
    }

    // Signed remainder (sign follows dividend); caller guarantees rhs != 0.
    constexpr BigInt smod(BigInt const& rhs) const {
        bool lhs_neg = is_negative();
        BigInt a = lhs_neg ? -(*this) : *this;
        BigInt b = rhs.is_negative() ? -rhs : rhs;
        BigInt r = a.umod(b);
        return lhs_neg ? -r : r;
    }

    constexpr BigInt operator-() const { return (~(*this)) + BigInt(WordType{1}); }

    constexpr size_t count_trailing_zeros() const {
        for (unsigned i = 0; i < num_of_words; ++i) {
            if (data[i] != 0) {
                return (i * word_width) + std::countr_zero(data[i]);
            }
        }
        return BitWidth;
    }

    constexpr size_t count_leading_zeros() const {
        for (unsigned i = num_of_words; i > 0; --i) {
            if (data[i - 1] != 0) {
                size_t leading_in_word = std::countl_zero(data[i - 1]);
                size_t total_leading = ((num_of_words - i) * word_width) + leading_in_word;
                size_t unused_top_bits = (num_of_words * word_width) - BitWidth;
                return total_leading - unused_top_bits;
            }
        }
        return BitWidth;
    }

    constexpr size_t popcount() const {
        size_t n = 0;
        for (unsigned i = 0; i < num_of_words; ++i) {
            n += std::popcount(data[i]);
        }
        return n;
    }

    template <size_t BW>
    friend constexpr void shift_right_logical(BigInt<BW>& val, size_t amount);

    template <size_t BW>
    friend constexpr void shift_right_arith(BigInt<BW>& val, size_t amount);

    template <size_t BW>
    friend constexpr void shift_left(BigInt<BW>& val, size_t amount);
};

template <size_t BitWidth>
constexpr void shift_right_logical(BigInt<BitWidth>& val, size_t amount) {
    if (amount >= BitWidth) {
        for (auto& word : val.data) {
            word = 0;
        }
        return;
    }
    tc_shift_right(
        val.data.data(), BigInt<BitWidth>::num_of_words, static_cast<unsigned>(amount)
    );
}

template <size_t BitWidth>
constexpr void shift_right_arith(BigInt<BitWidth>& val, size_t amount) {
    if (amount == 0) {
        return;
    }

    bool negative = val.is_negative();
    shift_right_logical(val, amount);

    if (!negative) {
        return;
    }

    // Set the top `amount` bits to sign-extend.
    size_t bits_to_set = (amount < BitWidth) ? amount : BitWidth;
    size_t start_bit = BitWidth - bits_to_set;
    constexpr unsigned word_width = BigInt<BitWidth>::word_width;
    for (size_t bit = start_bit; bit < BitWidth; ++bit) {
        val.data[bit / word_width] |= (uint64_t{1} << (bit % word_width));
    }
    val.clear_unused_bits();
}

template <size_t BitWidth>
constexpr void shift_left(BigInt<BitWidth>& val, size_t amount) {
    if (amount >= BitWidth) {
        for (auto& word : val.data) {
            word = 0;
        }
        return;
    }
    tc_shift_left(
        val.data.data(), BigInt<BitWidth>::num_of_words, static_cast<unsigned>(amount)
    );
    val.clear_unused_bits();
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_BIGINT_HPP

#ifndef COCONEXT_BIGINT_HPP
#define COCONEXT_BIGINT_HPP

// The multi-word arithmetic kernels below (the `tc*` word-array primitives, the
// Knuth division algorithm, and the division driver) are derived from LLVM's
// APInt implementation (llvm/lib/Support/APInt.cpp). Storage is factored out
// via non-owning views (BigIntConstRef / BigIntMutRef): the kernels operate on
// spans of words and carry no notion of who owns them, so the same code serves
// any width and any storage strategy.
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
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace coconext::types::detail {

using Word = uint64_t;
inline constexpr unsigned word_bits = 64;

// ---------------------------------------------------------------------------
// Word-array kernels (derived from LLVM APInt tc* primitives)
//
// Little-endian (word 0 is least significant). They carry no notion of bit
// width; the caller masks the top word after each mutating op.
// ---------------------------------------------------------------------------

constexpr Word tc_add(std::span<Word> dst, std::span<Word const> rhs, Word carry) {
    for (size_t i = 0; i < dst.size(); ++i) {
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

constexpr Word tc_subtract(std::span<Word> dst, std::span<Word const> rhs, Word carry) {
    for (size_t i = 0; i < dst.size(); ++i) {
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

constexpr int tc_multiply_part(
    std::span<Word> dst, std::span<Word const> src, Word multiplier, Word carry, bool add
) {
    size_t src_parts = src.size();
    size_t dst_parts = dst.size();
    size_t n = src_parts < dst_parts ? src_parts : dst_parts;

    for (size_t i = 0; i < n; ++i) {
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
        for (size_t i = dst_parts; i < src_parts; ++i) {
            if (src[i]) {
                return 1;
            }
        }
    }

    return 0;
}

constexpr int tc_multiply(
    std::span<Word> dst, std::span<Word const> lhs, std::span<Word const> rhs
) {
    int overflow = 0;
    size_t parts = dst.size();
    for (size_t i = 0; i < parts; ++i) {
        overflow |= tc_multiply_part(dst.subspan(i), lhs, rhs[i], 0, /*add=*/i != 0);
    }
    return overflow;
}

constexpr void tc_shift_left(std::span<Word> dst, unsigned count) {
    if (!count) {
        return;
    }
    size_t words = dst.size();

    size_t word_shift = count / word_bits;
    if (word_shift > words) {
        word_shift = words;
    }
    unsigned bit_shift = count % word_bits;

    if (bit_shift == 0) {
        for (size_t i = words; i-- > word_shift;) {
            dst[i] = dst[i - word_shift];
        }
    } else {
        for (size_t i = words; i-- > word_shift;) {
            dst[i] = dst[i - word_shift] << bit_shift;
            if (i > word_shift) {
                dst[i] |= dst[i - word_shift - 1] >> (word_bits - bit_shift);
            }
        }
    }

    for (size_t i = 0; i < word_shift; ++i) {
        dst[i] = 0;
    }
}

constexpr void tc_shift_right(std::span<Word> dst, unsigned count) {
    if (!count) {
        return;
    }
    size_t words = dst.size();

    size_t word_shift = count / word_bits;
    if (word_shift > words) {
        word_shift = words;
    }
    unsigned bit_shift = count % word_bits;
    size_t words_to_move = words - word_shift;

    if (bit_shift == 0) {
        for (size_t i = 0; i < words_to_move; ++i) {
            dst[i] = dst[i + word_shift];
        }
    } else {
        for (size_t i = 0; i != words_to_move; ++i) {
            dst[i] = dst[i + word_shift] >> bit_shift;
            if (i + 1 != words_to_move) {
                dst[i] |= dst[i + word_shift + 1] << (word_bits - bit_shift);
            }
        }
    }

    for (size_t i = words_to_move; i < words; ++i) {
        dst[i] = 0;
    }
}

constexpr int tc_compare(std::span<Word const> lhs, std::span<Word const> rhs) {
    size_t parts = lhs.size();
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
// ---------------------------------------------------------------------------

template <typename Limb>
struct wider;

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

template <typename Limb>
constexpr void knuth_div(Limb* u, Limb* v, Limb* q, Limb* r, unsigned m, unsigned n) {
    using D = typename wider<Limb>::type;
    using SD = typename as_signed<D>::type;
    constexpr unsigned lb = sizeof(Limb) * CHAR_BIT;
    constexpr D b = D(1) << lb;

    auto lo = [](D x) -> Limb { return static_cast<Limb>(x); };
    auto hi = [](D x) -> Limb { return static_cast<Limb>(x >> lb); };
    auto make = [](Limb h, Limb l) -> D { return (D(h) << lb) | l; };

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

    int j = static_cast<int>(m);
    do {
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

        SD borrow = 0;
        for (unsigned i = 0; i < n; ++i) {
            D p = qp * D(v[i]);
            SD subres = SD(u[j + i]) - borrow - SD(lo(p));
            u[j + i] = lo(static_cast<D>(subres));
            borrow = static_cast<SD>(static_cast<Limb>(hi(p) - hi(static_cast<D>(subres))));
        }
        bool is_neg = SD(u[j + n]) < borrow;
        u[j + n] -= lo(static_cast<D>(borrow));

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

constexpr void word_to_limbs(Word w, DivLimb* out) {
    if constexpr (limbs_per_word == 1) {
        out[0] = static_cast<DivLimb>(w);
    } else {
        for (unsigned k = 0; k < limbs_per_word; ++k) {
            out[k] = static_cast<DivLimb>(w >> (k * limb_bits));
        }
    }
}

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

struct DivideScratch {
    std::span<DivLimb> U;
    std::span<DivLimb> V;
    std::span<DivLimb> Q;
    std::span<DivLimb> R;
};

constexpr void divide_impl(
    std::span<Word const> lhs,
    unsigned lhs_words,
    std::span<Word const> rhs,
    unsigned rhs_words,
    std::span<Word> quotient,
    std::span<Word> remainder,
    DivideScratch scratch
) {
    unsigned n = rhs_words * limbs_per_word;
    unsigned m = (lhs_words * limbs_per_word) - n;

    for (unsigned i = 0; i < lhs_words; ++i) {
        word_to_limbs(lhs[i], &scratch.U[i * limbs_per_word]);
    }
    scratch.U[m + n] = 0;
    for (unsigned i = 0; i < rhs_words; ++i) {
        word_to_limbs(rhs[i], &scratch.V[i * limbs_per_word]);
    }

    for (unsigned i = n; i > 0 && scratch.V[i - 1] == 0; --i) {
        n--;
        m++;
    }
    for (unsigned i = m + n; i > 0 && scratch.U[i - 1] == 0; --i) {
        m--;
    }

    if (n == 1) {
        using D = typename wider<DivLimb>::type;
        DivLimb divisor = scratch.V[0];
        DivLimb rem = 0;
        for (int i = static_cast<int>(m); i >= 0; --i) {
            D partial = (D(rem) << limb_bits) | scratch.U[i];
            if (partial == 0) {
                scratch.Q[i] = 0;
                rem = 0;
            } else if (partial < divisor) {
                scratch.Q[i] = 0;
                rem = static_cast<DivLimb>(partial);
            } else if (partial == divisor) {
                scratch.Q[i] = 1;
                rem = 0;
            } else {
                scratch.Q[i] = static_cast<DivLimb>(partial / divisor);
                rem = static_cast<DivLimb>(partial - (D(scratch.Q[i]) * divisor));
            }
        }
        scratch.R[0] = rem;
    } else {
        knuth_div<DivLimb>(
            scratch.U.data(),
            scratch.V.data(),
            scratch.Q.data(),
            remainder.empty() ? nullptr : scratch.R.data(),
            m,
            n
        );
    }

    if (!quotient.empty()) {
        for (unsigned i = 0; i < lhs_words; ++i) {
            quotient[i] = limbs_to_word(&scratch.Q[i * limbs_per_word]);
        }
        for (size_t i = lhs_words; i < quotient.size(); ++i) {
            quotient[i] = 0;
        }
    }
    if (!remainder.empty()) {
        for (unsigned i = 0; i < rhs_words; ++i) {
            remainder[i] = limbs_to_word(&scratch.R[i * limbs_per_word]);
        }
        for (size_t i = rhs_words; i < remainder.size(); ++i) {
            remainder[i] = 0;
        }
    }
}

// Single-allocation heap scratch for divide_impl. Zero-initialized because
// divide_impl's output loop reads the full lhs_words*lpw / rhs_words*lpw tail,
// but knuth_div only writes q[0..m] / r[0..n-1].
struct OwnedDivScratch {
    std::unique_ptr<DivLimb[]> buf;
    DivideScratch view;
};

inline OwnedDivScratch make_owned_div_scratch(size_t max_limbs) {
    size_t u_len = 2 * max_limbs + 1;
    size_t v_len = max_limbs;
    size_t q_len = 2 * max_limbs;
    size_t r_len = max_limbs;
    size_t total = u_len + v_len + q_len + r_len;
    auto buf = std::make_unique<DivLimb[]>(total);
    DivLimb* p = buf.get();
    DivideScratch view{
        std::span<DivLimb>{p,                         u_len},
        std::span<DivLimb>{p + u_len,                 v_len},
        std::span<DivLimb>{p + u_len + v_len,         q_len},
        std::span<DivLimb>{p + u_len + v_len + q_len, r_len}
    };
    return OwnedDivScratch{std::move(buf), view};
}

// ---------------------------------------------------------------------------
// BigIntConstRef / BigIntMutRef: non-owning views over big-int word storage.
// Storage-agnostic algorithms are free functions taking these views; owners
// implicitly convert to the appropriate view, so every kernel has exactly one
// implementation regardless of how the words are stored.
// ---------------------------------------------------------------------------

class BigIntConstRef {
    std::span<Word const> data_;
    size_t bit_width_;

  public:
    constexpr BigIntConstRef(std::span<Word const> d, size_t bw)
        : data_(d), bit_width_(bw) {}

    constexpr std::span<Word const> data() const { return data_; }
    constexpr size_t bit_width() const { return bit_width_; }
    constexpr size_t num_words() const { return data_.size(); }
    constexpr Word word(size_t i) const { return data_[i]; }
};

class BigIntMutRef {
    std::span<Word> data_;
    size_t bit_width_;

  public:
    constexpr BigIntMutRef(std::span<Word> d, size_t bw) : data_(d), bit_width_(bw) {}

    constexpr operator BigIntConstRef() const {
        return BigIntConstRef{
            std::span<Word const>{data_.data(), data_.size()},
             bit_width_
        };
    }

    constexpr std::span<Word> data() const { return data_; }
    constexpr size_t bit_width() const { return bit_width_; }
    constexpr size_t num_words() const { return data_.size(); }

    constexpr Word last_word_mask() const {
        unsigned valid_bits = bit_width_ % word_bits;
        if (valid_bits == 0) {
            return ~Word(0);
        }
        return (Word(1) << valid_bits) - 1;
    }
};

// ---- Read-only operations on a view ----

constexpr bool is_negative(BigIntConstRef v) {
    if (v.bit_width() == 0) {
        return false;
    }
    unsigned sign_bit = (v.bit_width() - 1) % word_bits;
    return (v.data().back() >> sign_bit) & 1;
}

constexpr unsigned active_words(BigIntConstRef v) {
    auto d = v.data();
    for (size_t i = d.size(); i > 0; --i) {
        if (d[i - 1] != 0) {
            return static_cast<unsigned>(i);
        }
    }
    return 0;
}

constexpr void check_same_width(BigIntConstRef a, BigIntConstRef b) {
    if (a.bit_width() != b.bit_width()) {
        throw std::invalid_argument("BigInt bit width mismatch");
    }
}

constexpr int ucompare(BigIntConstRef a, BigIntConstRef b) {
    check_same_width(a, b);
    return tc_compare(a.data(), b.data());
}

constexpr int scompare(BigIntConstRef a, BigIntConstRef b) {
    check_same_width(a, b);
    bool an = is_negative(a);
    bool bn = is_negative(b);
    if (an != bn) {
        return an ? -1 : 1;
    }
    return tc_compare(a.data(), b.data());
}

constexpr size_t count_trailing_zeros(BigIntConstRef v) {
    auto d = v.data();
    for (size_t i = 0; i < d.size(); ++i) {
        if (d[i] != 0) {
            return (i * word_bits) + std::countr_zero(d[i]);
        }
    }
    return v.bit_width();
}

constexpr size_t count_leading_zeros(BigIntConstRef v) {
    auto d = v.data();
    size_t nw = d.size();
    for (size_t i = nw; i > 0; --i) {
        if (d[i - 1] != 0) {
            size_t leading_in_word = std::countl_zero(d[i - 1]);
            size_t total_leading = ((nw - i) * word_bits) + leading_in_word;
            size_t unused_top_bits = (nw * word_bits) - v.bit_width();
            return total_leading - unused_top_bits;
        }
    }
    return v.bit_width();
}

constexpr size_t popcount(BigIntConstRef v) {
    size_t n = 0;
    for (Word w : v.data()) {
        n += std::popcount(w);
    }
    return n;
}

constexpr bool get_bit(BigIntConstRef v, size_t index) {
    return (v.data()[index / word_bits] >> (index % word_bits)) & 1;
}

// ---- Mutating operations on a view ----

constexpr void clear_unused_bits(BigIntMutRef v) {
    auto d = v.data();
    if (!d.empty()) {
        d.back() &= v.last_word_mask();
    }
}

constexpr void add_assign(BigIntMutRef dst, BigIntConstRef rhs) {
    check_same_width(dst, rhs);
    tc_add(dst.data(), rhs.data(), 0);
    clear_unused_bits(dst);
}

constexpr void sub_assign(BigIntMutRef dst, BigIntConstRef rhs) {
    check_same_width(dst, rhs);
    tc_subtract(dst.data(), rhs.data(), 0);
    clear_unused_bits(dst);
}

constexpr void and_assign(BigIntMutRef dst, BigIntConstRef rhs) {
    check_same_width(dst, rhs);
    auto d = dst.data();
    auto s = rhs.data();
    for (size_t i = 0; i < d.size(); ++i) {
        d[i] &= s[i];
    }
}

constexpr void or_assign(BigIntMutRef dst, BigIntConstRef rhs) {
    check_same_width(dst, rhs);
    auto d = dst.data();
    auto s = rhs.data();
    for (size_t i = 0; i < d.size(); ++i) {
        d[i] |= s[i];
    }
}

constexpr void xor_assign(BigIntMutRef dst, BigIntConstRef rhs) {
    check_same_width(dst, rhs);
    auto d = dst.data();
    auto s = rhs.data();
    for (size_t i = 0; i < d.size(); ++i) {
        d[i] ^= s[i];
    }
}

constexpr void bitnot(BigIntMutRef v) {
    for (auto& w : v.data()) {
        w = ~w;
    }
    clear_unused_bits(v);
}

// Two's-complement negation: ~x + 1.
constexpr void negate(BigIntMutRef v) {
    Word carry = 1;
    for (auto& w : v.data()) {
        w = ~w;
        Word sum = w + carry;
        carry = (sum < w) ? 1 : 0;
        w = sum;
    }
    clear_unused_bits(v);
}

constexpr void shift_left(BigIntMutRef v, size_t amount) {
    if (amount >= v.bit_width()) {
        for (auto& w : v.data()) {
            w = 0;
        }
        return;
    }
    tc_shift_left(v.data(), static_cast<unsigned>(amount));
    clear_unused_bits(v);
}

constexpr void shift_right_logical(BigIntMutRef v, size_t amount) {
    if (amount >= v.bit_width()) {
        for (auto& w : v.data()) {
            w = 0;
        }
        return;
    }
    tc_shift_right(v.data(), static_cast<unsigned>(amount));
}

constexpr void shift_right_arith(BigIntMutRef v, size_t amount) {
    if (amount == 0) {
        return;
    }
    bool negative = is_negative(v);
    shift_right_logical(v, amount);
    if (!negative) {
        return;
    }
    size_t bw = v.bit_width();
    size_t bits_to_set = (amount < bw) ? amount : bw;
    size_t start_bit = bw - bits_to_set;
    auto d = v.data();
    for (size_t bit = start_bit; bit < bw; ++bit) {
        d[bit / word_bits] |= (Word(1) << (bit % word_bits));
    }
    clear_unused_bits(v);
}

// dst = lhs * rhs (truncated). dst must be disjoint from operands.
constexpr void multiply(BigIntMutRef dst, BigIntConstRef lhs, BigIntConstRef rhs) {
    check_same_width(dst, lhs);
    check_same_width(dst, rhs);
    tc_multiply(dst.data(), lhs.data(), rhs.data());
    clear_unused_bits(dst);
}

// Parse a decimal or (unsigned) 0x-prefixed hex literal into `dst`. Requires
// dst to be zero-valued on entry. Accepts ' and _ as digit separators.
// Throws std::invalid_argument on malformed input and std::out_of_range if
// the value doesn't fit in dst.bit_width() bits.
constexpr void parse_into(BigIntMutRef dst, std::string_view str) {
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
    if (i + 1 < str.length() && str[i] == '0' && (str[i + 1] == 'x' || str[i + 1] == 'X')) {
        is_hex = true;
        i += 2;
    }

    auto d = dst.data();
    size_t bw = dst.bit_width();

    if (is_hex) {
        if (is_neg) {
            throw std::invalid_argument("Hexadecimal value cannot be negative");
        }
        unsigned word_idx = 0;
        unsigned bit_shift = 0;
        for (int j = static_cast<int>(str.length()) - 1; j >= static_cast<int>(i); --j) {
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

            unsigned abs_bit = word_idx * word_bits + bit_shift;
            if (val != 0 && (abs_bit >= bw || (abs_bit + std::bit_width(val)) > bw)) {
                throw std::out_of_range("Hexadecimal literal exceeds BigInt width");
            }

            if (val != 0) {
                d[word_idx] |= (val << bit_shift);
            }
            bit_shift += 4;
            if (bit_shift == 64) {
                bit_shift = 0;
                word_idx++;
            }
        }
    } else {
        Word top_mask = dst.last_word_mask();
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
            for (size_t w = 0; w < d.size(); ++w) {
                uint64_t lower = (d[w] & 0xFFFFFFFF) * 10 + carry;
                uint64_t upper = (d[w] >> 32) * 10 + (lower >> 32);
                d[w] = (lower & 0xFFFFFFFF) | (upper << 32);
                carry = upper >> 32;
            }
            if (carry != 0 || (!d.empty() && (d.back() & ~top_mask) != 0)) {
                throw std::out_of_range("Decimal literal exceeds BigInt width");
            }
        }
    }

    if (is_neg) {
        negate(dst);
    }
    clear_unused_bits(dst);
}

// Load a signed/unsigned 128-bit native value into `dst`, sign- or
// zero-extended as appropriate. Requires dst to be zero on entry (default).
#if defined(__SIZEOF_INT128__)
constexpr void load_int128(BigIntMutRef dst, __int128_t val) {
    auto d = dst.data();
    if (d.empty()) {
        return;
    }
    d[0] = static_cast<Word>(val);
    if (d.size() > 1) {
        d[1] = static_cast<Word>(val >> 64);
    }
    if (val < 0) {
        for (size_t i = 2; i < d.size(); ++i) {
            d[i] = ~Word(0);
        }
    }
    clear_unused_bits(dst);
}

constexpr void load_uint128(BigIntMutRef dst, __uint128_t val) {
    auto d = dst.data();
    if (d.empty()) {
        return;
    }
    d[0] = static_cast<Word>(val);
    if (d.size() > 1) {
        d[1] = static_cast<Word>(val >> 64);
    }
    clear_unused_bits(dst);
}
#endif

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

    constexpr void mask_top_word() { data.back() &= last_word_mask; }

    // Scratch large enough for the worst-case operand of this width.
    struct DivScratchStorage {
        static constexpr size_t max_limbs = num_of_words * limbs_per_word;
        std::array<DivLimb, max_limbs + max_limbs + 1> U{};
        std::array<DivLimb, max_limbs> V{};
        std::array<DivLimb, max_limbs + max_limbs> Q{};
        std::array<DivLimb, max_limbs> R{};
        constexpr DivideScratch view() {
            return DivideScratch{
                std::span<DivLimb>{U},
                std::span<DivLimb>{V},
                std::span<DivLimb>{Q},
                std::span<DivLimb>{R}
            };
        }
    };

  public:
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
        mask_top_word();
    }

#if defined(__SIZEOF_INT128__)
    constexpr BigInt(__int128_t val) { load_int128(*this, val); }
    constexpr BigInt(__uint128_t val) { load_uint128(*this, val); }
#endif

    explicit constexpr BigInt(std::string_view str) { parse_into(*this, str); }

    constexpr BigInt& operator=(BigInt const&) = default;
    constexpr BigInt& operator=(BigInt&&) noexcept = default;
    constexpr BigInt(BigInt const&) = default;
    constexpr BigInt(BigInt&&) noexcept = default;

    constexpr operator BigIntConstRef() const {
        return BigIntConstRef{std::span<WordType const>{data}, BitWidth};
    }
    constexpr operator BigIntMutRef() {
        return BigIntMutRef{std::span<WordType>{data}, BitWidth};
    }

    constexpr WordType get_word(size_t index) const { return data[index]; }
    constexpr std::array<WordType, num_of_words> const& get_data() const { return data; }

    constexpr bool is_negative() const { return detail::is_negative(*this); }
    constexpr size_t count_trailing_zeros() const {
        return detail::count_trailing_zeros(*this);
    }
    constexpr size_t count_leading_zeros() const {
        return detail::count_leading_zeros(*this);
    }
    constexpr size_t popcount() const { return detail::popcount(*this); }
    constexpr int ucompare(BigInt const& rhs) const { return detail::ucompare(*this, rhs); }
    constexpr int scompare(BigInt const& rhs) const { return detail::scompare(*this, rhs); }

    constexpr explicit operator bool() const { return active_words(*this) != 0; }

    constexpr bool operator==(BigInt const& rhs) const { return data == rhs.data; }
    constexpr bool operator!=(BigInt const& rhs) const { return !(*this == rhs); }

    constexpr BigInt operator+(BigInt const& rhs) const {
        BigInt result(*this);
        add_assign(result, rhs);
        return result;
    }

    constexpr BigInt operator-(BigInt const& rhs) const {
        BigInt result(*this);
        sub_assign(result, rhs);
        return result;
    }

    // multiply(dst, lhs, rhs) requires dst disjoint from both operands; the
    // copy is a distinct object whose contents get overwritten.
    constexpr BigInt operator*(BigInt const& rhs) const {
        BigInt result(*this);
        multiply(result, *this, rhs);
        return result;
    }

    constexpr BigInt operator&(BigInt const& rhs) const {
        BigInt result(*this);
        and_assign(result, rhs);
        return result;
    }

    constexpr BigInt operator|(BigInt const& rhs) const {
        BigInt result(*this);
        or_assign(result, rhs);
        return result;
    }

    constexpr BigInt operator^(BigInt const& rhs) const {
        BigInt result(*this);
        xor_assign(result, rhs);
        return result;
    }

    constexpr BigInt operator~() const {
        BigInt result(*this);
        bitnot(result);
        return result;
    }

    constexpr BigInt operator-() const {
        BigInt result(*this);
        negate(result);
        return result;
    }

    // Division stays owner-side: scratch shape (std::array vs unique_ptr[])
    // depends on the storage tier.
    constexpr BigInt udiv(BigInt const& rhs) const {
        unsigned lw = active_words(*this);
        unsigned rw = active_words(rhs);
        if (lw == 0 || ucompare(rhs) < 0) {
            return BigInt{};
        }
        if (*this == rhs) {
            return BigInt(WordType{1});
        }
        BigInt result;
        DivScratchStorage scratch;
        divide_impl(
            std::span<WordType const>{data},
            lw,
            std::span<WordType const>{rhs.data},
            rw,
            std::span<WordType>{result.data},
            std::span<WordType>{},
            scratch.view()
        );
        result.mask_top_word();
        return result;
    }

    constexpr BigInt umod(BigInt const& rhs) const {
        unsigned lw = active_words(*this);
        unsigned rw = active_words(rhs);
        if (lw == 0 || ucompare(rhs) < 0) {
            return *this;
        }
        if (*this == rhs) {
            return BigInt{};
        }
        BigInt result;
        DivScratchStorage scratch;
        divide_impl(
            std::span<WordType const>{data},
            lw,
            std::span<WordType const>{rhs.data},
            rw,
            std::span<WordType>{},
            std::span<WordType>{result.data},
            scratch.view()
        );
        result.mask_top_word();
        return result;
    }

    constexpr BigInt sdiv(BigInt const& rhs) const {
        bool ln = is_negative();
        bool rn = rhs.is_negative();
        BigInt a = ln ? -(*this) : *this;
        BigInt b = rn ? -rhs : rhs;
        BigInt q = a.udiv(b);
        return (ln ^ rn) ? -q : q;
    }

    constexpr BigInt smod(BigInt const& rhs) const {
        bool ln = is_negative();
        BigInt a = ln ? -(*this) : *this;
        BigInt b = rhs.is_negative() ? -rhs : rhs;
        BigInt r = a.umod(b);
        return ln ? -r : r;
    }
};

// Free-function shift wrappers kept for source-level compat with int_base.hpp.
template <size_t BW>
constexpr void shift_right_logical(BigInt<BW>& val, size_t amount) {
    detail::shift_right_logical(static_cast<BigIntMutRef>(val), amount);
}

template <size_t BW>
constexpr void shift_right_arith(BigInt<BW>& val, size_t amount) {
    detail::shift_right_arith(static_cast<BigIntMutRef>(val), amount);
}

template <size_t BW>
constexpr void shift_left(BigInt<BW>& val, size_t amount) {
    detail::shift_left(static_cast<BigIntMutRef>(val), amount);
}
}  // namespace coconext::types::detail

#endif  // COCONEXT_BIGINT_HPP

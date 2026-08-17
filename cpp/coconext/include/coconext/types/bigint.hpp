#ifndef COCONEXT_BIGINT_HPP
#define COCONEXT_BIGINT_HPP

// The multi-word arithmetic kernels below (the `tc*` word-array primitives, the
// Knuth division algorithm, and the division driver) are derived from LLVM's
// APInt implementation (llvm/lib/Support/APInt.cpp). The owning Int/DynInt
// representations expose non-owning views (WordConstSpan / WordSpan) to these
// kernels, which carry no notion of ownership or numeric type.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Substantial modifications and non-LLVM-derived code provided under the below
// license.
// SPDX-FileCopyrightText: 2026 Kaleb Barrett
// SPDX-License-Identifier: BSD-3-Clause

#include <algorithm>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace coconext::types::detail {

using Word = uint64_t;
inline constexpr unsigned word_bits = 64;

// ---------------------------------------------------------------------------
// Word-array kernels (derived from LLVM APInt tc* primitives)
//
// Little-endian (word 0 is least significant). They carry no notion of bit
// width; the span-level drivers below provide the applicable physical width.
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
    size_t parts = std::min(dst.size(), rhs.size());
    for (size_t i = 0; i < parts; ++i) {
        overflow |= tc_multiply_part(dst.subspan(i), lhs, rhs[i], 0, /*add=*/i != 0);
    }

    // The first partial product overwrites its output and each later partial
    // product extends that initialized region by one word. Clear only a tail
    // that cannot contain any part of the product. This also fully defines the
    // result when rhs is empty, without requiring a zero-initialized dst.
    size_t initialized = 0;
    if (parts != 0) {
        initialized = std::min(dst.size(), lhs.size());
        initialized += std::min(parts, dst.size() - initialized);
    }
    for (size_t i = initialized; i < dst.size(); ++i) {
        dst[i] = 0;
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

struct DivideScratch {
    std::span<DivLimb> U;
    std::span<DivLimb> V;
    std::span<DivLimb> Q;
    std::span<DivLimb> R;
};

// Load an operand into the mutable Knuth scratch, optionally taking its
// two's-complement magnitude as it is loaded. This is the only copy required
// by wide signed division: negative operands do not need an intermediate
// owning unsigned value.
constexpr unsigned load_divide_operand(
    std::span<Word const> input,
    unsigned input_words,
    size_t input_bits,
    std::span<DivLimb> output,
    bool negate_input
) {
    bool carry = negate_input;
    for (unsigned i = 0; i < input_words; ++i) {
        Word word = negate_input ? ~input[i] : input[i];
        if (carry) {
            ++word;
            carry = word == 0;
        }
        if (i + 1 == input.size() && input_bits % word_bits != 0) {
            word &= (Word{1} << (input_bits % word_bits)) - 1;
        }
        word_to_limbs(word, &output[i * limbs_per_word]);
    }

    unsigned limbs = input_words * limbs_per_word;
    while (limbs != 0 && output[limbs - 1] == 0) {
        --limbs;
    }
    return limbs;
}

constexpr int compare_divide_operands(
    std::span<DivLimb const> lhs,
    unsigned lhs_limbs,
    std::span<DivLimb const> rhs,
    unsigned rhs_limbs
) {
    if (lhs_limbs != rhs_limbs) {
        return lhs_limbs > rhs_limbs ? 1 : -1;
    }
    while (lhs_limbs != 0) {
        --lhs_limbs;
        if (lhs[lhs_limbs] != rhs[lhs_limbs]) {
            return lhs[lhs_limbs] > rhs[lhs_limbs] ? 1 : -1;
        }
    }
    return 0;
}

constexpr void store_divide_result(
    std::span<Word> output, std::span<DivLimb const> input, unsigned input_limbs
) {
    unsigned limb = 0;
    for (Word& word : output) {
        word = 0;
        for (unsigned k = 0; k < limbs_per_word && limb < input_limbs; ++k, ++limb) {
            word |= static_cast<Word>(input[limb]) << (k * limb_bits);
        }
    }
}

// Divide magnitudes already loaded into U and V. lhs must be greater than rhs;
// the caller handles zero, less-than, and equality without entering Knuth's
// algorithm.
constexpr void divide_loaded(
    unsigned lhs_limbs,
    unsigned rhs_limbs,
    std::span<Word> quotient,
    std::span<Word> remainder,
    DivideScratch scratch
) {
    unsigned n = rhs_limbs;
    unsigned m = lhs_limbs - n;
    scratch.U[lhs_limbs] = 0;

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
        if (!remainder.empty()) {
            scratch.R[0] = rem;
        }
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
        store_divide_result(quotient, scratch.Q, m + 1);
    }
    if (!remainder.empty()) {
        store_divide_result(remainder, scratch.R, n);
    }
}

// Single-allocation heap scratch for wide dynamic division.
struct OwnedDivScratch {
    std::unique_ptr<DivLimb[]> buf;
    DivideScratch view;
};

inline OwnedDivScratch make_owned_div_scratch(size_t lhs_limbs, size_t rhs_limbs) {
    size_t u_len = lhs_limbs + 1;
    size_t v_len = rhs_limbs;
    size_t q_len = lhs_limbs;
    size_t r_len = rhs_limbs;
    size_t total = u_len + v_len + q_len + r_len;
    std::unique_ptr<DivLimb[]> buf{new DivLimb[total]};
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
// WordConstSpan / WordSpan: non-owning views over word storage.
// Storage-agnostic algorithms are free functions taking these views. Int and
// DynInt form the views explicitly, so every primitive has one implementation
// regardless of how the words are stored.
// ---------------------------------------------------------------------------

class WordConstSpan {
    std::span<Word const> data_;
    size_t bit_width_;

  public:
    constexpr WordConstSpan(std::span<Word const> d, size_t bw)
        : data_(d), bit_width_(bw) {}

    constexpr std::span<Word const> data() const { return data_; }
    constexpr size_t bit_width() const { return bit_width_; }
    constexpr size_t num_words() const { return data_.size(); }
    constexpr Word word(size_t i) const { return data_[i]; }
};

class WordSpan {
    std::span<Word> data_;
    size_t bit_width_;

  public:
    constexpr WordSpan(std::span<Word> d, size_t bw) : data_(d), bit_width_(bw) {}

    constexpr operator WordConstSpan() const {
        return WordConstSpan{
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

constexpr bool is_negative(WordConstSpan v) {
    if (v.bit_width() == 0) {
        return false;
    }
    unsigned sign_bit = (v.bit_width() - 1) % word_bits;
    return (v.data().back() >> sign_bit) & 1;
}

constexpr unsigned active_words(WordConstSpan v) {
    auto d = v.data();
    for (size_t i = d.size(); i > 0; --i) {
        if (d[i - 1] != 0) {
            return static_cast<unsigned>(i);
        }
    }
    return 0;
}

constexpr void check_same_width(WordConstSpan a, WordConstSpan b) {
    if (a.bit_width() != b.bit_width()) {
        throw std::invalid_argument("bit width mismatch");
    }
}

// TODO extended_word is used to deal with different sized arguments. Indexes above an
// arguments size gets the sign extended value. Indexes on the boundary get the sign
// extended applied to their upper bits. This is less than optimal, it would be better to
// simply handle the differences in different branches and use `if constexpr` to select the
// correct branches on const-bounded types.

constexpr Word extended_word(WordConstSpan value, size_t index, bool signed_value) {
    bool negative = signed_value && is_negative(value);
    if (index >= value.num_words()) {
        return negative ? ~Word{0} : Word{0};
    }
    Word word = value.word(index);
    if (negative && index + 1 == value.num_words()) {
        unsigned valid_bits = value.bit_width() % word_bits;
        if (valid_bits != 0) {
            word |= ~Word{0} << valid_bits;
        }
    }
    return word;
}

constexpr int ucompare(WordConstSpan a, WordConstSpan b) {
    size_t words = std::max(a.num_words(), b.num_words());
    while (words > 0) {
        --words;
        Word aw = extended_word(a, words, false);
        Word bw = extended_word(b, words, false);
        if (aw != bw) {
            return aw > bw ? 1 : -1;
        }
    }
    return 0;
}

constexpr int scompare(WordConstSpan a, WordConstSpan b) {
    bool an = is_negative(a);
    bool bn = is_negative(b);
    if (an != bn) {
        return an ? -1 : 1;
    }
    size_t words = std::max(a.num_words(), b.num_words());
    while (words > 0) {
        --words;
        Word aw = extended_word(a, words, true);
        Word bw = extended_word(b, words, true);
        if (aw != bw) {
            return aw > bw ? 1 : -1;
        }
    }
    return 0;
}

constexpr size_t count_trailing_zeros(WordConstSpan v) {
    auto d = v.data();
    for (size_t i = 0; i < d.size(); ++i) {
        if (d[i] != 0) {
            return (i * word_bits) + std::countr_zero(d[i]);
        }
    }
    return v.bit_width();
}

constexpr size_t count_leading_zeros(WordConstSpan v) {
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

constexpr size_t popcount(WordConstSpan v) {
    size_t n = 0;
    for (Word w : v.data()) {
        n += std::popcount(w);
    }
    return n;
}

constexpr bool get_bit(WordConstSpan v, size_t index) {
    return (v.data()[index / word_bits] >> (index % word_bits)) & 1;
}

constexpr void set_bit(WordSpan v, size_t index, bool value) {
    Word mask = Word{1} << (index % word_bits);
    Word& word = v.data()[index / word_bits];
    if (value) {
        word |= mask;
    } else {
        word &= ~mask;
    }
}

constexpr Word extract_bits(WordConstSpan v, size_t first, unsigned count) {
    if (count == 0 || first >= v.bit_width()) {
        return 0;
    }
    count = static_cast<unsigned>(std::min<size_t>(count, v.bit_width() - first));
    size_t word_index = first / word_bits;
    unsigned bit_index = first % word_bits;
    Word result = v.data()[word_index] >> bit_index;
    if (bit_index != 0 && count > word_bits - bit_index && word_index + 1 < v.data().size())
    {
        result |= v.data()[word_index + 1] << (word_bits - bit_index);
    }
    return count == word_bits ? result : result & ((Word{1} << count) - 1);
}

// ---- Mutating operations on a view ----

constexpr void clear_unused_bits(WordSpan v) {
    auto d = v.data();
    unsigned valid_bits = v.bit_width() % word_bits;
    if (!d.empty() && valid_bits != 0) {
        d.back() &= (Word{1} << valid_bits) - 1;
    }
}

template <typename IntT>
    requires(std::is_integral_v<IntT> && sizeof(IntT) <= sizeof(Word))
constexpr void load_native(WordSpan dst, IntT value) {
    auto words = dst.data();
    if (words.empty()) {
        return;
    }
    words[0] = static_cast<Word>(value);
    Word extension = 0;
    if constexpr (std::is_signed_v<IntT>) {
        if (value < 0) {
            extension = ~Word{0};
        }
    }
    for (size_t i = 1; i < words.size(); ++i) {
        words[i] = extension;
    }
    clear_unused_bits(dst);
}

// Copy the low bits shared by both widths and zero-fill the rest of dst.
// This is the common implementation behind zero extension and truncation.
constexpr void copy_bits(WordSpan dst, WordConstSpan src) {
    auto d = dst.data();
    auto s = src.data();
    size_t common_words = std::min(d.size(), s.size());
    for (size_t i = 0; i < common_words; ++i) {
        d[i] = s[i];
    }
    for (size_t i = common_words; i < d.size(); ++i) {
        d[i] = 0;
    }
    clear_unused_bits(dst);
}

constexpr void truncate(WordSpan dst, WordConstSpan src) {
    if (dst.bit_width() > src.bit_width()) {
        throw std::invalid_argument("truncate cannot widen");
    }
    copy_bits(dst, src);
}

template <typename DigitToChar>
std::string format_power_of_two(
    WordConstSpan src, size_t bits_per_digit, size_t num_chars, DigitToChar digit_to_char
) {
    std::string result;
    result.reserve(num_chars);
    for (size_t i = num_chars; i > 0; --i) {
        uint8_t digit = static_cast<uint8_t>(
            extract_bits(src, (i - 1) * bits_per_digit, bits_per_digit)
        );
        result.push_back(digit_to_char(digit));
    }
    return result;
}

constexpr void negate(WordSpan v);

inline std::string format_decimal(WordConstSpan src, bool signed_value) {
    if (src.bit_width() == 0) {
        return "";
    }
    bool negative = signed_value && is_negative(src);
    std::vector<Word> magnitude(src.data().begin(), src.data().end());
    WordSpan mag{magnitude, src.bit_width()};
    if (negative) {
        negate(mag);
    }
    if (active_words(mag) == 0) {
        return "0";
    }

    std::string digits;
    while (active_words(mag) != 0) {
        uint64_t remainder = 0;
        for (size_t i = magnitude.size(); i > 0; --i) {
            uint64_t high = (remainder << 32) | (magnitude[i - 1] >> 32);
            uint64_t high_quotient = high / 10;
            remainder = high % 10;
            uint64_t low = (remainder << 32) | (magnitude[i - 1] & 0xFFFFFFFFULL);
            uint64_t low_quotient = low / 10;
            remainder = low % 10;
            magnitude[i - 1] = (high_quotient << 32) | low_quotient;
        }
        digits.push_back(static_cast<char>('0' + remainder));
    }
    if (negative) {
        digits.push_back('-');
    }
    std::reverse(digits.begin(), digits.end());
    return digits;
}

constexpr void add_assign(WordSpan dst, WordConstSpan rhs) {
    check_same_width(dst, rhs);
    tc_add(dst.data(), rhs.data(), 0);
    clear_unused_bits(dst);
}

constexpr void sub_assign(WordSpan dst, WordConstSpan rhs) {
    check_same_width(dst, rhs);
    tc_subtract(dst.data(), rhs.data(), 0);
    clear_unused_bits(dst);
}

template <bool SignedOperands>
constexpr void add_extended(WordSpan dst, WordConstSpan lhs, WordConstSpan rhs) {
    Word carry = 0;
    for (size_t i = 0; i < dst.num_words(); ++i) {
        Word a = extended_word(lhs, i, SignedOperands);
        Word b = extended_word(rhs, i, SignedOperands);
        Word sum = a + b;
        Word next_carry = sum < a;
        Word with_carry = sum + carry;
        next_carry |= with_carry < sum;
        dst.data()[i] = with_carry;
        carry = next_carry;
    }
    clear_unused_bits(dst);
}

template <bool SignedOperands>
constexpr void sub_extended(WordSpan dst, WordConstSpan lhs, WordConstSpan rhs) {
    Word borrow = 0;
    for (size_t i = 0; i < dst.num_words(); ++i) {
        Word a = extended_word(lhs, i, SignedOperands);
        Word b = extended_word(rhs, i, SignedOperands);
        Word difference = a - b;
        Word next_borrow = a < b;
        Word with_borrow = difference - borrow;
        next_borrow |= difference < borrow;
        dst.data()[i] = with_borrow;
        borrow = next_borrow;
    }
    clear_unused_bits(dst);
}

constexpr void and_assign(WordSpan dst, WordConstSpan rhs) {
    check_same_width(dst, rhs);
    auto d = dst.data();
    auto s = rhs.data();
    for (size_t i = 0; i < d.size(); ++i) {
        d[i] &= s[i];
    }
}

constexpr void or_assign(WordSpan dst, WordConstSpan rhs) {
    check_same_width(dst, rhs);
    auto d = dst.data();
    auto s = rhs.data();
    for (size_t i = 0; i < d.size(); ++i) {
        d[i] |= s[i];
    }
}

constexpr void xor_assign(WordSpan dst, WordConstSpan rhs) {
    check_same_width(dst, rhs);
    auto d = dst.data();
    auto s = rhs.data();
    for (size_t i = 0; i < d.size(); ++i) {
        d[i] ^= s[i];
    }
}

constexpr void bitnot(WordSpan v) {
    for (auto& w : v.data()) {
        w = ~w;
    }
    clear_unused_bits(v);
}

// Two's-complement negation: ~x + 1.
constexpr void negate(WordSpan v) {
    Word carry = 1;
    for (auto& w : v.data()) {
        w = ~w;
        Word sum = w + carry;
        carry = (sum < w) ? 1 : 0;
        w = sum;
    }
    clear_unused_bits(v);
}

constexpr void shift_left(WordSpan v, size_t amount) {
    if (amount >= v.bit_width()) {
        for (auto& w : v.data()) {
            w = 0;
        }
        return;
    }
    tc_shift_left(v.data(), static_cast<unsigned>(amount));
    clear_unused_bits(v);
}

constexpr void shift_right_logical(WordSpan v, size_t amount) {
    if (amount >= v.bit_width()) {
        for (auto& w : v.data()) {
            w = 0;
        }
        return;
    }
    tc_shift_right(v.data(), static_cast<unsigned>(amount));
}

constexpr void shift_right_arith(WordSpan v, size_t amount) {
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

constexpr void reverse_bits_bigint(WordSpan dst, WordConstSpan src) {
    check_same_width(dst, src);
    auto d = dst.data();
    auto s = src.data();
    size_t n = s.size();
    for (size_t i = 0; i < n; ++i) {
        Word w = s[n - 1 - i];
        w = ((w & 0xaaaaaaaaaaaaaaaaULL) >> 1) | ((w & 0x5555555555555555ULL) << 1);
        w = ((w & 0xccccccccccccccccULL) >> 2) | ((w & 0x3333333333333333ULL) << 2);
        w = ((w & 0xf0f0f0f0f0f0f0f0ULL) >> 4) | ((w & 0x0f0f0f0f0f0f0f0fULL) << 4);
        w = ((w & 0xff00ff00ff00ff00ULL) >> 8) | ((w & 0x00ff00ff00ff00ffULL) << 8);
        w = ((w & 0xffff0000ffff0000ULL) >> 16) | ((w & 0x0000ffff0000ffffULL) << 16);
        d[i] = (w >> 32) | (w << 32);
    }
    if (n * word_bits > dst.bit_width()) {
        tc_shift_right(d, static_cast<unsigned>(n * word_bits - dst.bit_width()));
    }
    clear_unused_bits(dst);
}

// dst = lhs * rhs (truncated). dst must be disjoint from operands.
constexpr void multiply(WordSpan dst, WordConstSpan lhs, WordConstSpan rhs) {
    check_same_width(dst, lhs);
    check_same_width(dst, rhs);
    tc_multiply(dst.data(), lhs.data(), rhs.data());
    clear_unused_bits(dst);
}

constexpr void multiply_unsigned(WordSpan dst, WordConstSpan lhs, WordConstSpan rhs) {
    tc_multiply(dst.data(), lhs.data(), rhs.data());
    clear_unused_bits(dst);
}

constexpr void subtract_shifted(WordSpan dst, WordConstSpan rhs, size_t shift) {
    size_t word_shift = shift / word_bits;
    unsigned bit_shift = shift % word_bits;
    Word borrow = 0;
    for (size_t i = 0; i < dst.num_words(); ++i) {
        Word shifted = 0;
        if (i >= word_shift) {
            size_t source_index = i - word_shift;
            if (source_index < rhs.num_words()) {
                shifted = rhs.word(source_index) << bit_shift;
            }
            if (bit_shift != 0 && source_index > 0 && source_index - 1 < rhs.num_words()) {
                shifted |= rhs.word(source_index - 1) >> (word_bits - bit_shift);
            }
        }
        Word original = dst.data()[i];
        Word difference = original - shifted;
        Word next_borrow = original < shifted;
        Word with_borrow = difference - borrow;
        next_borrow |= difference < borrow;
        dst.data()[i] = with_borrow;
        borrow = next_borrow;
    }
    clear_unused_bits(dst);
}

constexpr void multiply_signed(WordSpan dst, WordConstSpan lhs, WordConstSpan rhs) {
    multiply_unsigned(dst, lhs, rhs);
    if (is_negative(lhs)) {
        subtract_shifted(dst, rhs, lhs.bit_width());
    }
    if (is_negative(rhs)) {
        subtract_shifted(dst, lhs, rhs.bit_width());
    }
}

// Unsigned quotient and remainder. Storage ownership and scratch allocation
// stay with the caller. The quotient must be wide enough for lhs and the
// remainder must be wide enough for rhs.
constexpr void divide_unsigned(
    WordSpan quotient,
    WordSpan remainder,
    WordConstSpan lhs,
    WordConstSpan rhs,
    DivideScratch scratch
) {
    unsigned rhs_words = active_words(rhs);
    if (rhs_words == 0) {
        throw std::domain_error("Division by zero");
    }

    unsigned lhs_words = active_words(lhs);
    int comparison = ucompare(lhs, rhs);
    if (lhs_words == 0 || comparison < 0) {
        for (Word& word : quotient.data()) {
            word = 0;
        }
        copy_bits(remainder, lhs);
        return;
    }
    if (comparison == 0) {
        for (Word& word : quotient.data()) {
            word = 0;
        }
        for (Word& word : remainder.data()) {
            word = 0;
        }
        set_bit(quotient, 0, true);
        return;
    }

    unsigned lhs_limbs =
        load_divide_operand(lhs.data(), lhs_words, lhs.bit_width(), scratch.U, false);
    unsigned rhs_limbs =
        load_divide_operand(rhs.data(), rhs_words, rhs.bit_width(), scratch.V, false);
    divide_loaded(lhs_limbs, rhs_limbs, quotient.data(), remainder.data(), scratch);
    clear_unused_bits(quotient);
    clear_unused_bits(remainder);
}

// Truncating signed division. The remainder follows the dividend's sign.
constexpr void divide_signed(
    WordSpan quotient,
    WordSpan remainder,
    WordConstSpan lhs,
    WordConstSpan rhs,
    DivideScratch scratch
) {
    bool lhs_negative = is_negative(lhs);
    bool rhs_negative = is_negative(rhs);

    unsigned lhs_limbs = load_divide_operand(
        lhs.data(),
        static_cast<unsigned>(lhs.num_words()),
        lhs.bit_width(),
        scratch.U,
        lhs_negative
    );
    unsigned rhs_limbs = load_divide_operand(
        rhs.data(),
        static_cast<unsigned>(rhs.num_words()),
        rhs.bit_width(),
        scratch.V,
        rhs_negative
    );
    if (rhs_limbs == 0) {
        throw std::domain_error("Division by zero");
    }

    int comparison = compare_divide_operands(scratch.U, lhs_limbs, scratch.V, rhs_limbs);
    if (lhs_limbs == 0 || comparison < 0) {
        for (Word& word : quotient.data()) {
            word = 0;
        }
        store_divide_result(remainder.data(), scratch.U, lhs_limbs);
    } else if (comparison == 0) {
        for (Word& word : quotient.data()) {
            word = 0;
        }
        for (Word& word : remainder.data()) {
            word = 0;
        }
        set_bit(quotient, 0, true);
    } else {
        divide_loaded(lhs_limbs, rhs_limbs, quotient.data(), remainder.data(), scratch);
    }

    if (lhs_negative != rhs_negative) {
        negate(quotient);
    }
    if (lhs_negative) {
        negate(remainder);
    }
}

// Floor signed division. Modulo follows the divisor's sign.
constexpr void divide_modulo(
    WordSpan quotient,
    WordSpan modulo,
    WordConstSpan lhs,
    WordConstSpan rhs,
    DivideScratch scratch
) {
    divide_signed(quotient, modulo, lhs, rhs, scratch);
    if (active_words(modulo) != 0 && is_negative(lhs) != is_negative(rhs)) {
        Word borrow = 1;
        for (Word& word : quotient.data()) {
            Word previous = word;
            word -= borrow;
            borrow = word > previous;
        }
        clear_unused_bits(quotient);
        add_extended<true>(modulo, modulo, rhs);
        clear_unused_bits(modulo);
    }
}

// Parse a decimal or (unsigned) 0x-prefixed hex literal into `dst`. Requires
// dst to be zero-valued on entry. Accepts ' and _ as digit separators.
// Throws std::invalid_argument on malformed input and std::out_of_range if
// the value doesn't fit in dst.bit_width() bits.
constexpr void parse_into(WordSpan dst, std::string_view str) {
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
                throw std::out_of_range("Hexadecimal literal exceeds bit width");
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
                throw std::out_of_range("Decimal literal exceeds bit width");
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
constexpr void load_int128(WordSpan dst, __int128_t val) {
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

constexpr void load_uint128(WordSpan dst, __uint128_t val) {
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

}  // namespace coconext::types::detail

#endif  // COCONEXT_BIGINT_HPP

#ifndef COCONEXT_DYN_INT_BASE_HPP
#define COCONEXT_DYN_INT_BASE_HPP

#include <algorithm>
#include <coconext/types/bigint.hpp>
#include <coconext/types/int_base.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace coconext::types::detail {

// DynBits: the runtime-width sibling of Bits<W>. Same operations, same
// semantics; the width is a constructor argument instead of a template
// parameter, so it cannot enforce width agreement at compile time (see the
// mismatch policy below).
//
// Storage is a small-buffer union: values up to sbo_bits live inline, wider
// ones on the heap. Both arms are word arrays, so words() spans whichever is
// active and every operation is a kernel call over that span -- the union is
// confined to one accessor rather than branching at each op.
//
// Not constexpr: the heap arm allocates.
class DynBits {
  public:
    // The single knob. Raising it widens the no-allocation band and grows
    // sizeof(DynBits) by 8 bytes per word; nothing else has to change.
    static constexpr size_t sbo_words = 2;
    static constexpr size_t sbo_bits = sbo_words * word_bits;

    static_assert(sbo_words >= 1, "the inline buffer needs at least one word");

    explicit DynBits(size_t width) : width_(width) {
        if (is_inline()) {
            for (size_t i = 0; i < sbo_words; ++i) {
                storage_.inline_[i] = 0;
            }
        } else {
            std::construct_at(&storage_.heap_, new Word[num_words()]());
        }
    }

    template <NativeInteger IntT>
    DynBits(size_t width, IntT val) : DynBits(width) {
        if (width == 0) {
            throw std::invalid_argument("DynBits(0) has no integer representation");
        }
        auto w = words();
        w[0] = static_cast<Word>(static_cast<uint64_t>(val));
        if constexpr (std::is_signed_v<IntT>) {
            if (val < 0) {
                for (size_t i = 1; i < w.size(); ++i) {
                    w[i] = ~Word(0);
                }
            }
        }
        clear_unused_bits(mut());
    }

#if defined(__SIZEOF_INT128__)
    DynBits(size_t width, __int128_t val) : DynBits(width) { load_int128(mut(), val); }
    DynBits(size_t width, __uint128_t val) : DynBits(width) { load_uint128(mut(), val); }
#endif

    // Parses decimal or 0x hex, with ' and _ accepted as digit separators.
    // Throws std::out_of_range if the literal does not fit in `width` bits.
    DynBits(size_t width, std::string_view str) : DynBits(width) { parse_into(mut(), str); }

    // Widening copy of a static Bits<W>, so the two families interoperate.
    template <size_t W>
    explicit DynBits(Bits<W> const& src) : DynBits(W) {
        for (size_t i = 0; i < W; ++i) {
            if (src.get_bit(i)) {
                set_bit(i, true);
            }
        }
    }

    DynBits(DynBits const& other) : DynBits(other.width_) {
        auto dst = words();
        auto src = other.words();
        for (size_t i = 0; i < dst.size(); ++i) {
            dst[i] = src[i];
        }
    }

    DynBits(DynBits&& other) noexcept : width_(other.width_) {
        if (is_inline()) {
            for (size_t i = 0; i < sbo_words; ++i) {
                storage_.inline_[i] = other.storage_.inline_[i];
            }
        } else {
            std::construct_at(&storage_.heap_, std::move(other.storage_.heap_));
            // Leave the source owning nothing but still destructible.
            other.width_ = 0;
        }
    }

    DynBits& operator=(DynBits const& other) {
        if (this != &other) {
            DynBits tmp(other);
            *this = std::move(tmp);
        }
        return *this;
    }

    DynBits& operator=(DynBits&& other) noexcept {
        if (this != &other) {
            destroy();
            width_ = other.width_;
            if (is_inline()) {
                for (size_t i = 0; i < sbo_words; ++i) {
                    storage_.inline_[i] = other.storage_.inline_[i];
                }
            } else {
                std::construct_at(&storage_.heap_, std::move(other.storage_.heap_));
                other.width_ = 0;
            }
        }
        return *this;
    }

    ~DynBits() { destroy(); }

    size_t width() const { return width_; }

    size_t num_words() const { return (width_ + word_bits - 1) / word_bits; }

    bool get_bit(size_t index) const { return detail::get_bit(cref(), index); }

    void set_bit(size_t index, bool val) {
        auto w = words();
        Word mask = Word(1) << (index % word_bits);
        if (val) {
            w[index / word_bits] |= mask;
        } else {
            w[index / word_bits] &= ~mask;
        }
    }

    BigIntConstRef raw() const {
        if (width_ == 0) {
            throw std::domain_error("raw() on a null DynBits; it has no value");
        }
        return cref();
    }

    size_t popcount() const { return detail::popcount(cref()); }
    size_t count_leading_zeros() const { return detail::count_leading_zeros(cref()); }
    size_t count_trailing_zeros() const { return detail::count_trailing_zeros(cref()); }

    // Equality is total: differing widths compare unequal rather than throwing,
    // so a DynBits can be a hash key without width-matched lookups blowing up.
    // The orderings below cannot do the same -- "less than" across widths has no
    // answer that is right for both zero- and sign-extension -- so they throw.
    bool operator==(DynBits const& other) const {
        if (width_ != other.width_) {
            return false;
        }
        auto a = words();
        auto b = other.words();
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(DynBits const& other) const { return !(*this == other); }

    bool ult(DynBits const& other) const { return ucompare(cref(), other.cref()) < 0; }
    bool ule(DynBits const& other) const { return !other.ult(*this); }
    bool ugt(DynBits const& other) const { return other.ult(*this); }
    bool uge(DynBits const& other) const { return !ult(other); }

    bool slt(DynBits const& other) const { return scompare(cref(), other.cref()) < 0; }
    bool sle(DynBits const& other) const { return !other.slt(*this); }
    bool sgt(DynBits const& other) const { return other.slt(*this); }
    bool sge(DynBits const& other) const { return !slt(other); }

    DynBits operator&(DynBits const& other) const {
        DynBits result(*this);
        and_assign(result.mut(), other.cref());
        return result;
    }

    DynBits operator|(DynBits const& other) const {
        DynBits result(*this);
        or_assign(result.mut(), other.cref());
        return result;
    }

    DynBits operator^(DynBits const& other) const {
        DynBits result(*this);
        xor_assign(result.mut(), other.cref());
        return result;
    }

    DynBits operator~() const {
        DynBits result(*this);
        bitnot(result.mut());
        return result;
    }

    DynBits operator<<(size_t amount) const {
        DynBits result(*this);
        shift_left(result.mut(), amount);
        return result;
    }

    DynBits srl(size_t amount) const {
        DynBits result(*this);
        shift_right_logical(result.mut(), amount);
        return result;
    }

    DynBits sra(size_t amount) const {
        DynBits result(*this);
        shift_right_arith(result.mut(), amount);
        return result;
    }

    // Width-changing operations, mirroring the Bits<W> members.

    DynBits zero_extend(size_t target) const {
        if (target < width_) {
            throw std::invalid_argument("zero_extend cannot narrow");
        }
        return widened(target);
    }

    DynBits sign_extend(size_t target) const {
        if (target < width_) {
            throw std::invalid_argument("sign_extend cannot narrow");
        }
        DynBits result = widened(target);
        if (width_ > 0 && target > width_ && get_bit(width_ - 1)) {
            for (size_t i = width_; i < target; ++i) {
                result.set_bit(i, true);
            }
        }
        return result;
    }

    DynBits truncate(size_t target) const {
        if (target > width_) {
            throw std::invalid_argument("truncate cannot widen");
        }
        DynBits result(target);
        for (size_t i = 0; i < target; ++i) {
            if (get_bit(i)) {
                result.set_bit(i, true);
            }
        }
        return result;
    }

    DynBits saturate_unsigned(size_t target) const {
        if (target >= width_) {
            return zero_extend(target);
        }
        if (target == 0) {
            return DynBits(0);
        }
        for (size_t i = target; i < width_; ++i) {
            if (get_bit(i)) {
                return ~DynBits(target);
            }
        }
        return truncate(target);
    }

    DynBits saturate_signed(size_t target) const {
        if (target >= width_) {
            return sign_extend(target);
        }
        if (target == 0) {
            return DynBits(0);
        }
        bool negative = width_ > 0 && get_bit(width_ - 1);
        bool in_range = true;
        for (size_t i = target - 1; i < width_; ++i) {
            if (get_bit(i) != negative) {
                in_range = false;
                break;
            }
        }
        if (in_range) {
            return truncate(target);
        }
        DynBits limit(target);
        limit.set_bit(target - 1, true);
        return negative ? limit : ~limit;
    }

    std::string to_binary_string() const {
        std::string res;
        res.reserve(width_);
        for (size_t i = width_; i > 0; --i) {
            res.push_back(get_bit(i - 1) ? '1' : '0');
        }
        return res;
    }

    std::string to_decimal_string(bool is_signed = false) const {
        if (width_ == 0) {
            return "";
        }
        bool negative = is_signed && detail::is_negative(cref());
        DynBits mag(*this);
        if (negative) {
            negate(mag.mut());
        }
        if (active_words(mag.cref()) == 0) {
            return "0";
        }
        DynBits ten(width_, uint64_t{10});
        std::string digits;
        while (active_words(mag.cref()) != 0) {
            DynBits rem = same_width_umod(mag, ten);
            digits.push_back(static_cast<char>('0' + rem.words()[0]));
            mag = same_width_udiv(mag, ten);
        }
        if (negative) {
            digits.push_back('-');
        }
        std::reverse(digits.begin(), digits.end());
        return digits;
    }

    std::string to_hexadecimal_string() const {
        char const hex_digits[] = "0123456789abcdef";
        return digits_from_bits(4, (width_ + 3) / 4, [&](uint8_t d) {
            return hex_digits[d];
        });
    }

    std::string to_octal_string() const {
        return digits_from_bits(3, (width_ + 2) / 3, [](uint8_t d) {
            return static_cast<char>('0' + d);
        });
    }

  private:
    friend struct dyn_same_width;

    size_t width_;
    union Storage {
        Word inline_[sbo_words];
        std::unique_ptr<Word[]> heap_;
        Storage() {}
        ~Storage() {}
    } storage_;

    bool is_inline() const { return width_ <= sbo_bits; }

    void destroy() {
        if (!is_inline()) {
            std::destroy_at(&storage_.heap_);
        }
    }

    std::span<Word> words() {
        return is_inline() ? std::span<Word>{storage_.inline_, num_words()}
                           : std::span<Word>{storage_.heap_.get(), num_words()};
    }

    std::span<Word const> words() const {
        return is_inline() ? std::span<Word const>{storage_.inline_, num_words()}
                           : std::span<Word const>{storage_.heap_.get(), num_words()};
    }

    BigIntConstRef cref() const { return BigIntConstRef{words(), width_}; }
    BigIntMutRef mut() { return BigIntMutRef{words(), width_}; }

    DynBits widened(size_t target) const {
        DynBits result(target);
        auto dst = result.words();
        auto src = words();
        for (size_t i = 0; i < src.size() && i < dst.size(); ++i) {
            dst[i] = src[i];
        }
        clear_unused_bits(result.mut());
        return result;
    }

    // Same-width primitives. Private for the same reason Bits' are: wrapping
    // arithmetic has no business on the public surface of an exact-width type.
    static DynBits same_width_add(DynBits const& a, DynBits const& b) {
        DynBits result(a);
        add_assign(result.mut(), b.cref());
        return result;
    }

    static DynBits same_width_sub(DynBits const& a, DynBits const& b) {
        DynBits result(a);
        sub_assign(result.mut(), b.cref());
        return result;
    }

    static DynBits same_width_mul(DynBits const& a, DynBits const& b) {
        DynBits result(a);
        multiply(result.mut(), a.cref(), b.cref());
        return result;
    }

    static DynBits divmod(DynBits const& a, DynBits const& b, bool want_quotient) {
        check_same_width(a.cref(), b.cref());
        if (active_words(b.cref()) == 0) {
            throw std::domain_error("Division by zero");
        }
        unsigned lw = active_words(a.cref());
        unsigned rw = active_words(b.cref());
        if (lw == 0 || ucompare(a.cref(), b.cref()) < 0) {
            return want_quotient ? DynBits(a.width_) : a;
        }
        if (a == b) {
            return want_quotient ? DynBits(a.width_, uint64_t{1}) : DynBits(a.width_);
        }
        DynBits result(a.width_);
        OwnedDivScratch scratch = make_owned_div_scratch(a.num_words() * limbs_per_word);
        std::span<Word> out = result.words();
        std::span<Word> empty{};
        divide_impl(
            a.words(),
            lw,
            b.words(),
            rw,
            want_quotient ? out : empty,
            want_quotient ? empty : out,
            scratch.view
        );
        clear_unused_bits(result.mut());
        return result;
    }

    static DynBits same_width_udiv(DynBits const& a, DynBits const& b) {
        return divmod(a, b, /*want_quotient=*/true);
    }

    static DynBits same_width_umod(DynBits const& a, DynBits const& b) {
        return divmod(a, b, /*want_quotient=*/false);
    }

    static DynBits same_width_sdiv(DynBits const& a, DynBits const& b) {
        bool ln = detail::is_negative(a.cref());
        bool rn = detail::is_negative(b.cref());
        DynBits x(a);
        if (ln) {
            negate(x.mut());
        }
        DynBits y(b);
        if (rn) {
            negate(y.mut());
        }
        DynBits q = same_width_udiv(x, y);
        if (ln ^ rn) {
            negate(q.mut());
        }
        return q;
    }

    static DynBits same_width_smod(DynBits const& a, DynBits const& b) {
        bool ln = detail::is_negative(a.cref());
        DynBits x(a);
        if (ln) {
            negate(x.mut());
        }
        DynBits y(b);
        if (detail::is_negative(b.cref())) {
            negate(y.mut());
        }
        DynBits r = same_width_umod(x, y);
        if (ln) {
            negate(r.mut());
        }
        return r;
    }

    template <typename DigitToChar>
    std::string digits_from_bits(
        size_t bits_per_digit, size_t num_chars, DigitToChar digit_to_char
    ) const {
        std::string res;
        res.reserve(num_chars);
        for (size_t i = num_chars; i > 0; --i) {
            uint8_t d = 0;
            for (size_t j = bits_per_digit; j > 0; --j) {
                size_t bit_idx = (i - 1) * bits_per_digit + (j - 1);
                if (bit_idx < width_) {
                    d = static_cast<uint8_t>((d << 1) | (get_bit(bit_idx) ? 1 : 0));
                } else {
                    d = static_cast<uint8_t>(d << 1);
                }
            }
            res.push_back(digit_to_char(d));
        }
        return res;
    }
};

// The same-width primitive layer, mirroring detail::same_width for Bits<W>.
struct dyn_same_width {
    static DynBits add(DynBits const& a, DynBits const& b) {
        return DynBits::same_width_add(a, b);
    }
    static DynBits sub(DynBits const& a, DynBits const& b) {
        return DynBits::same_width_sub(a, b);
    }
    static DynBits mul(DynBits const& a, DynBits const& b) {
        return DynBits::same_width_mul(a, b);
    }
    static DynBits udiv(DynBits const& a, DynBits const& b) {
        return DynBits::same_width_udiv(a, b);
    }
    static DynBits umod(DynBits const& a, DynBits const& b) {
        return DynBits::same_width_umod(a, b);
    }
    static DynBits sdiv(DynBits const& a, DynBits const& b) {
        return DynBits::same_width_sdiv(a, b);
    }
    static DynBits smod(DynBits const& a, DynBits const& b) {
        return DynBits::same_width_smod(a, b);
    }
};

// Growing arithmetic. Same contract as the Bits<W> forms: the result is wide
// enough to hold every value the operation can produce, and operands are
// extended to it first, so differing operand widths are ordinary rather than
// an error.

inline DynBits add_unsigned(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width()) + 1;
    return dyn_same_width::add(a.zero_extend(wr), b.zero_extend(wr));
}

inline DynBits add_signed(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width()) + 1;
    return dyn_same_width::add(a.sign_extend(wr), b.sign_extend(wr));
}

inline DynBits sub_unsigned(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width()) + 1;
    return dyn_same_width::sub(a.zero_extend(wr), b.zero_extend(wr));
}

inline DynBits sub_signed(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width()) + 1;
    return dyn_same_width::sub(a.sign_extend(wr), b.sign_extend(wr));
}

inline DynBits mul_unsigned(DynBits const& a, DynBits const& b) {
    size_t wr = a.width() + b.width();
    return dyn_same_width::mul(a.zero_extend(wr), b.zero_extend(wr));
}

inline DynBits mul_signed(DynBits const& a, DynBits const& b) {
    size_t wr = a.width() + b.width();
    return dyn_same_width::mul(a.sign_extend(wr), b.sign_extend(wr));
}

inline DynBits div_unsigned(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width()) + 1;
    return dyn_same_width::udiv(a.zero_extend(wr), b.zero_extend(wr))
        .truncate(a.width() + 1);
}

inline DynBits div_signed(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width()) + 1;
    return dyn_same_width::sdiv(a.sign_extend(wr), b.sign_extend(wr))
        .truncate(a.width() + 1);
}

inline DynBits rem_unsigned(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width());
    return dyn_same_width::umod(a.zero_extend(wr), b.zero_extend(wr)).truncate(b.width());
}

// C-style remainder: the sign follows the dividend.
inline DynBits rem_signed(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width()) + 1;
    return dyn_same_width::smod(a.sign_extend(wr), b.sign_extend(wr)).truncate(b.width());
}

// VHDL/Python modulo: the sign follows the divisor.
inline DynBits mod_signed(DynBits const& a, DynBits const& b) {
    size_t wr = std::max(a.width(), b.width()) + 1;
    DynBits ae = a.sign_extend(wr);
    DynBits be = b.sign_extend(wr);
    DynBits r = dyn_same_width::smod(ae, be);
    DynBits zero(wr);
    if (r != zero && (r.slt(zero) != be.slt(zero))) {
        r = dyn_same_width::add(r, be);
    }
    return r.truncate(b.width());
}

inline DynBits negate_signed(DynBits const& a) {
    size_t wr = a.width() + 1;
    return dyn_same_width::sub(DynBits(wr), a.sign_extend(wr));
}

inline DynBits abs_signed(DynBits const& a) {
    size_t wr = a.width() + 1;
    DynBits ext = a.sign_extend(wr);
    if (a.width() > 0 && a.get_bit(a.width() - 1)) {
        return dyn_same_width::sub(DynBits(wr), ext);
    }
    return ext;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_INT_BASE_HPP

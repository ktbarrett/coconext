#ifndef COCONEXT_DYN_INT_BASE_HPP
#define COCONEXT_DYN_INT_BASE_HPP

#include <algorithm>
#include <coconext/types/big_int.hpp>
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
        load_native(mut(), val);
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
        if constexpr (W == 0) {
            return;
        } else if constexpr (Bits<W>::is_wide) {
            detail::zero_extend(mut(), src.raw());
        } else if constexpr (sizeof(typename Bits<W>::IntType) <= sizeof(Word)) {
            load_native(mut(), src.raw());
        } else {
#if defined(__SIZEOF_INT128__)
            load_uint128(mut(), src.raw());
#endif
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

    void set_bit(size_t index, bool val) { detail::set_bit(mut(), index, val); }

    WordConstSpan raw() const {
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
    // Named orderings define their extension explicitly, so differing widths
    // are valid: unsigned comparisons zero-extend and signed comparisons
    // sign-extend.
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
        DynBits result(target);
        detail::sign_extend(result.mut(), cref());
        return result;
    }

    DynBits truncate(size_t target) const {
        if (target > width_) {
            throw std::invalid_argument("truncate cannot widen");
        }
        DynBits result(target);
        detail::truncate(result.mut(), cref());
        return result;
    }

    DynBits saturate_unsigned(size_t target) const {
        if (target >= width_) {
            return zero_extend(target);
        }
        if (target == 0) {
            return DynBits(0);
        }
        if (!detail::fits_unsigned(cref(), target)) {
            return ~DynBits(target);
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
        bool negative = detail::is_negative(cref());
        if (detail::fits_signed(cref(), target)) {
            return truncate(target);
        }
        DynBits limit(target);
        limit.set_bit(target - 1, true);
        return negative ? limit : ~limit;
    }

    std::string to_binary_string() const {
        return format_power_of_two(cref(), 1, width_, [](uint8_t d) {
            return static_cast<char>('0' + d);
        });
    }

    std::string to_decimal_string(bool is_signed = false) const {
        return format_decimal(cref(), is_signed);
    }

    std::string to_hexadecimal_string() const {
        char const hex_digits[] = "0123456789abcdef";
        return format_power_of_two(cref(), 4, (width_ + 3) / 4, [&](uint8_t d) {
            return hex_digits[d];
        });
    }

    std::string to_octal_string() const {
        return format_power_of_two(cref(), 3, (width_ + 2) / 3, [](uint8_t d) {
            return static_cast<char>('0' + d);
        });
    }

  private:
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

  public:
    WordConstSpan cref() const { return WordConstSpan{words(), width_}; }
    WordSpan mut() { return WordSpan{words(), width_}; }

    DynBits widened(size_t target) const {
        DynBits result(target);
        detail::zero_extend(result.mut(), cref());
        return result;
    }

    // Same-width primitives. Private for the same reason Bits' are: wrapping
    // arithmetic has no business on the public surface of an exact-width type.
    static DynBits exact_add(DynBits const& a, DynBits const& b) {
        DynBits result(a);
        add_assign(result.mut(), b.cref());
        return result;
    }

    static DynBits exact_sub(DynBits const& a, DynBits const& b) {
        DynBits result(a);
        sub_assign(result.mut(), b.cref());
        return result;
    }

    static DynBits exact_mul(DynBits const& a, DynBits const& b) {
        DynBits result(a);
        multiply(result.mut(), a.cref(), b.cref());
        return result;
    }

    static std::pair<DynBits, DynBits> exact_divmod(
        DynBits const& a, DynBits const& b, bool is_signed, bool modulo
    ) {
        check_same_width(a.cref(), b.cref());
        DynBits quotient(a.width_);
        DynBits remainder(a.width_);
        OwnedDivScratch scratch = make_owned_div_scratch(a.num_words() * limbs_per_word);
        if (modulo) {
            DynBits lhs_magnitude(a.width_);
            DynBits rhs_magnitude(a.width_);
            detail::divide_modulo(
                quotient.mut(),
                remainder.mut(),
                a.cref(),
                b.cref(),
                lhs_magnitude.mut(),
                rhs_magnitude.mut(),
                scratch.view
            );
        } else if (is_signed) {
            DynBits lhs_magnitude(a.width_);
            DynBits rhs_magnitude(a.width_);
            detail::divide_signed(
                quotient.mut(),
                remainder.mut(),
                a.cref(),
                b.cref(),
                lhs_magnitude.mut(),
                rhs_magnitude.mut(),
                scratch.view
            );
        } else {
            detail::divide_unsigned(
                quotient.mut(), remainder.mut(), a.cref(), b.cref(), scratch.view
            );
        }
        return {std::move(quotient), std::move(remainder)};
    }
};

// Growing arithmetic. Same contract as the Bits<W> forms: the result is wide
// enough to hold every value the operation can produce. Kernels consume each
// operand at its original width, so differing operand widths are ordinary
// rather than an error.

inline DynBits add_unsigned(DynBits const& a, DynBits const& b) {
    DynBits result(std::max(a.width(), b.width()) + 1);
    add_extended(result.mut(), a.cref(), b.cref(), false, false);
    return result;
}

inline DynBits add_signed(DynBits const& a, DynBits const& b) {
    DynBits result(std::max(a.width(), b.width()) + 1);
    add_extended(result.mut(), a.cref(), b.cref(), true, true);
    return result;
}

inline DynBits sub_unsigned(DynBits const& a, DynBits const& b) {
    DynBits result(std::max(a.width(), b.width()) + 1);
    sub_extended(result.mut(), a.cref(), b.cref(), false, false);
    return result;
}

inline DynBits sub_signed(DynBits const& a, DynBits const& b) {
    DynBits result(std::max(a.width(), b.width()) + 1);
    sub_extended(result.mut(), a.cref(), b.cref(), true, true);
    return result;
}

inline DynBits mul_unsigned(DynBits const& a, DynBits const& b) {
    DynBits result(a.width() + b.width());
    detail::multiply_unsigned(result.mut(), a.cref(), b.cref());
    return result;
}

inline DynBits mul_signed(DynBits const& a, DynBits const& b) {
    DynBits result(a.width() + b.width());
    detail::multiply_signed(result.mut(), a.cref(), b.cref());
    return result;
}

inline std::pair<DynBits, DynBits> divrem_impl(
    DynBits const& a, DynBits const& b, bool is_signed, bool modulo
) {
    size_t magnitude_width = std::max(a.width(), b.width()) + 1;
    DynBits quotient(a.width() + 1);
    DynBits remainder(b.width());
    OwnedDivScratch scratch = make_owned_div_scratch(
        (magnitude_width + word_bits - 1) / word_bits * limbs_per_word
    );
    if (is_signed) {
        DynBits lhs_magnitude(magnitude_width);
        DynBits rhs_magnitude(magnitude_width);
        if (modulo) {
            detail::divide_modulo(
                quotient.mut(),
                remainder.mut(),
                a.cref(),
                b.cref(),
                lhs_magnitude.mut(),
                rhs_magnitude.mut(),
                scratch.view
            );
        } else {
            detail::divide_signed(
                quotient.mut(),
                remainder.mut(),
                a.cref(),
                b.cref(),
                lhs_magnitude.mut(),
                rhs_magnitude.mut(),
                scratch.view
            );
        }
    } else {
        detail::divide_unsigned(
            quotient.mut(), remainder.mut(), a.cref(), b.cref(), scratch.view
        );
    }
    return {std::move(quotient), std::move(remainder)};
}

inline std::pair<DynBits, DynBits> divrem_unsigned(DynBits const& a, DynBits const& b) {
    return divrem_impl(a, b, false, false);
}

inline std::pair<DynBits, DynBits> divrem_signed(DynBits const& a, DynBits const& b) {
    return divrem_impl(a, b, true, false);
}

inline std::pair<DynBits, DynBits> divmod_signed(DynBits const& a, DynBits const& b) {
    return divrem_impl(a, b, true, true);
}

inline DynBits div_unsigned(DynBits const& a, DynBits const& b) {
    return divrem_unsigned(a, b).first;
}

inline DynBits div_signed(DynBits const& a, DynBits const& b) {
    return divrem_signed(a, b).first;
}

inline DynBits rem_unsigned(DynBits const& a, DynBits const& b) {
    return divrem_unsigned(a, b).second;
}

// C-style remainder: the sign follows the dividend.
inline DynBits rem_signed(DynBits const& a, DynBits const& b) {
    return divrem_signed(a, b).second;
}

// VHDL/Python modulo: the sign follows the divisor.
inline DynBits mod_signed(DynBits const& a, DynBits const& b) {
    return divmod_signed(a, b).second;
}

inline DynBits negate_signed(DynBits const& a) {
    size_t wr = a.width() + 1;
    return DynBits::exact_sub(DynBits(wr), a.sign_extend(wr));
}

inline DynBits abs_signed(DynBits const& a) {
    size_t wr = a.width() + 1;
    DynBits ext = a.sign_extend(wr);
    if (a.width() > 0 && a.get_bit(a.width() - 1)) {
        return DynBits::exact_sub(DynBits(wr), ext);
    }
    return ext;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_INT_BASE_HPP

#ifndef COCONEXT_DYN_INT_BASE_HPP
#define COCONEXT_DYN_INT_BASE_HPP

#include <algorithm>
#include <climits>
#include <coconext/types/bigint.hpp>
#include <coconext/types/int_base.hpp>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace coconext::types::detail {

// Runtime-width owning integer representation. Storage, canonical extension,
// and all algorithms live together; only the storage-agnostic WordSpan kernels
// remain outside this class.
template <bool SignedRepresentation>
class DynInt {
    template <bool>
    friend class DynInt;

  public:
    static constexpr bool is_signed = SignedRepresentation;
    static constexpr size_t sbo_words = 2;
    static constexpr size_t sbo_bits = sbo_words * word_bits;

    static_assert(sbo_words >= 1, "the inline buffer needs at least one word");

    explicit DynInt(size_t width) : width_(width) { initialize_storage(); }

    template <NativeInteger IntT>
    DynInt(size_t width, IntT val) : DynInt(width) {
        if (width == 0) {
            throw std::invalid_argument("DynInt(0) has no integer representation");
        }
#if defined(__SIZEOF_INT128__)
        if constexpr (sizeof(IntT) > sizeof(Word)) {
            if constexpr (std::is_signed_v<IntT>) {
                load_int128(physical_mut(), static_cast<__int128_t>(val));
            } else {
                load_uint128(physical_mut(), static_cast<__uint128_t>(val));
            }
        } else
#endif
        {
            load_native(physical_mut(), val);
        }
        if (width_ < sizeof(IntT) * CHAR_BIT
            || (!SignedRepresentation && std::is_signed_v<IntT>))
        {
            canonicalize();
        }
    }

#if defined(__SIZEOF_INT128__)
    DynInt(size_t width, __int128_t val) : DynInt(width) {
        load_int128(physical_mut(), val);
        if (width_ < 128 || !SignedRepresentation) {
            canonicalize();
        }
    }
    DynInt(size_t width, __uint128_t val) : DynInt(width) {
        load_uint128(physical_mut(), val);
        if (width_ < 128) {
            canonicalize();
        }
    }
#endif

    DynInt(size_t width, std::string_view str) : DynInt(width) {
        parse_into(logical_mut(), str);
        if constexpr (SignedRepresentation) {
            canonicalize();
        }
    }

    template <size_t W, bool OtherSigned>
    explicit DynInt(Int<W, OtherSigned> const& src) : DynInt(W) {
        if constexpr (W > 0) {
            auto dst = physical_mut();
            if constexpr (Int<W, OtherSigned>::is_wide) {
                auto source = src.raw().data();
                for (size_t i = 0; i < dst.num_words(); ++i) {
                    dst.data()[i] = source[i];
                }
            } else {
#if defined(__SIZEOF_INT128__)
                if constexpr (sizeof(typename Int<W, OtherSigned>::IntType) > sizeof(Word))
                {
                    if constexpr (OtherSigned) {
                        using SourceSigned =
                            typename as_signed<typename Int<W, OtherSigned>::IntType>::type;
                        load_int128(
                            dst,
                            static_cast<__int128_t>(static_cast<SourceSigned>(src.raw()))
                        );
                    } else {
                        load_uint128(dst, static_cast<__uint128_t>(src.raw()));
                    }
                } else
#endif
                {
                    if constexpr (OtherSigned) {
                        using SourceSigned =
                            typename as_signed<typename Int<W, OtherSigned>::IntType>::type;
                        load_native(dst, static_cast<SourceSigned>(src.raw()));
                    } else {
                        load_native(dst, src.raw());
                    }
                }
            }
            if constexpr (SignedRepresentation != OtherSigned) {
                canonicalize();
            }
        }
    }

    template <bool OtherSigned>
    explicit DynInt(DynInt<OtherSigned> const& other) : DynInt(other.width(), other) {}

    template <bool OtherSigned>
    DynInt(size_t width, DynInt<OtherSigned> const& other) : DynInt(width) {
        auto dst = physical_mut();
        auto src = other.physical_cref();
        for (size_t i = 0; i < dst.num_words(); ++i) {
            dst.data()[i] = extended_word(src, i, OtherSigned);
        }
        if (width_ < other.width_ || SignedRepresentation != OtherSigned) {
            canonicalize();
        }
    }

    DynInt(DynInt const& other) : DynInt(other.width_) {
        auto dst = words();
        auto src = other.words();
        for (size_t i = 0; i < dst.size(); ++i) {
            dst[i] = src[i];
        }
    }

    DynInt(DynInt&& other) noexcept : width_(other.width_) {
        if (is_inline()) {
            for (size_t i = 0; i < sbo_words; ++i) {
                storage_.inline_[i] = other.storage_.inline_[i];
            }
        } else {
            std::construct_at(&storage_.heap_, std::move(other.storage_.heap_));
            std::destroy_at(&other.storage_.heap_);
            other.width_ = 0;
            for (size_t i = 0; i < sbo_words; ++i) {
                other.storage_.inline_[i] = 0;
            }
        }
    }

    DynInt& operator=(DynInt const& other) {
        if (this != &other) {
            DynInt tmp(other);
            *this = std::move(tmp);
        }
        return *this;
    }

    DynInt& operator=(DynInt&& other) noexcept {
        if (this != &other) {
            destroy_storage();
            width_ = other.width_;
            if (is_inline()) {
                for (size_t i = 0; i < sbo_words; ++i) {
                    storage_.inline_[i] = other.storage_.inline_[i];
                }
            } else {
                std::construct_at(&storage_.heap_, std::move(other.storage_.heap_));
                std::destroy_at(&other.storage_.heap_);
                other.width_ = 0;
                for (size_t i = 0; i < sbo_words; ++i) {
                    other.storage_.inline_[i] = 0;
                }
            }
        }
        return *this;
    }

    ~DynInt() { destroy_storage(); }

    size_t width() const { return width_; }
    size_t num_words() const { return (width_ + word_bits - 1) / word_bits; }
    size_t physical_width() const { return num_words() * word_bits; }

    bool get_bit(size_t index) const {
        if (index >= width_) {
            throw std::out_of_range("Bit index out of bounds");
        }
        return detail::get_bit(logical_cref(), index);
    }

    void set_bit(size_t index, bool val) {
        if (index >= width_) {
            throw std::out_of_range("Bit index out of bounds");
        }
        detail::set_bit(logical_mut(), index, val);
        if constexpr (SignedRepresentation) {
            if (index == width_ - 1) {
                canonicalize();
            }
        }
    }

    WordConstSpan raw() const {
        if (width_ == 0) {
            throw std::domain_error("raw() on a zero-width DynInt is undefined");
        }
        return physical_cref();
    }

    DynInt<false> logical_bits() const { return DynInt<false>(width_, *this); }

    size_t popcount() const {
        size_t count = 0;
        auto data = words();
        for (size_t i = 0; i < data.size(); ++i) {
            Word word = data[i];
            if (i + 1 == data.size() && width_ % word_bits != 0) {
                word &= (Word{1} << (width_ % word_bits)) - 1;
            }
            count += std::popcount(word);
        }
        return count;
    }

    size_t count_leading_zeros() const {
        auto data = words();
        for (size_t i = data.size(); i > 0; --i) {
            Word word = data[i - 1];
            if (i == data.size() && width_ % word_bits != 0) {
                word &= (Word{1} << (width_ % word_bits)) - 1;
            }
            if (word != 0) {
                size_t unused = data.size() * word_bits - width_;
                return ((data.size() - i) * word_bits) + std::countl_zero(word) - unused;
            }
        }
        return width_;
    }

    size_t count_trailing_zeros() const {
        return detail::count_trailing_zeros(logical_cref());
    }

    bool operator==(DynInt const& other) const {
        return width_ == other.width_ && words_equal(other);
    }

    std::strong_ordering operator<=>(DynInt const& other) const {
        check_same_width(other);
        if (less(other)) {
            return std::strong_ordering::less;
        }
        if (other.less(*this)) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }

    DynInt operator&(DynInt const& other) const {
        check_same_width(other);
        DynInt result(*this);
        and_assign(result.physical_mut(), other.physical_cref());
        return result;
    }
    DynInt operator|(DynInt const& other) const {
        check_same_width(other);
        DynInt result(*this);
        or_assign(result.physical_mut(), other.physical_cref());
        return result;
    }
    DynInt operator^(DynInt const& other) const {
        check_same_width(other);
        DynInt result(*this);
        xor_assign(result.physical_mut(), other.physical_cref());
        return result;
    }
    DynInt operator~() const {
        DynInt result(*this);
        bitnot(result.physical_mut());
        if constexpr (!SignedRepresentation) {
            result.canonicalize();
        }
        return result;
    }
    DynInt operator<<(size_t amount) const {
        if (amount >= width_) {
            return DynInt(width_);
        }
        DynInt result(*this);
        shift_left(result.physical_mut(), amount);
        result.canonicalize();
        return result;
    }
    DynInt operator>>(size_t amount) const {
        if (width_ == 0) {
            return DynInt(0);
        }
        if (amount >= width_) {
            if constexpr (SignedRepresentation) {
                return get_bit(width_ - 1) ? ~DynInt(width_) : DynInt(width_);
            } else {
                return DynInt(width_);
            }
        }
        DynInt result(*this);
        if constexpr (SignedRepresentation) {
            shift_right_arith(result.physical_mut(), amount);
        } else {
            shift_right_logical(result.physical_mut(), amount);
        }
        return result;
    }

    DynInt truncate(size_t target) const {
        if (target > width_) {
            throw std::invalid_argument("truncate cannot widen");
        }
        return DynInt(target, *this);
    }

    DynInt<false> saturate_unsigned(size_t target) const
        requires(!SignedRepresentation)
    {
        if (target >= width_) {
            return DynInt<false>(target, *this);
        }
        if (target == 0) {
            return DynInt<false>(0);
        }
        DynInt<false> maximum = ~DynInt<false>(target);
        return !maximum.less(*this) ? DynInt<false>(target, *this) : maximum;
    }

    DynInt<true> saturate_signed(size_t target) const
        requires SignedRepresentation
    {
        if (target >= width_) {
            return DynInt<true>(target, *this);
        }
        if (target == 0) {
            return DynInt<true>(0);
        }
        DynInt<true> minimum(target);
        minimum.set_bit(target - 1, true);
        DynInt<true> maximum = ~minimum;
        if (less(minimum)) {
            return minimum;
        }
        if (maximum.less(*this)) {
            return maximum;
        }
        return DynInt<true>(target, *this);
    }

    std::string to_binary_string() const {
        return format_power_of_two(logical_cref(), 1, width_, [](uint8_t d) {
            return static_cast<char>('0' + d);
        });
    }
    std::string to_decimal_string() const {
        return format_decimal(physical_cref(), SignedRepresentation);
    }
    std::string to_decimal_string(bool signed_value) const {
        return format_decimal(physical_cref(), signed_value);
    }
    std::string to_hexadecimal_string() const {
        char const digits[] = "0123456789abcdef";
        return format_power_of_two(logical_cref(), 4, (width_ + 3) / 4, [&](uint8_t d) {
            return digits[d];
        });
    }
    std::string to_octal_string() const {
        return format_power_of_two(logical_cref(), 3, (width_ + 2) / 3, [](uint8_t d) {
            return static_cast<char>('0' + d);
        });
    }

    static DynInt exact_add(DynInt const& a, DynInt const& b) {
        a.check_same_width(b);
        DynInt result(a);
        add_assign(result.physical_mut(), b.physical_cref());
        result.canonicalize();
        return result;
    }
    static DynInt exact_sub(DynInt const& a, DynInt const& b) {
        a.check_same_width(b);
        DynInt result(a);
        sub_assign(result.physical_mut(), b.physical_cref());
        result.canonicalize();
        return result;
    }
    static DynInt exact_mul(DynInt const& a, DynInt const& b) {
        a.check_same_width(b);
        DynInt result(a.width_);
        if constexpr (SignedRepresentation) {
            multiply_signed(result.physical_mut(), a.physical_cref(), b.physical_cref());
        } else {
            multiply_unsigned(result.physical_mut(), a.physical_cref(), b.physical_cref());
        }
        result.canonicalize();
        return result;
    }

    template <bool Sa, bool Sb>
    static DynInt arithmetic(
        DynInt<Sa> const& a, DynInt<Sb> const& b, size_t result_width, char operation
    ) {
        DynInt result(result_width);
        if (operation == '+') {
            add_extended(
                result.physical_mut(), a.physical_cref(), b.physical_cref(), Sa, Sb
            );
        } else if (operation == '-') {
            sub_extended(
                result.physical_mut(), a.physical_cref(), b.physical_cref(), Sa, Sb
            );
        } else if constexpr (Sa && Sb) {
            multiply_signed(result.physical_mut(), a.physical_cref(), b.physical_cref());
        } else {
            multiply_unsigned(result.physical_mut(), a.physical_cref(), b.physical_cref());
        }
        return result;
    }

    static std::pair<DynInt, DynInt> divide(DynInt const& a, DynInt const& b, bool modulo) {
        size_t common_width = std::max(a.width_, b.width_) + 1;
        size_t max_words = (common_width + word_bits - 1) / word_bits;
        DynInt quotient(a.width_ + 1);
        DynInt remainder(b.width_);
        OwnedDivScratch scratch = make_owned_div_scratch(max_words * limbs_per_word);
        if constexpr (SignedRepresentation) {
            DynInt<false> lhs_magnitude(a.width_);
            DynInt<false> rhs_magnitude(b.width_);
            if (modulo) {
                divide_modulo(
                    quotient.physical_mut(),
                    remainder.physical_mut(),
                    a.physical_cref(),
                    b.physical_cref(),
                    lhs_magnitude.physical_mut(),
                    rhs_magnitude.physical_mut(),
                    scratch.view
                );
            } else {
                divide_signed(
                    quotient.physical_mut(),
                    remainder.physical_mut(),
                    a.physical_cref(),
                    b.physical_cref(),
                    lhs_magnitude.physical_mut(),
                    rhs_magnitude.physical_mut(),
                    scratch.view
                );
            }
        } else {
            divide_unsigned(
                quotient.physical_mut(),
                remainder.physical_mut(),
                a.physical_cref(),
                b.physical_cref(),
                scratch.view
            );
        }
        return {std::move(quotient), std::move(remainder)};
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

    void initialize_storage() {
        if (is_inline()) {
            for (size_t i = 0; i < sbo_words; ++i) {
                storage_.inline_[i] = 0;
            }
        } else {
            std::construct_at(&storage_.heap_, new Word[num_words()]());
        }
    }

    void destroy_storage() {
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

    WordConstSpan logical_cref() const { return WordConstSpan{words(), width_}; }
    WordSpan logical_mut() { return WordSpan{words(), width_}; }
    WordConstSpan physical_cref() const { return WordConstSpan{words(), physical_width()}; }
    WordSpan physical_mut() { return WordSpan{words(), physical_width()}; }

    void canonicalize() {
        if (width_ == 0 || width_ == physical_width()) {
            return;
        }
        unsigned valid_bits = width_ % word_bits;
        Word mask = (Word{1} << valid_bits) - 1;
        Word& top = words().back();
        if constexpr (SignedRepresentation) {
            Word extension = Word{0} - ((top >> (valid_bits - 1)) & Word{1});
            top = (top & mask) | (extension & ~mask);
        } else {
            top &= mask;
        }
    }

    bool less(DynInt const& other) const {
        if constexpr (SignedRepresentation) {
            return scompare(physical_cref(), other.physical_cref()) < 0;
        } else {
            return ucompare(physical_cref(), other.physical_cref()) < 0;
        }
    }

    bool words_equal(DynInt const& other) const {
        auto a = words();
        auto b = other.words();
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }

    void check_same_width(DynInt const& other) const {
        if (width_ != other.width_) {
            throw std::invalid_argument("bit width mismatch");
        }
    }
};

using DynUInt = DynInt<false>;
using DynSInt = DynInt<true>;

inline DynUInt operator+(DynUInt const& a, DynUInt const& b) {
    return DynUInt::arithmetic(a, b, std::max(a.width(), b.width()) + 1, '+');
}
inline DynSInt operator+(DynSInt const& a, DynSInt const& b) {
    return DynSInt::arithmetic(a, b, std::max(a.width(), b.width()) + 1, '+');
}
inline DynSInt operator-(DynUInt const& a, DynUInt const& b) {
    return DynSInt::arithmetic(a, b, std::max(a.width(), b.width()) + 1, '-');
}
inline DynSInt operator-(DynSInt const& a, DynSInt const& b) {
    return DynSInt::arithmetic(a, b, std::max(a.width(), b.width()) + 1, '-');
}
inline DynUInt operator*(DynUInt const& a, DynUInt const& b) {
    return DynUInt::arithmetic(a, b, a.width() + b.width(), '*');
}
inline DynSInt operator*(DynSInt const& a, DynSInt const& b) {
    return DynSInt::arithmetic(a, b, a.width() + b.width(), '*');
}

inline std::pair<DynUInt, DynUInt> divrem(DynUInt const& a, DynUInt const& b) {
    return DynUInt::divide(a, b, false);
}
inline std::pair<DynSInt, DynSInt> divrem(DynSInt const& a, DynSInt const& b) {
    return DynSInt::divide(a, b, false);
}
inline std::pair<DynSInt, DynSInt> divmod(DynSInt const& a, DynSInt const& b) {
    return DynSInt::divide(a, b, true);
}
inline DynUInt operator/(DynUInt const& a, DynUInt const& b) { return divrem(a, b).first; }
inline DynSInt operator/(DynSInt const& a, DynSInt const& b) { return divrem(a, b).first; }
inline DynUInt operator%(DynUInt const& a, DynUInt const& b) { return divrem(a, b).second; }
inline DynSInt operator%(DynSInt const& a, DynSInt const& b) { return divrem(a, b).second; }
inline DynSInt mod(DynSInt const& a, DynSInt const& b) { return divmod(a, b).second; }

inline DynSInt operator-(DynSInt const& a) {
    size_t result_width = a.width() + 1;
    return DynSInt::arithmetic(
        DynSInt(result_width), DynSInt(result_width, a), result_width, '-'
    );
}
inline DynSInt operator-(DynUInt const& a) {
    size_t result_width = a.width() + 1;
    return DynSInt::arithmetic(DynUInt(a.width()), a, result_width, '-');
}
inline DynSInt abs(DynSInt const& a) {
    DynSInt extended(a.width() + 1, a);
    return a.width() != 0 && a.get_bit(a.width() - 1) ? -a : extended;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_INT_BASE_HPP

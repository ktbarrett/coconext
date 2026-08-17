#ifndef COCONEXT_DYN_INT_BASE_HPP
#define COCONEXT_DYN_INT_BASE_HPP

#include <algorithm>
#include <climits>
#include <coconext/types/bigint.hpp>
#include <coconext/types/int_base.hpp>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
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
    static constexpr size_t sbo_bits = word_bits;

    explicit DynInt(size_t width) : width_(width) { initialize_storage(); }

    template <NativeInteger IntT>
    DynInt(size_t width, IntT val) : DynInt(width) {
        if (width == 0) {
            throw std::invalid_argument("DynInt(0) has no integer representation");
        }
        assert((native_value_fits<SignedRepresentation>(width, val)));
        if (is_native()) {
            storage_.native_ = static_cast<Word>(val);
            canonicalize();
            return;
        }
#if defined(__SIZEOF_INT128__)
        if constexpr (sizeof(IntT) > sizeof(Word)) {
            if constexpr (std::is_signed_v<IntT>) {
                assign_int128(static_cast<__int128_t>(val));
            } else {
                assign_uint128(static_cast<__uint128_t>(val));
            }
        } else
#endif
        {
            assign_native(val);
        }
        canonicalize();
    }

#if defined(__SIZEOF_INT128__)
    DynInt(size_t width, __int128_t val) : DynInt(width) {
        assert((native_value_fits<SignedRepresentation>(width, val)));
        assign_int128(val);
        canonicalize();
    }
    DynInt(size_t width, __uint128_t val) : DynInt(width) {
        assert((native_value_fits<SignedRepresentation>(width, val)));
        assign_uint128(val);
        canonicalize();
    }
#endif

    DynInt(size_t width, std::string_view str) : DynInt(width) {
        if (is_native()) {
            storage_.native_ = parse_native(str);
            canonicalize();
        } else {
            parse_into(logical_mut(), str);
            if constexpr (SignedRepresentation) {
                canonicalize();
            }
        }
    }

    template <size_t W, bool OtherSigned>
    explicit DynInt(Int<W, OtherSigned> const& src) : DynInt(W) {
        if constexpr (W > 0) {
            if constexpr (W <= word_bits) {
                storage_.native_ = static_cast<Word>(src.raw());
            } else if constexpr (Int<W, OtherSigned>::is_wide) {
                auto source = src.raw().data();
                auto destination = words();
                for (size_t i = 0; i < destination.size(); ++i) {
                    destination[i] = source[i];
                }
            } else {
#if defined(__SIZEOF_INT128__)
                if constexpr (sizeof(typename Int<W, OtherSigned>::IntType) > sizeof(Word))
                {
                    if constexpr (OtherSigned) {
                        using SourceSigned =
                            typename as_signed<typename Int<W, OtherSigned>::IntType>::type;
                        assign_int128(
                            static_cast<__int128_t>(static_cast<SourceSigned>(src.raw()))
                        );
                    } else {
                        assign_uint128(static_cast<__uint128_t>(src.raw()));
                    }
                } else
#endif
                {
                    if constexpr (OtherSigned) {
                        using SourceSigned =
                            typename as_signed<typename Int<W, OtherSigned>::IntType>::type;
                        assign_native(static_cast<SourceSigned>(src.raw()));
                    } else {
                        assign_native(src.raw());
                    }
                }
            }
            canonicalize();
        }
    }

    template <bool OtherSigned>
    explicit DynInt(DynInt<OtherSigned> const& other) : DynInt(other.width(), other) {}

    template <bool OtherSigned>
    DynInt(size_t width, DynInt<OtherSigned> const& other) : DynInt(width) {
        if (is_native()) {
            storage_.native_ = other.low_word();
        } else {
            auto destination = words();
            Word extension = OtherSigned && other.is_negative() ? ~Word{0} : Word{0};
            for (size_t i = 0; i < destination.size(); ++i) {
                destination[i] = i < other.num_words() ? other.word(i) : extension;
            }
        }
        canonicalize();
    }

    DynInt(DynInt const& other) : DynInt(other.width_) {
        if (is_native()) {
            storage_.native_ = other.storage_.native_;
        } else {
            auto destination = words();
            auto source = other.words();
            for (size_t i = 0; i < destination.size(); ++i) {
                destination[i] = source[i];
            }
        }
    }

    DynInt(DynInt&& other) noexcept : width_(other.width_) {
        if (is_native()) {
            storage_.native_ = other.storage_.native_;
        } else {
            storage_.heap_ = other.storage_.heap_;
            other.width_ = 0;
            other.storage_.native_ = 0;
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
            if (is_native()) {
                storage_.native_ = other.storage_.native_;
            } else {
                storage_.heap_ = other.storage_.heap_;
                other.width_ = 0;
                other.storage_.native_ = 0;
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
        if (is_native()) {
            return (storage_.native_ >> index) & Word{1};
        }
        return (storage_.heap_[index / word_bits] >> (index % word_bits)) & Word{1};
    }

    void set_bit(size_t index, bool val) {
        if (index >= width_) {
            throw std::out_of_range("Bit index out of bounds");
        }
        Word mask = Word{1} << (index % word_bits);
        Word& target = is_native() ? storage_.native_ : storage_.heap_[index / word_bits];
        target = val ? target | mask : target & ~mask;
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
        if (is_native()) {
            return std::popcount(logical_native_value());
        }
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
        if (is_native()) {
            Word value = logical_native_value();
            return value == 0 ? width_ : std::countl_zero(value) - (word_bits - width_);
        }
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
        if (is_native()) {
            Word value = logical_native_value();
            return value == 0 ? width_ : std::countr_zero(value);
        }
        return detail::count_trailing_zeros(logical_cref());
    }

    bool operator==(DynInt const& other) const {
        if (width_ != other.width_) {
            return false;
        }
        if (is_native()) {
            return storage_.native_ == other.storage_.native_;
        }
        return words_equal(other);
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
        if (is_native()) {
            result.storage_.native_ &= other.storage_.native_;
        } else {
            and_assign(result.physical_mut(), other.physical_cref());
        }
        return result;
    }
    DynInt operator|(DynInt const& other) const {
        check_same_width(other);
        DynInt result(*this);
        if (is_native()) {
            result.storage_.native_ |= other.storage_.native_;
        } else {
            or_assign(result.physical_mut(), other.physical_cref());
        }
        return result;
    }
    DynInt operator^(DynInt const& other) const {
        check_same_width(other);
        DynInt result(*this);
        if (is_native()) {
            result.storage_.native_ ^= other.storage_.native_;
        } else {
            xor_assign(result.physical_mut(), other.physical_cref());
        }
        return result;
    }
    DynInt operator~() const {
        DynInt result(*this);
        if (is_native()) {
            result.storage_.native_ = ~result.storage_.native_;
        } else {
            bitnot(result.physical_mut());
        }
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
        if (is_native()) {
            result.storage_.native_ <<= amount;
        } else {
            shift_left(result.physical_mut(), amount);
        }
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
        if (is_native()) {
            if constexpr (SignedRepresentation) {
                result.storage_.native_ =
                    static_cast<Word>(static_cast<int64_t>(storage_.native_) >> amount);
            } else {
                result.storage_.native_ >>= amount;
            }
        } else if constexpr (SignedRepresentation) {
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
        if (is_native()) {
            return width_ == 0 ? ""
                               : std::format("{:0{}b}", logical_native_value(), width_);
        }
        return format_power_of_two(logical_cref(), 1, width_, [](uint8_t d) {
            return static_cast<char>('0' + d);
        });
    }
    std::string to_decimal_string() const {
        return to_decimal_string(SignedRepresentation);
    }
    std::string to_decimal_string(bool signed_value) const {
        if (is_native()) {
            if (width_ == 0) {
                return "";
            }
            return signed_value ? std::format("{}", static_cast<int64_t>(storage_.native_))
                                : std::format("{}", storage_.native_);
        }
        return format_decimal(physical_cref(), signed_value);
    }
    std::string to_hexadecimal_string() const {
        if (is_native()) {
            return width_ == 0
                     ? ""
                     : std::format("{:0{}x}", logical_native_value(), (width_ + 3) / 4);
        }
        char const digits[] = "0123456789abcdef";
        return format_power_of_two(logical_cref(), 4, (width_ + 3) / 4, [&](uint8_t d) {
            return digits[d];
        });
    }
    std::string to_octal_string() const {
        if (is_native()) {
            return width_ == 0
                     ? ""
                     : std::format("{:0{}o}", logical_native_value(), (width_ + 2) / 3);
        }
        return format_power_of_two(logical_cref(), 3, (width_ + 2) / 3, [](uint8_t d) {
            return static_cast<char>('0' + d);
        });
    }

    static DynInt exact_add(DynInt const& a, DynInt const& b) {
        a.check_same_width(b);
        DynInt result(a);
        if (a.is_native()) {
            result.storage_.native_ += b.storage_.native_;
        } else {
            add_assign(result.physical_mut(), b.physical_cref());
        }
        result.canonicalize();
        return result;
    }
    static DynInt exact_sub(DynInt const& a, DynInt const& b) {
        a.check_same_width(b);
        DynInt result(a);
        if (a.is_native()) {
            result.storage_.native_ -= b.storage_.native_;
        } else {
            sub_assign(result.physical_mut(), b.physical_cref());
        }
        result.canonicalize();
        return result;
    }
    static DynInt exact_mul(DynInt const& a, DynInt const& b) {
        a.check_same_width(b);
        DynInt result(a.width_);
        if (a.is_native()) {
            result.storage_.native_ = a.storage_.native_ * b.storage_.native_;
        } else if constexpr (SignedRepresentation) {
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
        static_assert(Sa == Sb);
        DynInt result(result_width);
        if (a.is_native() && b.is_native()) {
            if (result.is_native()) {
                if constexpr (SignedRepresentation) {
                    int64_t lhs = Sa ? static_cast<int64_t>(a.storage_.native_)
                                     : static_cast<int64_t>(a.logical_native_value());
                    int64_t rhs = Sb ? static_cast<int64_t>(b.storage_.native_)
                                     : static_cast<int64_t>(b.logical_native_value());
                    if (operation == '+') {
                        result.storage_.native_ = static_cast<Word>(lhs + rhs);
                    } else if (operation == '-') {
                        result.storage_.native_ = static_cast<Word>(lhs - rhs);
                    } else {
                        result.storage_.native_ = static_cast<Word>(lhs * rhs);
                    }
                } else {
                    Word lhs = a.logical_native_value();
                    Word rhs = b.logical_native_value();
                    if (operation == '+') {
                        result.storage_.native_ = lhs + rhs;
                    } else if (operation == '-') {
                        result.storage_.native_ = lhs - rhs;
                    } else {
                        result.storage_.native_ = lhs * rhs;
                    }
                }
                result.canonicalize();
                return result;
            }
#if defined(__SIZEOF_INT128__)
            if constexpr (SignedRepresentation) {
                __int128_t lhs = Sa ? static_cast<int64_t>(a.storage_.native_)
                                    : static_cast<__int128_t>(a.logical_native_value());
                __int128_t rhs = Sb ? static_cast<int64_t>(b.storage_.native_)
                                    : static_cast<__int128_t>(b.logical_native_value());
                if (operation == '+') {
                    result.assign_int128(lhs + rhs);
                } else if (operation == '-') {
                    result.assign_int128(lhs - rhs);
                } else {
                    result.assign_int128(lhs * rhs);
                }
            } else {
                __uint128_t lhs = a.logical_native_value();
                __uint128_t rhs = b.logical_native_value();
                if (operation == '+') {
                    result.assign_uint128(lhs + rhs);
                } else if (operation == '-') {
                    result.assign_uint128(lhs - rhs);
                } else {
                    result.assign_uint128(lhs * rhs);
                }
            }
            result.canonicalize();
            return result;
#endif
        }
        if (operation == '+') {
            add_extended<Sa>(result.physical_mut(), a.physical_cref(), b.physical_cref());
        } else if (operation == '-') {
            sub_extended<Sa>(result.physical_mut(), a.physical_cref(), b.physical_cref());
        } else if constexpr (Sa) {
            multiply_signed(result.physical_mut(), a.physical_cref(), b.physical_cref());
        } else {
            multiply_unsigned(result.physical_mut(), a.physical_cref(), b.physical_cref());
        }
        return result;
    }

    static std::pair<DynInt, DynInt> divide(DynInt const& a, DynInt const& b, bool modulo) {
        if (a.is_native() && b.is_native()) {
            if constexpr (SignedRepresentation) {
                int64_t lhs = static_cast<int64_t>(a.storage_.native_);
                int64_t rhs = static_cast<int64_t>(b.storage_.native_);
                if (rhs == 0) {
                    throw std::domain_error("Division by zero");
                }
                if (lhs == std::numeric_limits<int64_t>::min() && rhs == -1) {
                    return {
                        DynInt(a.width_ + 1, Word{1} << (word_bits - 1)),
                        DynInt(b.width_, int64_t{0})
                    };
                }
                int64_t quotient = lhs / rhs;
                int64_t remainder = lhs % rhs;
                if (modulo && remainder != 0 && (lhs < 0) != (rhs < 0)) {
                    --quotient;
                    remainder += rhs;
                }
                return {DynInt(a.width_ + 1, quotient), DynInt(b.width_, remainder)};
            } else {
                Word lhs = a.logical_native_value();
                Word rhs = b.logical_native_value();
                if (rhs == 0) {
                    throw std::domain_error("Division by zero");
                }
                return {DynInt(a.width_ + 1, lhs / rhs), DynInt(b.width_, lhs % rhs)};
            }
        }
        size_t lhs_limbs = a.num_words() * limbs_per_word;
        size_t rhs_limbs = b.num_words() * limbs_per_word;
        DynInt quotient(a.width_ + 1);
        DynInt remainder(b.width_);
        OwnedDivScratch scratch = make_owned_div_scratch(lhs_limbs, rhs_limbs);
        if constexpr (SignedRepresentation) {
            if (modulo) {
                divide_modulo(
                    quotient.physical_mut(),
                    remainder.physical_mut(),
                    a.physical_cref(),
                    b.physical_cref(),
                    scratch.view
                );
            } else {
                divide_signed(
                    quotient.physical_mut(),
                    remainder.physical_mut(),
                    a.physical_cref(),
                    b.physical_cref(),
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

    template <bool SourceSigned>
    static DynInt growing_negate(DynInt<SourceSigned> const& value)
        requires SignedRepresentation
    {
        size_t result_width = value.width_ + 1;
        if (value.is_native()) {
            DynInt result(result_width);
            if (result.is_native()) {
                int64_t operand = SourceSigned
                                    ? static_cast<int64_t>(value.storage_.native_)
                                    : static_cast<int64_t>(value.logical_native_value());
                result.storage_.native_ = static_cast<Word>(-operand);
                result.canonicalize();
                return result;
            }
#if defined(__SIZEOF_INT128__)
            __int128_t operand = SourceSigned
                                   ? static_cast<int64_t>(value.storage_.native_)
                                   : static_cast<__int128_t>(value.logical_native_value());
            result.assign_int128(-operand);
            result.canonicalize();
            return result;
#endif
        }
        return arithmetic(
            DynInt(result_width), DynInt(result_width, value), result_width, '-'
        );
    }

  private:
    size_t width_;
    union Storage {
        Word native_;
        Word* heap_;
    } storage_;

    bool is_native() const { return width_ <= sbo_bits; }

    void initialize_storage() {
        if (is_native()) {
            storage_.native_ = 0;
        } else {
            storage_.heap_ = new Word[num_words()]();
        }
    }

    void destroy_storage() {
        if (!is_native()) {
            delete[] storage_.heap_;
        }
    }

    std::span<Word> words() {
        return is_native() ? std::span<Word>{&storage_.native_, num_words()}
                           : std::span<Word>{storage_.heap_, num_words()};
    }
    std::span<Word const> words() const {
        return is_native() ? std::span<Word const>{&storage_.native_, num_words()}
                           : std::span<Word const>{storage_.heap_, num_words()};
    }

    Word low_word() const { return width_ == 0 ? Word{0} : word(0); }
    Word word(size_t index) const {
        return is_native() ? storage_.native_ : storage_.heap_[index];
    }

    Word native_mask() const {
        return width_ == 0 ? Word{0} : width_ == 64 ? ~Word{0} : (Word{1} << width_) - 1;
    }

    Word logical_native_value() const { return storage_.native_ & native_mask(); }

    bool is_negative() const {
        return SignedRepresentation && width_ != 0
            && ((word((width_ - 1) / word_bits) >> ((width_ - 1) % word_bits)) & 1);
    }

    template <typename IntT>
        requires(std::is_integral_v<IntT> && sizeof(IntT) <= sizeof(Word))
    void assign_native(IntT value) {
        if (is_native()) {
            storage_.native_ = static_cast<Word>(value);
            return;
        }
        storage_.heap_[0] = static_cast<Word>(value);
        Word extension = 0;
        if constexpr (std::is_signed_v<IntT>) {
            extension = value < 0 ? ~Word{0} : Word{0};
        }
        for (size_t i = 1; i < num_words(); ++i) {
            storage_.heap_[i] = extension;
        }
    }

#if defined(__SIZEOF_INT128__)
    void assign_int128(__int128_t value) {
        if (is_native()) {
            storage_.native_ = static_cast<Word>(value);
            return;
        }
        storage_.heap_[0] = static_cast<Word>(value);
        storage_.heap_[1] = static_cast<Word>(static_cast<__uint128_t>(value) >> word_bits);
        Word extension = value < 0 ? ~Word{0} : Word{0};
        for (size_t i = 2; i < num_words(); ++i) {
            storage_.heap_[i] = extension;
        }
    }

    void assign_uint128(__uint128_t value) {
        if (is_native()) {
            storage_.native_ = static_cast<Word>(value);
            return;
        }
        storage_.heap_[0] = static_cast<Word>(value);
        storage_.heap_[1] = static_cast<Word>(value >> word_bits);
        for (size_t i = 2; i < num_words(); ++i) {
            storage_.heap_[i] = 0;
        }
    }
#endif

    Word parse_native(std::string_view str) const {
        if (str.empty()) {
            return 0;
        }
        bool negative = false;
        size_t pos = 0;
        if (str[pos] == '-') {
            negative = true;
            ++pos;
        } else if (str[pos] == '+') {
            ++pos;
        }
        bool hexadecimal = pos + 1 < str.size() && str[pos] == '0'
                        && (str[pos + 1] == 'x' || str[pos + 1] == 'X');
        if (hexadecimal) {
            if (negative) {
                throw std::invalid_argument("Hexadecimal value cannot be negative");
            }
            pos += 2;
        }

        Word value = 0;
        Word maximum = native_mask();
        Word base = hexadecimal ? 16 : 10;
        for (; pos < str.size(); ++pos) {
            char c = str[pos];
            if (c == '\'' || c == '_') {
                continue;
            }
            unsigned digit;
            if (c >= '0' && c <= '9') {
                digit = static_cast<unsigned>(c - '0');
            } else if (hexadecimal && c >= 'a' && c <= 'f') {
                digit = static_cast<unsigned>(c - 'a' + 10);
            } else if (hexadecimal && c >= 'A' && c <= 'F') {
                digit = static_cast<unsigned>(c - 'A' + 10);
            } else {
                throw std::invalid_argument(
                    hexadecimal ? "Invalid hexadecimal character"
                                : "Invalid base-10 character"
                );
            }
            if (static_cast<Word>(digit) > maximum || value > (maximum - digit) / base) {
                throw std::out_of_range(
                    hexadecimal ? "Hexadecimal literal exceeds bit width"
                                : "Decimal literal exceeds bit width"
                );
            }
            value = value * base + digit;
        }
        return negative ? (Word{0} - value) & maximum : value;
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
        Word& top = is_native() ? storage_.native_ : storage_.heap_[num_words() - 1];
        if constexpr (SignedRepresentation) {
            Word extension = Word{0} - ((top >> (valid_bits - 1)) & Word{1});
            top = (top & mask) | (extension & ~mask);
        } else {
            top &= mask;
        }
    }

    bool less(DynInt const& other) const {
        if (is_native() && other.is_native()) {
            if constexpr (SignedRepresentation) {
                return static_cast<int64_t>(storage_.native_)
                     < static_cast<int64_t>(other.storage_.native_);
            } else {
                return storage_.native_ < other.storage_.native_;
            }
        }
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

inline DynSInt operator-(DynSInt const& a) { return DynSInt::growing_negate(a); }
inline DynSInt operator-(DynUInt const& a) { return DynSInt::growing_negate(a); }
inline DynSInt abs(DynSInt const& a) {
    DynSInt extended(a.width() + 1, a);
    return a.width() != 0 && a.get_bit(a.width() - 1) ? -a : extended;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_INT_BASE_HPP

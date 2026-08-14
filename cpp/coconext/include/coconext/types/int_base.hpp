#ifndef COCONEXT_INT_BASE_HPP
#define COCONEXT_INT_BASE_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <coconext/types/bigint.hpp>
#include <coconext/types/direction.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/resize_mode.hpp>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace coconext::types {

namespace detail {

struct EmptyStorage {};

template <size_t W>
using WideWords = std::array<Word, (W + word_bits - 1) / word_bits>;

template <size_t BW>
struct IntTypePicker {
    using type = std::conditional_t<
        BW == 0,
        EmptyStorage,
        std::conditional_t<
            (BW <= 8),
            uint8_t,
            std::conditional_t<
                (BW <= 16),
                uint16_t,
                std::conditional_t<
                    (BW <= 32),
                    uint32_t,
                    std::conditional_t<
                        (BW <= 64),
                        uint64_t,
#if defined(__SIZEOF_INT128__)
                        std::conditional_t<(BW <= 128), __uint128_t, WideWords<BW>>
#else
                        WideWords<BW>
#endif
                        >>>>>;
};

#if defined(__SIZEOF_INT128__)
static constexpr bool supports_128B = true;
using wide_uint = __uint128_t;
using wide_int = __int128_t;
#else
static constexpr bool supports_128B = false;
using wide_uint = uint64_t;
using wide_int = int64_t;
#endif

constexpr size_t integer_storage_width(size_t width) {
    if (width == 0) {
        return 0;
    }
    if (width <= 8) {
        return 8;
    }
    if (width <= 16) {
        return 16;
    }
    if (width <= 32) {
        return 32;
    }
    if (width <= 64) {
        return 64;
    }
    if constexpr (supports_128B) {
        if (width <= 128) {
            return 128;
        }
    }
    return ((width + word_bits - 1) / word_bits) * word_bits;
}

template <size_t W, bool SignedRepresentation>
class Int {
    template <size_t, bool>
    friend class Int;

  public:
    static constexpr size_t width = W;
    static constexpr size_t physical_width = integer_storage_width(W);
    static constexpr bool is_signed = SignedRepresentation;
    static constexpr bool is_wide = physical_width > (supports_128B ? 128 : 64);
    using IntType = IntTypePicker<physical_width>::type;
    using RawType = std::conditional_t<is_wide, WordConstSpan, IntType>;

  private:
    // These buffers are used only to bridge a native scalar into an operation
    // that also has wide storage. Native-only operations never form word spans.
    using NativeBuffer =
        std::array<Word, is_wide ? 0 : (physical_width + word_bits - 1) / word_bits>;
    using NativeArithmetic =
        std::conditional_t<(sizeof(IntType) < sizeof(unsigned)), unsigned, IntType>;

    constexpr WideWords<physical_width> native_word_array() const
        requires(!is_wide && physical_width > 0)
    {
        WideWords<physical_width> result{};
        result[0] = static_cast<Word>(storage_);
#if defined(__SIZEOF_INT128__)
        if constexpr (sizeof(IntType) > sizeof(Word)) {
            result[1] = static_cast<Word>(storage_ >> word_bits);
        }
#endif
        return result;
    }

    constexpr WordConstSpan physical_wide_cref(NativeBuffer& buffer) const {
        if constexpr (physical_width == 0) {
            return WordConstSpan{buffer, 0};
        } else if constexpr (is_wide) {
            return WordConstSpan{std::span<Word const>{storage_}, physical_width};
        } else {
            buffer = native_word_array();
            return WordConstSpan{buffer, physical_width};
        }
    }

    constexpr WordSpan physical_mut(NativeBuffer& buffer) {
        if constexpr (is_wide) {
            return WordSpan{std::span<Word>{storage_}, physical_width};
        } else {
            return WordSpan{buffer, physical_width};
        }
    }

    constexpr void finish_output(NativeBuffer const& buffer) {
        if constexpr (!is_wide && physical_width > 0) {
            storage_ = static_cast<IntType>(buffer[0]);
#if defined(__SIZEOF_INT128__)
            if constexpr (sizeof(IntType) > sizeof(Word)) {
                storage_ |= static_cast<IntType>(buffer[1]) << word_bits;
            }
#endif
        }
    }

    static constexpr IntType compute_logical_mask() {
        if constexpr (W == 0 || is_wide) {
            return IntType{};
        } else if constexpr (W == physical_width) {
            return ~IntType{0};
        } else {
            return (IntType{1} << W) - 1;
        }
    }

    static constexpr IntType logical_mask = compute_logical_mask();

    constexpr IntType logical_native_value() const
        requires(!is_wide && W > 0)
    {
        return storage_ & logical_mask;
    }

    constexpr WordConstSpan logical_wide_cref() const
        requires is_wide
    {
        return WordConstSpan{std::span<Word const>{storage_}, W};
    }

    static constexpr IntType parse_native(std::string_view str)
        requires(!is_wide && W > 0)
    {
        if (str.empty()) {
            return IntType{};
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

        IntType value = 0;
        IntType maximum = logical_mask;
        IntType base = hexadecimal ? 16 : 10;
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
            if (static_cast<IntType>(digit) > maximum || value > (maximum - digit) / base) {
                throw std::out_of_range(
                    hexadecimal ? "Hexadecimal literal exceeds bit width"
                                : "Decimal literal exceeds bit width"
                );
            }
            value = static_cast<IntType>(value * base + digit);
        }
        if (negative) {
            value = static_cast<IntType>(IntType{0} - value) & maximum;
        }
        return value;
    }

    // Storage is zero- or sign-extended from W through physical_width. Only
    // operations that can disturb those extension bits call canonicalize().
    constexpr void canonicalize() {
        if constexpr (W == 0 || W == physical_width) {
            return;
        } else if constexpr (!is_wide) {
            if constexpr (SignedRepresentation) {
                IntType extension =
                    static_cast<IntType>(IntType{0} - ((storage_ >> (W - 1)) & IntType{1}));
                storage_ = static_cast<IntType>(
                    (storage_ & logical_mask) | (extension & ~logical_mask)
                );
            } else {
                storage_ = static_cast<IntType>(storage_ & logical_mask);
            }
        } else {
            constexpr unsigned valid_bits = W % word_bits;
            constexpr Word logical_mask = (Word{1} << valid_bits) - 1;
            Word& top = storage_.back();
            if constexpr (SignedRepresentation) {
                Word extension = Word{0} - ((top >> (valid_bits - 1)) & Word{1});
                top = (top & logical_mask) | (extension & ~logical_mask);
            } else {
                top &= logical_mask;
            }
        }
    }

    template <size_t OtherW, bool OtherSigned>
    constexpr void copy_from(Int<OtherW, OtherSigned> const& other) {
        if constexpr (W == 0) {
            return;
        } else if constexpr (OtherW == 0) {
            storage_ = IntType{};
        } else if constexpr (!is_wide && !Int<OtherW, OtherSigned>::is_wide) {
            if constexpr (OtherSigned) {
                using OtherSignedType =
                    typename as_signed<typename Int<OtherW, OtherSigned>::IntType>::type;
                storage_ =
                    static_cast<IntType>(static_cast<OtherSignedType>(other.storage_));
            } else {
                storage_ = static_cast<IntType>(other.storage_);
            }
        } else {
            NativeBuffer dst_buffer{};
            typename Int<OtherW, OtherSigned>::NativeBuffer src_buffer{};
            auto dst = physical_mut(dst_buffer);
            auto src = other.physical_wide_cref(src_buffer);
            for (size_t i = 0; i < dst.num_words(); ++i) {
                dst.data()[i] = extended_word(src, i, OtherSigned);
            }
            finish_output(dst_buffer);
        }
        if constexpr (W < OtherW || SignedRepresentation != OtherSigned) {
            canonicalize();
        }
    }

    template <size_t OtherW, bool OtherSigned>
    constexpr bool less(Int<OtherW, OtherSigned> const& other) const {
        if constexpr (W > 0 && OtherW > 0 && !is_wide && !Int<OtherW, OtherSigned>::is_wide)
        {
            if constexpr (SignedRepresentation) {
                constexpr size_t common_width =
                    std::max(physical_width, Int<OtherW, OtherSigned>::physical_width);
                using CommonSigned =
                    typename as_signed<typename IntTypePicker<common_width>::type>::type;
                using ThisSigned = typename as_signed<IntType>::type;
                using OtherSignedType =
                    typename as_signed<typename Int<OtherW, OtherSigned>::IntType>::type;
                return static_cast<CommonSigned>(static_cast<ThisSigned>(storage_))
                     < static_cast<CommonSigned>(
                           static_cast<OtherSignedType>(other.storage_)
                     );
            } else {
                constexpr size_t common_width =
                    std::max(physical_width, Int<OtherW, OtherSigned>::physical_width);
                using CommonUnsigned = typename IntTypePicker<common_width>::type;
                return static_cast<CommonUnsigned>(storage_)
                     < static_cast<CommonUnsigned>(other.storage_);
            }
        } else {
            NativeBuffer a_buffer{};
            typename Int<OtherW, OtherSigned>::NativeBuffer b_buffer{};
            auto a = physical_wide_cref(a_buffer);
            auto b = other.physical_wide_cref(b_buffer);
            if constexpr (SignedRepresentation) {
                return scompare(a, b) < 0;
            } else {
                return ucompare(a, b) < 0;
            }
        }
    }

  public:  // constructors and conversion
    constexpr Int() = default;

    template <NativeInteger IntT>
    constexpr Int(IntT val) {
        static_assert(W > 0, "zero-width Int has no integer representation");
        if constexpr (!is_wide && W > 0) {
            storage_ = static_cast<IntType>(val);
        } else {
            auto dst = WordSpan{std::span<Word>{storage_}, physical_width};
#if defined(__SIZEOF_INT128__)
            if constexpr (sizeof(IntT) > sizeof(Word)) {
                if constexpr (std::is_signed_v<IntT>) {
                    load_int128(dst, static_cast<__int128_t>(val));
                } else {
                    load_uint128(dst, static_cast<__uint128_t>(val));
                }
            } else
#endif
            {
                load_native(dst, val);
            }
        }
        if constexpr (
            W < sizeof(IntT) * CHAR_BIT || (!SignedRepresentation && std::is_signed_v<IntT>)
        )
        {
            canonicalize();
        }
    }

    constexpr Int(std::string_view val) {
        static_assert(W > 0, "zero-width Int has no integer representation");
        if constexpr (is_wide) {
            parse_into(WordSpan{std::span<Word>{storage_}, W}, val);
        } else {
            storage_ = parse_native(val);
        }
        if constexpr (SignedRepresentation) {
            canonicalize();
        }
    }

    template <typename U>
        requires std::convertible_to<U, Bit>
    constexpr Int(std::initializer_list<U> init) {
        if (init.size() != W) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " does not match Int width " + std::to_string(W)
            );
        }
        if constexpr (W > 0) {
            size_t bit_pos = W - 1;
            for (auto const& val : init) {
                if (static_cast<bool>(Bit(val))) {
                    set_bit(bit_pos, true);
                }
                if (bit_pos > 0) {
                    --bit_pos;
                }
            }
        }
    }

    template <size_t OtherW, bool OtherSigned>
    constexpr explicit Int(Int<OtherW, OtherSigned> const& other) {
        copy_from(other);
    }

    constexpr Int<W, false> logical_bits() const { return Int<W, false>(*this); }

    constexpr RawType raw() const {
        static_assert(W > 0, "raw() on a zero-width Int is undefined");
        if constexpr (is_wide) {
            return WordConstSpan{std::span<Word const>{storage_}, physical_width};
        } else {
            return storage_;
        }
    }

    constexpr bool get_bit(size_t index) const {
        if (index >= W) {
            throw std::out_of_range("Bit index out of bounds");
        }
        if constexpr (is_wide) {
            return detail::get_bit(
                WordConstSpan{std::span<Word const>{storage_}, physical_width}, index
            );
        } else {
            return (storage_ >> index) & IntType{1};
        }
    }

    constexpr void set_bit(size_t index, bool val) {
        if (index >= W) {
            throw std::out_of_range("Bit index out of bounds");
        }
        if constexpr (is_wide) {
            detail::set_bit(
                WordSpan{std::span<Word>{storage_}, physical_width}, index, val
            );
        } else {
            IntType mask = IntType{1} << index;
            storage_ = val ? static_cast<IntType>(storage_ | mask)
                           : static_cast<IntType>(storage_ & ~mask);
        }
        if constexpr (SignedRepresentation && W < physical_width) {
            if (index == W - 1) {
                canonicalize();
            }
        }
    }

    class BitReference {
        Int& parent_;
        size_t index_;

      public:
        constexpr BitReference(Int& parent, size_t index)
            : parent_(parent), index_(index) {}
        constexpr operator Bit() const {
            return parent_.get_bit(index_) ? Bit::_1 : Bit::_0;
        }
        constexpr explicit operator char() const {
            return parent_.get_bit(index_) ? '1' : '0';
        }
        constexpr explicit operator bool() const { return parent_.get_bit(index_); }
        constexpr BitReference const& operator=(Bit val) const {
            parent_.set_bit(index_, static_cast<bool>(val));
            return *this;
        }
        constexpr BitReference const& operator=(BitReference const& other) const {
            parent_.set_bit(index_, static_cast<bool>(static_cast<Bit>(other)));
            return *this;
        }
    };

    template <bool IsConst>
    class IteratorImpl {
        using Parent = std::conditional_t<IsConst, Int const, Int>;
        Parent* parent_ = nullptr;
        size_t index_ = 0;

      public:
        using iterator_concept = std::random_access_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = Bit;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = std::conditional_t<IsConst, Bit, BitReference>;

        constexpr IteratorImpl() = default;
        constexpr IteratorImpl(Parent* parent, size_t index)
            : parent_(parent), index_(index) {}
        constexpr reference operator*() const {
            size_t bit_pos = W > 0 ? W - 1 - index_ : 0;
            if constexpr (IsConst) {
                return parent_->get_bit(bit_pos) ? Bit::_1 : Bit::_0;
            } else {
                return BitReference(*parent_, bit_pos);
            }
        }
        constexpr reference operator[](difference_type n) const { return *(*this + n); }
        constexpr IteratorImpl& operator++() {
            ++index_;
            return *this;
        }
        constexpr IteratorImpl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }
        constexpr IteratorImpl& operator--() {
            --index_;
            return *this;
        }
        constexpr IteratorImpl operator--(int) {
            auto copy = *this;
            --*this;
            return copy;
        }
        constexpr IteratorImpl& operator+=(difference_type n) {
            index_ += n;
            return *this;
        }
        constexpr IteratorImpl& operator-=(difference_type n) {
            index_ -= n;
            return *this;
        }
        constexpr IteratorImpl operator+(difference_type n) const {
            return IteratorImpl(parent_, index_ + n);
        }
        constexpr IteratorImpl operator-(difference_type n) const {
            return IteratorImpl(parent_, index_ - n);
        }
        friend constexpr IteratorImpl operator+(difference_type n, IteratorImpl const& it) {
            return it + n;
        }
        constexpr difference_type operator-(IteratorImpl const& other) const {
            return static_cast<difference_type>(index_)
                 - static_cast<difference_type>(other.index_);
        }
        constexpr bool operator==(IteratorImpl const& other) const {
            return parent_ == other.parent_ && index_ == other.index_;
        }
        constexpr auto operator<=>(IteratorImpl const& other) const {
            return index_ <=> other.index_;
        }
    };

    constexpr auto begin() { return IteratorImpl<false>(this, 0); }
    constexpr auto begin() const { return IteratorImpl<true>(this, 0); }
    constexpr auto end() { return IteratorImpl<false>(this, W); }
    constexpr auto end() const { return IteratorImpl<true>(this, W); }
    constexpr auto rbegin() { return std::make_reverse_iterator(end()); }
    constexpr auto rbegin() const { return std::make_reverse_iterator(end()); }
    constexpr auto rend() { return std::make_reverse_iterator(begin()); }
    constexpr auto rend() const { return std::make_reverse_iterator(begin()); }

    constexpr BitReference operator[](size_t index) {
        if (index >= W) {
            throw std::out_of_range("Bit index out of bounds");
        }
        return BitReference(*this, index);
    }
    constexpr Bit operator[](size_t index) const {
        return get_bit(index) ? Bit::_1 : Bit::_0;
    }

    constexpr size_t count_trailing_zeros() const {
        if constexpr (W == 0) {
            return 0;
        } else if constexpr (!is_wide) {
            IntType value = logical_native_value();
            if (value == 0) {
                return W;
            }
#if defined(__SIZEOF_INT128__)
            if constexpr (sizeof(IntType) > sizeof(Word)) {
                Word lower = static_cast<Word>(value);
                return lower != 0
                         ? std::countr_zero(lower)
                         : word_bits
                               + std::countr_zero(static_cast<Word>(value >> word_bits));
            } else
#endif
            {
                return std::countr_zero(value);
            }
        } else {
            return detail::count_trailing_zeros(logical_wide_cref());
        }
    }
    constexpr size_t count_leading_zeros() const {
        if constexpr (W == 0) {
            return 0;
        } else if constexpr (!is_wide) {
            IntType value = logical_native_value();
            if (value == 0) {
                return W;
            }
            constexpr size_t unused = physical_width - W;
#if defined(__SIZEOF_INT128__)
            if constexpr (sizeof(IntType) > sizeof(Word)) {
                Word upper = static_cast<Word>(value >> word_bits);
                return upper != 0
                         ? std::countl_zero(upper) - unused
                         : word_bits + std::countl_zero(static_cast<Word>(value)) - unused;
            } else
#endif
            {
                return std::countl_zero(value) - unused;
            }
        } else {
            auto logical = logical_wide_cref();
            size_t unused = logical.num_words() * word_bits - W;
            for (size_t i = logical.num_words(); i > 0; --i) {
                Word word = logical.word(i - 1);
                if (i == logical.num_words() && W % word_bits != 0) {
                    word &= (Word{1} << (W % word_bits)) - 1;
                }
                if (word != 0) {
                    return ((logical.num_words() - i) * word_bits) + std::countl_zero(word)
                         - unused;
                }
            }
            return W;
        }
    }
    constexpr size_t popcount() const {
        if constexpr (W == 0) {
            return 0;
        } else if constexpr (!is_wide) {
            IntType value = logical_native_value();
#if defined(__SIZEOF_INT128__)
            if constexpr (sizeof(IntType) > sizeof(Word)) {
                return std::popcount(static_cast<Word>(value))
                     + std::popcount(static_cast<Word>(value >> word_bits));
            } else
#endif
            {
                return std::popcount(value);
            }
        } else {
            auto logical = logical_wide_cref();
            size_t result = 0;
            for (size_t i = 0; i < logical.num_words(); ++i) {
                Word word = logical.word(i);
                if (i + 1 == logical.num_words() && W % word_bits != 0) {
                    word &= (Word{1} << (W % word_bits)) - 1;
                }
                result += std::popcount(word);
            }
            return result;
        }
    }

    friend constexpr bool operator==(Int const& lhs, Int const& rhs) {
        if constexpr (W == 0) {
            return true;
        } else {
            return lhs.storage_ == rhs.storage_;
        }
    }

    friend constexpr std::strong_ordering operator<=>(Int const& lhs, Int const& rhs) {
        if (lhs.less(rhs)) {
            return std::strong_ordering::less;
        }
        if (rhs.less(lhs)) {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }

    constexpr Int operator&(Int const& other) const {
        if constexpr (W == 0) {
            return Int{};
        } else if constexpr (!is_wide) {
            Int result;
            result.storage_ = static_cast<IntType>(storage_ & other.storage_);
            return result;
        } else {
            Int result(*this);
            and_assign(
                WordSpan{std::span<Word>{result.storage_}, physical_width},
                WordConstSpan{std::span<Word const>{other.storage_}, physical_width}
            );
            return result;
        }
    }
    constexpr Int operator|(Int const& other) const {
        if constexpr (W == 0) {
            return Int{};
        } else if constexpr (!is_wide) {
            Int result;
            result.storage_ = static_cast<IntType>(storage_ | other.storage_);
            return result;
        } else {
            Int result(*this);
            or_assign(
                WordSpan{std::span<Word>{result.storage_}, physical_width},
                WordConstSpan{std::span<Word const>{other.storage_}, physical_width}
            );
            return result;
        }
    }
    constexpr Int operator^(Int const& other) const {
        if constexpr (W == 0) {
            return Int{};
        } else if constexpr (!is_wide) {
            Int result;
            result.storage_ = static_cast<IntType>(storage_ ^ other.storage_);
            return result;
        } else {
            Int result(*this);
            xor_assign(
                WordSpan{std::span<Word>{result.storage_}, physical_width},
                WordConstSpan{std::span<Word const>{other.storage_}, physical_width}
            );
            return result;
        }
    }
    constexpr Int operator~() const {
        if constexpr (W == 0) {
            return Int{};
        } else if constexpr (!is_wide) {
            Int result;
            result.storage_ = static_cast<IntType>(~storage_);
            if constexpr (!SignedRepresentation) {
                result.canonicalize();
            }
            return result;
        } else {
            Int result(*this);
            bitnot(WordSpan{std::span<Word>{result.storage_}, physical_width});
            if constexpr (!SignedRepresentation) {
                result.canonicalize();
            }
            return result;
        }
    }
    constexpr Int operator<<(size_t amount) const {
        if constexpr (W == 0) {
            return Int{};
        } else {
            if (amount >= W) {
                return Int{};
            }
            if constexpr (!is_wide) {
                Int result;
                result.storage_ = static_cast<IntType>(storage_ << amount);
                result.canonicalize();
                return result;
            } else {
                Int result(*this);
                shift_left(
                    WordSpan{std::span<Word>{result.storage_}, physical_width}, amount
                );
                result.canonicalize();
                return result;
            }
        }
    }
    constexpr Int operator>>(size_t amount) const {
        if constexpr (W == 0) {
            return Int{};
        } else {
            if (amount >= W) {
                if constexpr (SignedRepresentation) {
                    return get_bit(W - 1) ? ~Int{} : Int{};
                } else {
                    return Int{};
                }
            }
            if constexpr (!is_wide) {
                Int result;
                if constexpr (SignedRepresentation) {
                    using SignedIntType = typename as_signed<IntType>::type;
                    result.storage_ = static_cast<IntType>(
                        static_cast<SignedIntType>(storage_) >> amount
                    );
                } else {
                    result.storage_ = static_cast<IntType>(storage_ >> amount);
                }
                return result;
            } else {
                Int result(*this);
                if constexpr (SignedRepresentation) {
                    shift_right_arith(
                        WordSpan{std::span<Word>{result.storage_}, physical_width}, amount
                    );
                } else {
                    shift_right_logical(
                        WordSpan{std::span<Word>{result.storage_}, physical_width}, amount
                    );
                }
                return result;
            }
        }
    }

    template <size_t TargetW>
        requires(TargetW <= W)
    constexpr Int<TargetW, SignedRepresentation> truncate() const {
        return Int<TargetW, SignedRepresentation>(*this);
    }

    template <size_t TargetW>
    constexpr Int<TargetW, false> saturate_unsigned() const
        requires(!SignedRepresentation)
    {
        if constexpr (TargetW >= W) {
            return Int<TargetW, false>(*this);
        } else if constexpr (TargetW == 0) {
            return Int<0, false>{};
        } else {
            Int<TargetW, false> maximum = ~Int<TargetW, false>{};
            return !maximum.less(*this) ? Int<TargetW, false>(*this) : maximum;
        }
    }
    template <size_t TargetW>
    constexpr Int<TargetW, true> saturate_signed() const
        requires SignedRepresentation
    {
        if constexpr (TargetW >= W) {
            return Int<TargetW, true>(*this);
        } else if constexpr (TargetW == 0) {
            return Int<0, true>{};
        } else {
            Int<TargetW, true> minimum{};
            minimum.set_bit(TargetW - 1, true);
            Int<TargetW, true> maximum = ~minimum;
            if (less(minimum)) {
                return minimum;
            }
            if (maximum.less(*this)) {
                return maximum;
            }
            return Int<TargetW, true>(*this);
        }
    }

    std::string to_binary_string() const {
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            return std::format(
                "{:0{}b}", static_cast<wide_uint>(logical_native_value()), W
            );
        } else {
            return format_power_of_two(logical_wide_cref(), 1, W, [](uint8_t d) {
                return static_cast<char>('0' + d);
            });
        }
    }
    std::string to_decimal_string() const {
        return to_decimal_string(SignedRepresentation);
    }
    std::string to_decimal_string(bool signed_value) const {
        if constexpr (W == 0) {
            return "";
        } else if constexpr (is_wide) {
            return format_decimal(
                WordConstSpan{std::span<Word const>{storage_}, physical_width}, signed_value
            );
        } else if (signed_value) {
            using SignedIntType = typename as_signed<IntType>::type;
            return std::format(
                "{}", static_cast<wide_int>(static_cast<SignedIntType>(storage_))
            );
        } else {
            return std::format("{}", static_cast<wide_uint>(storage_));
        }
    }
    std::string to_hexadecimal_string() const {
        constexpr size_t digits = (W + 3) / 4;
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            return std::format(
                "{:0{}x}", static_cast<wide_uint>(logical_native_value()), digits
            );
        } else {
            char const hex_digits[] = "0123456789abcdef";
            return format_power_of_two(logical_wide_cref(), 4, digits, [&](uint8_t d) {
                return hex_digits[d];
            });
        }
    }
    std::string to_octal_string() const {
        constexpr size_t digits = (W + 2) / 3;
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            return std::format(
                "{:0{}o}", static_cast<wide_uint>(logical_native_value()), digits
            );
        } else {
            return format_power_of_two(logical_wide_cref(), 3, digits, [](uint8_t d) {
                return static_cast<char>('0' + d);
            });
        }
    }

    static constexpr Int exact_add(Int const& a, Int const& b) {
        if constexpr (W == 0) {
            return Int{};
        }
        Int result;
        if constexpr (!is_wide && W > 0) {
            result.storage_ = static_cast<IntType>(
                static_cast<NativeArithmetic>(a.storage_)
                + static_cast<NativeArithmetic>(b.storage_)
            );
        } else {
            result = arithmetic(a, b, '+');
        }
        result.canonicalize();
        return result;
    }
    static constexpr Int exact_sub(Int const& a, Int const& b) {
        if constexpr (W == 0) {
            return Int{};
        }
        Int result;
        if constexpr (!is_wide && W > 0) {
            result.storage_ = static_cast<IntType>(
                static_cast<NativeArithmetic>(a.storage_)
                - static_cast<NativeArithmetic>(b.storage_)
            );
        } else {
            result = arithmetic(a, b, '-');
        }
        result.canonicalize();
        return result;
    }
    static constexpr Int exact_mul(Int const& a, Int const& b) {
        if constexpr (W == 0) {
            return Int{};
        }
        Int result;
        if constexpr (!is_wide && W > 0) {
            result.storage_ = static_cast<IntType>(
                static_cast<NativeArithmetic>(a.storage_)
                * static_cast<NativeArithmetic>(b.storage_)
            );
        } else {
            result = arithmetic(a, b, '*');
        }
        result.canonicalize();
        return result;
    }

    template <size_t Wa, bool Sa, size_t Wb, bool Sb>
    static constexpr Int arithmetic(Int<Wa, Sa> const& a, Int<Wb, Sb> const& b, char op) {
        static_assert(Sa == Sb);
        Int result;
        if constexpr (
            W > 0 && Wa > 0 && Wb > 0 && !is_wide && !Int<Wa, Sa>::is_wide
            && !Int<Wb, Sb>::is_wide
        )
        {
            if constexpr (Sa) {
                using ResultSigned = typename as_signed<IntType>::type;
                using A_Signed = typename as_signed<typename Int<Wa, Sa>::IntType>::type;
                using B_Signed = typename as_signed<typename Int<Wb, Sb>::IntType>::type;
                ResultSigned lhs =
                    static_cast<ResultSigned>(static_cast<A_Signed>(a.storage_));
                ResultSigned rhs =
                    static_cast<ResultSigned>(static_cast<B_Signed>(b.storage_));
                if (op == '+') {
                    result.storage_ = static_cast<IntType>(lhs + rhs);
                } else if (op == '-') {
                    result.storage_ = static_cast<IntType>(lhs - rhs);
                } else {
                    result.storage_ = static_cast<IntType>(lhs * rhs);
                }
            } else {
                IntType lhs = static_cast<IntType>(a.storage_);
                IntType rhs = static_cast<IntType>(b.storage_);
                if (op == '+') {
                    result.storage_ = static_cast<IntType>(lhs + rhs);
                } else if (op == '-') {
                    result.storage_ = static_cast<IntType>(lhs - rhs);
                } else {
                    result.storage_ = static_cast<IntType>(lhs * rhs);
                }
            }
            return result;
        }
        NativeBuffer result_buffer{};
        typename Int<Wa, Sa>::NativeBuffer a_buffer{};
        typename Int<Wb, Sb>::NativeBuffer b_buffer{};
        auto out = result.physical_mut(result_buffer);
        auto av = a.physical_wide_cref(a_buffer);
        auto bv = b.physical_wide_cref(b_buffer);
        if (op == '+') {
            add_extended(out, av, bv, Sa, Sb);
        } else if (op == '-') {
            sub_extended(out, av, bv, Sa, Sb);
        } else if constexpr (Sa && Sb) {
            multiply_signed(out, av, bv);
        } else {
            multiply_unsigned(out, av, bv);
        }
        result.finish_output(result_buffer);
        return result;
    }

    template <size_t Wa, bool Sa, size_t Wb, bool Sb>
    static constexpr std::pair<Int, Int<Wb, SignedRepresentation>> divide(
        Int<Wa, Sa> const& a, Int<Wb, Sb> const& b, bool modulo
    ) {
        static_assert(Sa == Sb && Sa == SignedRepresentation);
        if constexpr (
            Wa > 0 && Wb > 0 && !is_wide && !Int<Wb, SignedRepresentation>::is_wide
            && !Int<Wa, Sa>::is_wide && !Int<Wb, Sb>::is_wide
        )
        {
            Int quotient;
            Int<Wb, SignedRepresentation> remainder;
            constexpr size_t common_width =
                std::max(Int<Wa, Sa>::physical_width, Int<Wb, Sb>::physical_width);
            using CommonUnsigned = typename IntTypePicker<common_width>::type;
            if constexpr (SignedRepresentation) {
                using CommonSigned = typename as_signed<CommonUnsigned>::type;
                using A_Signed = typename as_signed<typename Int<Wa, Sa>::IntType>::type;
                using B_Signed = typename as_signed<typename Int<Wb, Sb>::IntType>::type;
                CommonSigned lhs =
                    static_cast<CommonSigned>(static_cast<A_Signed>(a.storage_));
                CommonSigned rhs =
                    static_cast<CommonSigned>(static_cast<B_Signed>(b.storage_));
                if (rhs == 0) {
                    throw std::domain_error("Division by zero");
                }
                if (lhs == std::numeric_limits<CommonSigned>::min() && rhs == -1) {
                    quotient.storage_ =
                        static_cast<IntType>(CommonUnsigned{1} << (common_width - 1));
                    remainder.storage_ = {};
                } else {
                    auto quotient_value = lhs / rhs;
                    auto remainder_value = lhs % rhs;
                    if (modulo && remainder_value != 0 && (lhs < 0) != (rhs < 0)) {
                        --quotient_value;
                        remainder_value += rhs;
                    }
                    quotient.storage_ = static_cast<IntType>(quotient_value);
                    remainder.storage_ =
                        static_cast<typename Int<Wb, SignedRepresentation>::IntType>(
                            remainder_value
                        );
                }
            } else {
                CommonUnsigned lhs = static_cast<CommonUnsigned>(a.storage_);
                CommonUnsigned rhs = static_cast<CommonUnsigned>(b.storage_);
                if (rhs == 0) {
                    throw std::domain_error("Division by zero");
                }
                quotient.storage_ = static_cast<IntType>(lhs / rhs);
                remainder.storage_ =
                    static_cast<typename Int<Wb, SignedRepresentation>::IntType>(lhs % rhs);
            }
            return {quotient, remainder};
        }
        constexpr size_t magnitude_width = integer_storage_width(std::max(Wa, Wb) + 1);
        constexpr size_t max_limbs =
            (magnitude_width + word_bits - 1) / word_bits * limbs_per_word;
        Int quotient;
        Int<Wb, SignedRepresentation> remainder;
        Int<Wa, false> lhs_magnitude;
        Int<Wb, false> rhs_magnitude;
        NativeBuffer quotient_buffer{};
        typename Int<Wb, SignedRepresentation>::NativeBuffer remainder_buffer{};
        typename Int<Wa, false>::NativeBuffer lhs_magnitude_buffer{};
        typename Int<Wb, false>::NativeBuffer rhs_magnitude_buffer{};
        typename Int<Wa, Sa>::NativeBuffer a_buffer{};
        typename Int<Wb, Sb>::NativeBuffer b_buffer{};
        std::array<DivLimb, max_limbs * 2 + 1> u{};
        std::array<DivLimb, max_limbs> v{};
        std::array<DivLimb, max_limbs * 2> q{};
        std::array<DivLimb, max_limbs> r{};
        DivideScratch scratch{u, v, q, r};
        auto quotient_view = quotient.physical_mut(quotient_buffer);
        auto remainder_view = remainder.physical_mut(remainder_buffer);
        auto av = a.physical_wide_cref(a_buffer);
        auto bv = b.physical_wide_cref(b_buffer);
        if constexpr (SignedRepresentation) {
            auto lhs_magnitude_view = lhs_magnitude.physical_mut(lhs_magnitude_buffer);
            auto rhs_magnitude_view = rhs_magnitude.physical_mut(rhs_magnitude_buffer);
            if (modulo) {
                divide_modulo(
                    quotient_view,
                    remainder_view,
                    av,
                    bv,
                    lhs_magnitude_view,
                    rhs_magnitude_view,
                    scratch
                );
            } else {
                divide_signed(
                    quotient_view,
                    remainder_view,
                    av,
                    bv,
                    lhs_magnitude_view,
                    rhs_magnitude_view,
                    scratch
                );
            }
        } else {
            divide_unsigned(quotient_view, remainder_view, av, bv, scratch);
        }
        quotient.finish_output(quotient_buffer);
        remainder.finish_output(remainder_buffer);
        return {quotient, remainder};
    }

  private:
    IntType storage_{};
};

template <size_t W>
using UInt = Int<W, false>;

template <size_t W>
using SInt = Int<W, true>;

template <size_t Wa, size_t Wb>
constexpr UInt<std::max(Wa, Wb) + 1> operator+(UInt<Wa> const& a, UInt<Wb> const& b) {
    return UInt<std::max(Wa, Wb) + 1>::arithmetic(a, b, '+');
}

template <size_t Wa, size_t Wb>
constexpr SInt<std::max(Wa, Wb) + 1> operator+(SInt<Wa> const& a, SInt<Wb> const& b) {
    return SInt<std::max(Wa, Wb) + 1>::arithmetic(a, b, '+');
}

template <size_t Wa, size_t Wb>
constexpr SInt<std::max(Wa, Wb) + 1> operator-(UInt<Wa> const& a, UInt<Wb> const& b) {
    return SInt<std::max(Wa, Wb) + 1>::arithmetic(a, b, '-');
}

template <size_t Wa, size_t Wb>
constexpr SInt<std::max(Wa, Wb) + 1> operator-(SInt<Wa> const& a, SInt<Wb> const& b) {
    return SInt<std::max(Wa, Wb) + 1>::arithmetic(a, b, '-');
}

template <size_t Wa, size_t Wb>
constexpr UInt<Wa + Wb> operator*(UInt<Wa> const& a, UInt<Wb> const& b) {
    return UInt<Wa + Wb>::arithmetic(a, b, '*');
}

template <size_t Wa, size_t Wb>
constexpr SInt<Wa + Wb> operator*(SInt<Wa> const& a, SInt<Wb> const& b) {
    return SInt<Wa + Wb>::arithmetic(a, b, '*');
}

template <size_t Wa, size_t Wb>
constexpr std::pair<UInt<Wa + 1>, UInt<Wb>> divrem(UInt<Wa> const& a, UInt<Wb> const& b) {
    return UInt<Wa + 1>::divide(a, b, false);
}

template <size_t Wa, size_t Wb>
constexpr std::pair<SInt<Wa + 1>, SInt<Wb>> divrem(SInt<Wa> const& a, SInt<Wb> const& b) {
    return SInt<Wa + 1>::divide(a, b, false);
}

template <size_t Wa, size_t Wb>
constexpr std::pair<SInt<Wa + 1>, SInt<Wb>> divmod(SInt<Wa> const& a, SInt<Wb> const& b) {
    return SInt<Wa + 1>::divide(a, b, true);
}

template <size_t Wa, size_t Wb>
constexpr UInt<Wa + 1> operator/(UInt<Wa> const& a, UInt<Wb> const& b) {
    return divrem(a, b).first;
}
template <size_t Wa, size_t Wb>
constexpr SInt<Wa + 1> operator/(SInt<Wa> const& a, SInt<Wb> const& b) {
    return divrem(a, b).first;
}
template <size_t Wa, size_t Wb>
constexpr UInt<Wb> operator%(UInt<Wa> const& a, UInt<Wb> const& b) {
    return divrem(a, b).second;
}
template <size_t Wa, size_t Wb>
constexpr SInt<Wb> operator%(SInt<Wa> const& a, SInt<Wb> const& b) {
    return divrem(a, b).second;
}
template <size_t Wa, size_t Wb>
constexpr SInt<Wb> mod(SInt<Wa> const& a, SInt<Wb> const& b) {
    return divmod(a, b).second;
}

template <size_t W>
constexpr SInt<W + 1> operator-(SInt<W> const& a) {
    SInt<W + 1> extended(a);
    return SInt<W + 1>::arithmetic(SInt<W + 1>{}, extended, '-');
}

template <size_t W>
constexpr SInt<W + 1> operator-(UInt<W> const& a) {
    return SInt<W + 1>::arithmetic(UInt<W>{}, a, '-');
}

template <size_t W>
constexpr SInt<W + 1> abs(SInt<W> const& a) {
    SInt<W + 1> extended(a);
    if constexpr (W == 0) {
        return extended;
    } else {
        return a.get_bit(W - 1) ? -a : extended;
    }
}

template <size_t bits>
constexpr auto max_unsigned() {
    if constexpr ((bits > 64 && !supports_128B) || (bits > 128)) {
        return ~UInt<bits>{};
    } else if constexpr (bits > 64) {
        if constexpr (bits == 128) {
            return ~__uint128_t{0};
        } else {
            return ((__uint128_t)1 << bits) - 1;
        }
    } else if constexpr (bits == 64) {
        return ~uint64_t{0};
    } else {
        return (uint64_t{1} << bits) - 1;
    }
}

// Build a {n-1 DOWNTO 0} Range from a length, the HDL convention for numeric
// types. Used by Unsigned/Signed/DynUnsigned/DynSigned constructors that take
// just a width.
constexpr Range int_downto_range(size_t n) {
    return Range{static_cast<Range::value_type>(n) - 1, Direction::DOWNTO, 0};
}

// Range NTTP dispatcher for the Unsigned<...>/Signed<...> template aliases.
// Same shape as the logic_array `make_logic_static_range`: defaults to DOWNTO
// when the user didn't pick a direction explicitly.
//   `Unsigned<8>`        -> {7 DOWNTO 0}
//   `Unsigned<Range{R}>` -> R (passthrough)
//   `Unsigned<7, 0>`     -> {7 DOWNTO 0}  (auto)
//   `Unsigned<3, 3>`     -> {3 DOWNTO 3}  (default DOWNTO when L == R)
//   `Unsigned<0, 7>`     -> {0 TO 7}      (auto)
//   `Unsigned<L, D, R>`  -> {L D R}       (explicit)
template <auto... Args>
constexpr Range make_int_range() {
    static_assert(
        sizeof...(Args) >= 1 && sizeof...(Args) <= 3,
        "Unsigned/Signed takes 1 to 3 range args"
    );
    constexpr auto t = std::tuple{Args...};
    if constexpr (sizeof...(Args) == 1) {
        using First = std::remove_cvref_t<decltype(std::get<0>(t))>;
        if constexpr (std::is_same_v<First, Range>) {
            return std::get<0>(t);
        } else {
            static_assert(
                std::integral<First>,
                "single template arg must be a Range value or an integral length"
            );
            static_assert(std::get<0>(t) >= 0, "length must be non-negative");
            return int_downto_range(static_cast<size_t>(std::get<0>(t)));
        }
    } else if constexpr (sizeof...(Args) == 2) {
        constexpr Range r{
            static_cast<Range::value_type>(std::get<0>(t)),
            static_cast<Range::value_type>(std::get<1>(t))
        };
        if constexpr (r.left == r.right) {
            return Range{r.left, Direction::DOWNTO, r.right};
        } else {
            return r;
        }
    } else {  // 3
        static_assert(
            std::is_same_v<std::remove_cvref_t<decltype(std::get<1>(t))>, Direction>,
            "three-arg form requires (left, Direction, right)"
        );
        return Range{
            static_cast<Range::value_type>(std::get<0>(t)),
            std::get<1>(t),
            static_cast<Range::value_type>(std::get<2>(t))
        };
    }
}

template <typename T>
class [[nodiscard]] auto_reinterpreted {
    T value_;

  public:
    constexpr explicit auto_reinterpreted(T v) : value_(std::forward<T>(v)) {}

    auto_reinterpreted(auto_reinterpreted const&) = delete;
    auto_reinterpreted& operator=(auto_reinterpreted const&) = delete;

    constexpr auto_reinterpreted(auto_reinterpreted&& other) noexcept
        : value_(std::forward<T>(other.value_)) {}

    constexpr auto_reinterpreted& operator=(auto_reinterpreted&&) = delete;

    constexpr T consume() && { return std::forward<T>(value_); }
};

template <typename T>
class [[nodiscard]] auto_resized {
    T value_;
    overflow_mode ovf_;
    round_mode rnd_;

  public:
    constexpr auto_resized(T v, overflow_mode o, round_mode r)
        : value_(std::forward<T>(v)), ovf_(o), rnd_(r) {}

    auto_resized(auto_resized const&) = delete;
    auto_resized& operator=(auto_resized const&) = delete;

    constexpr auto_resized(auto_resized&& other) noexcept
        : value_(std::forward<T>(other.value_)), ovf_(other.ovf_), rnd_(other.rnd_) {}

    constexpr auto_resized& operator=(auto_resized&&) = delete;
    constexpr std::tuple<T, overflow_mode, round_mode> consume() && {
        return {std::forward<T>(value_), ovf_, rnd_};
    }
};

template <typename T>
[[nodiscard]] constexpr auto_reinterpreted<T const&> as(T const& x) noexcept {
    return auto_reinterpreted<T const&>(x);
}

template <typename T>
    requires(!std::is_lvalue_reference_v<T>)
[[nodiscard]] constexpr auto_reinterpreted<T> as(T&& x) noexcept {
    return auto_reinterpreted<T>(std::move(x));
}

template <typename T>
[[nodiscard]] constexpr auto_resized<T const&> resize(
    T const& x,
    overflow_mode ovf = overflow_mode::wrap,
    round_mode rnd = round_mode::truncate
) noexcept {
    return auto_resized<T const&>(x, ovf, rnd);
}

// Prvalue (temporary) overload
template <typename T>
    requires(!std::is_lvalue_reference_v<T>)
[[nodiscard]] constexpr auto_resized<T> resize(
    T&& x, overflow_mode ovf = overflow_mode::wrap, round_mode rnd = round_mode::truncate
) noexcept {
    return auto_resized<T>(std::move(x), ovf, rnd);
}

}  // namespace detail

template <detail::HasBits Target, detail::HasBits Source>
constexpr Target as(Source const& source) noexcept {
    static_assert(
        Target::static_range.length() == Source::static_range.length(),
        "as() requires equal widths."
    );
    return Target(detail::bits(source));
}

}  // namespace coconext::types

#endif  // COCONEXT_INT_BASE_HPP

#ifndef COCONEXT_DYN_INT_BASE_HPP
#define COCONEXT_DYN_INT_BASE_HPP

#include <algorithm>
#include <array>
#include <climits>
#include <coconext/types/bigint.hpp>
#include <coconext/types/int_base.hpp>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace coconext::types::detail {

// Runtime-width owning integer representation. Values up to 64 bits use an
// unsigned native scalar; larger values own a Word array. Storage, canonical
// extension, and all algorithms live together; only the storage-agnostic
// WordSpan kernels remain outside this class.
template <bool SignedRepresentation>
class DynInt {
    template <bool>
    friend class DynInt;

  public:
    using NativeUInt = std::uint64_t;
    using NativeSInt = std::int64_t;

    static constexpr bool is_signed = SignedRepresentation;
    static constexpr size_t sbo_bits = std::numeric_limits<NativeUInt>::digits;

    static_assert(sizeof(NativeUInt) == sizeof(Word));
    static_assert(sizeof(NativeSInt) == sizeof(NativeUInt));

    explicit DynInt(size_t width) : width_(width) { initialize_storage(); }

    template <NativeInteger IntT>
    DynInt(size_t width, IntT val) : DynInt(width) {
        if (width == 0) {
            throw std::invalid_argument("DynInt(0) has no integer representation");
        }
        assert((native_value_fits<SignedRepresentation>(width, val)));
        if (is_native()) {
            storage_.native_ = native_from_value(val);
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
    }

#if defined(__SIZEOF_INT128__)
    DynInt(size_t width, __int128_t val) : DynInt(width) {
        assert((native_value_fits<SignedRepresentation>(width, val)));
        assign_int128(val);
    }
    DynInt(size_t width, __uint128_t val) : DynInt(width) {
        assert((native_value_fits<SignedRepresentation>(width, val)));
        assign_uint128(val);
    }
#endif

    DynInt(size_t width, std::string_view str) : DynInt(width) {
        if (is_native()) {
            storage_.native_ = parse_native(str);
        } else {
            parse_into<SignedRepresentation>(heap_logical_mut(), str);
        }
    }

    template <size_t W, bool OtherSigned>
    explicit DynInt(Int<W, OtherSigned> const& src) : DynInt(W) {
        if constexpr (W > 0) {
            if constexpr (W <= sbo_bits) {
                storage_.native_ = native_from_logical_bits(
                    src.logical_bits().template to_native_integer<NativeUInt>()
                );
            } else if constexpr (Int<W, OtherSigned>::is_wide) {
                auto const& source = src.storage_;
                auto destination = heap_words();
                for (size_t i = 0; i < destination.size(); ++i) {
                    destination[i] = source[i];
                }
            } else {
#if defined(__SIZEOF_INT128__)
                if constexpr (sizeof(typename Int<W, OtherSigned>::IntType) > sizeof(Word))
                {
                    if constexpr (OtherSigned) {
                        assign_int128(src.template to_native_integer<__int128_t>());
                    } else {
                        assign_uint128(src.template to_native_integer<__uint128_t>());
                    }
                } else
#endif
                {
                    if constexpr (OtherSigned) {
                        using SourceSigned =
                            typename as_signed<typename Int<W, OtherSigned>::IntType>::type;
                        assign_native(src.template to_native_integer<SourceSigned>());
                    } else {
                        assign_native(src.template to_native_integer<
                                      typename Int<W, OtherSigned>::IntType>());
                    }
                }
            }
            if constexpr (W > sbo_bits && OtherSigned != SignedRepresentation) {
                canonicalize_padding_bits();
            }
        }
    }

    template <bool OtherSigned>
    explicit DynInt(DynInt<OtherSigned> const& other) : DynInt(other.width(), other) {}

    template <bool OtherSigned>
    DynInt(size_t width, DynInt<OtherSigned> const& other) : DynInt(width) {
        if (is_native()) {
            storage_.native_ =
                native_from_logical_bits(static_cast<NativeUInt>(other.low_word()));
        } else {
            auto destination = heap_words();
            Word extension = Word{0};
            if constexpr (OtherSigned) {
                extension = other.is_negative() ? ~Word{0} : Word{0};
            }
            for (size_t i = 0; i < destination.size(); ++i) {
                destination[i] = i < other.num_words() ? other.word(i) : extension;
            }
            if (width_ < other.width_ || OtherSigned != SignedRepresentation) {
                canonicalize_padding_bits();
            }
        }
    }

    DynInt(DynInt const& other) : DynInt(other.width_) {
        if (is_native()) {
            storage_.native_ = other.storage_.native_;
        } else {
            auto destination = heap_words();
            auto source = other.heap_words();
            for (size_t i = 0; i < destination.size(); ++i) {
                destination[i] = source[i];
            }
        }
    }

    DynInt(DynInt&& other) noexcept : width_(other.width_) {
        if (is_native()) {
            storage_.native_ = other.storage_.native_;
        } else {
            std::construct_at(&storage_.heap_, std::move(other.storage_.heap_));
            std::destroy_at(&other.storage_.heap_);
            other.width_ = 0;
            std::construct_at(&other.storage_.native_, NativeUInt{0});
        }
    }

    template <bool OtherSigned>
        requires(OtherSigned != SignedRepresentation)
    DynInt(DynInt<OtherSigned>&& other) noexcept : width_(other.width_) {
        if (is_native()) {
            storage_.native_ = native_from_logical_bits(other.logical_native_value());
        } else {
            std::construct_at(&storage_.heap_, std::move(other.storage_.heap_));
            std::destroy_at(&other.storage_.heap_);
            other.width_ = 0;
            std::construct_at(&other.storage_.native_, NativeUInt{0});
            canonicalize_padding_bits();
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
            if (!is_native()) {
                std::destroy_at(&storage_.heap_);
                std::construct_at(&storage_.native_, NativeUInt{0});
            }
            width_ = other.width_;
            if (is_native()) {
                storage_.native_ = other.storage_.native_;
            } else {
                std::construct_at(&storage_.heap_, std::move(other.storage_.heap_));
                std::destroy_at(&other.storage_.heap_);
                other.width_ = 0;
                std::construct_at(&other.storage_.native_, NativeUInt{0});
            }
        }
        return *this;
    }

    ~DynInt() { destroy_storage(); }

    size_t width() const { return width_; }
    size_t num_words() const { return (width_ + word_bits - 1) / word_bits; }
    size_t physical_width() const {
        return width_ == 0 ? 0 : is_native() ? sbo_bits : num_words() * word_bits;
    }

    bool get_bit(size_t index) const {
        if (index >= width_) {
            throw std::out_of_range("Bit index out of bounds");
        }
        if (is_native()) {
            return (storage_.native_ >> index) & NativeUInt{1};
        }
        return (storage_.heap_[index / word_bits] >> (index % word_bits)) & Word{1};
    }

    bool is_negative() const noexcept
        requires SignedRepresentation
    {
        return width_ != 0
            && ((word((width_ - 1) / word_bits) >> ((width_ - 1) % word_bits)) & 1);
    }

    void set_bit(size_t index, bool val) {
        if (index >= width_) {
            throw std::out_of_range("Bit index out of bounds");
        }
        if (is_native()) {
            NativeUInt mask = NativeUInt{1} << index;
            storage_.native_ = val ? storage_.native_ | mask : storage_.native_ & ~mask;
            if constexpr (SignedRepresentation) {
                if (index == width_ - 1) {
                    storage_.native_ = native_from_logical_bits(storage_.native_);
                }
            }
        } else {
            Word mask = Word{1} << (index % word_bits);
            Word& target = storage_.heap_[index / word_bits];
            target = val ? target | mask : target & ~mask;
            if constexpr (SignedRepresentation) {
                if (index == width_ - 1) {
                    canonicalize_padding_bits();
                }
            }
        }
    }

    class BitReference {
        DynInt& parent_;
        size_t index_;

      public:
        BitReference(DynInt& parent, size_t index) : parent_(parent), index_(index) {}
        operator Bit() const { return parent_.get_bit(index_) ? Bit::_1 : Bit::_0; }
        explicit operator char() const { return parent_.get_bit(index_) ? '1' : '0'; }
        explicit operator bool() const { return parent_.get_bit(index_); }
        BitReference const& operator=(Bit val) const {
            parent_.set_bit(index_, static_cast<bool>(val));
            return *this;
        }
        BitReference const& operator=(BitReference const& other) const {
            parent_.set_bit(index_, static_cast<bool>(static_cast<Bit>(other)));
            return *this;
        }
    };

    template <bool IsConst>
    class IteratorImpl {
        using Parent = std::conditional_t<IsConst, DynInt const, DynInt>;
        Parent* parent_ = nullptr;
        size_t index_ = 0;

      public:
        using iterator_concept = std::random_access_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = Bit;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = std::conditional_t<IsConst, Bit, BitReference>;

        IteratorImpl() = default;
        IteratorImpl(Parent* parent, size_t index) : parent_(parent), index_(index) {}
        reference operator*() const {
            size_t bit_pos = parent_->width() > 0 ? parent_->width() - 1 - index_ : 0;
            if constexpr (IsConst) {
                return parent_->get_bit(bit_pos) ? Bit::_1 : Bit::_0;
            } else {
                return BitReference(*parent_, bit_pos);
            }
        }
        reference operator[](difference_type n) const { return *(*this + n); }
        IteratorImpl& operator++() {
            ++index_;
            return *this;
        }
        IteratorImpl operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }
        IteratorImpl& operator--() {
            --index_;
            return *this;
        }
        IteratorImpl operator--(int) {
            auto copy = *this;
            --*this;
            return copy;
        }
        IteratorImpl& operator+=(difference_type n) {
            index_ += n;
            return *this;
        }
        IteratorImpl& operator-=(difference_type n) {
            index_ -= n;
            return *this;
        }
        IteratorImpl operator+(difference_type n) const {
            return IteratorImpl(parent_, index_ + n);
        }
        IteratorImpl operator-(difference_type n) const {
            return IteratorImpl(parent_, index_ - n);
        }
        friend IteratorImpl operator+(difference_type n, IteratorImpl const& it) {
            return it + n;
        }
        difference_type operator-(IteratorImpl const& other) const {
            return static_cast<difference_type>(index_)
                 - static_cast<difference_type>(other.index_);
        }
        bool operator==(IteratorImpl const& other) const {
            return parent_ == other.parent_ && index_ == other.index_;
        }
        auto operator<=>(IteratorImpl const& other) const {
            return index_ <=> other.index_;
        }
    };

    auto begin() { return IteratorImpl<false>(this, 0); }
    auto begin() const { return IteratorImpl<true>(this, 0); }
    auto end() { return IteratorImpl<false>(this, width_); }
    auto end() const { return IteratorImpl<true>(this, width_); }
    auto rbegin() { return std::make_reverse_iterator(end()); }
    auto rbegin() const { return std::make_reverse_iterator(end()); }
    auto rend() { return std::make_reverse_iterator(begin()); }
    auto rend() const { return std::make_reverse_iterator(begin()); }

    BitReference operator[](size_t index) {
        if (index >= width_) {
            throw std::out_of_range("Bit index out of bounds");
        }
        return BitReference(*this, index);
    }
    Bit operator[](size_t index) const { return get_bit(index) ? Bit::_1 : Bit::_0; }

    DynInt<false> logical_bits() const { return DynInt<false>(width_, *this); }

    template <NativeInteger T>
    T to_native_integer() const {
        if (width_ == 0) {
            throw std::domain_error("zero-width DynInt has no native integer value");
        }

        constexpr size_t native_bits = sizeof(T) * CHAR_BIT;
        constexpr bool target_signed = std::numeric_limits<T>::is_signed;
        bool negative = false;
        if constexpr (SignedRepresentation) {
            negative = is_negative();
        }

        bool fits = true;
        if constexpr (target_signed) {
            if constexpr (SignedRepresentation) {
                if (width_ > native_bits) {
                    for (size_t i = native_bits - 1; i < width_; ++i) {
                        fits &= get_bit(i) == negative;
                    }
                }
            } else if (width_ >= native_bits) {
                for (size_t i = native_bits - 1; i < width_; ++i) {
                    fits &= !get_bit(i);
                }
            }
        } else {
            if constexpr (SignedRepresentation) {
                fits &= !negative;
            }
            if (width_ > native_bits) {
                for (size_t i = native_bits; i < width_; ++i) {
                    fits &= !get_bit(i);
                }
            }
        }
        if (!fits) {
            throw std::out_of_range("Value outside destination native type range");
        }

        wide_uint bits = 0;
        size_t const bits_to_copy = std::min(width_, native_bits);
        for (size_t i = 0; i < bits_to_copy; ++i) {
            if (get_bit(i)) {
                bits |= wide_uint{1} << i;
            }
        }

        if constexpr (target_signed) {
            if (negative) {
                if (width_ < native_bits) {
                    bits |= ~wide_uint{0} << width_;
                }
                constexpr wide_uint target_mask = [] {
                    if constexpr (native_bits == sizeof(wide_uint) * CHAR_BIT) {
                        return ~wide_uint{0};
                    } else {
                        return (wide_uint{1} << native_bits) - 1;
                    }
                }();
                wide_uint const magnitude = (~bits + 1) & target_mask;
                if (magnitude == (wide_uint{1} << std::numeric_limits<T>::digits)) {
                    return std::numeric_limits<T>::min();
                }
                return static_cast<T>(-static_cast<wide_int>(magnitude));
            }
        }
        return static_cast<T>(bits);
    }

    size_t popcount() const {
        if (is_native()) {
            return std::popcount(logical_native_value());
        }
        size_t count = 0;
        auto data = heap_words();
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
            NativeUInt value = logical_native_value();
            return value == 0 ? width_ : std::countl_zero(value) - (sbo_bits - width_);
        }
        auto data = heap_words();
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
            NativeUInt value = logical_native_value();
            return value == 0 ? width_ : std::countr_zero(value);
        }
        return detail::count_trailing_zeros(heap_logical_cref());
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
            and_assign(result.heap_physical_mut(), other.heap_physical_cref());
        }
        return result;
    }
    DynInt operator|(DynInt const& other) const {
        check_same_width(other);
        DynInt result(*this);
        if (is_native()) {
            result.storage_.native_ |= other.storage_.native_;
        } else {
            or_assign(result.heap_physical_mut(), other.heap_physical_cref());
        }
        return result;
    }
    DynInt operator^(DynInt const& other) const {
        check_same_width(other);
        DynInt result(*this);
        if (is_native()) {
            result.storage_.native_ ^= other.storage_.native_;
        } else {
            xor_assign(result.heap_physical_mut(), other.heap_physical_cref());
        }
        return result;
    }
    DynInt operator~() const {
        if (width_ == 0) {
            return DynInt(0);
        }
        DynInt result(*this);
        if (is_native()) {
            result.storage_.native_ = ~result.storage_.native_;
        } else {
            bitnot(result.heap_physical_mut());
        }
        if constexpr (!SignedRepresentation) {
            if (result.is_native()) {
                result.storage_.native_ =
                    result.native_from_logical_bits(result.storage_.native_);
            } else {
                result.canonicalize_padding_bits();
            }
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
            result.storage_.native_ =
                result.native_from_logical_bits(result.storage_.native_);
        } else {
            shift_left(result.heap_physical_mut(), amount);
            result.canonicalize_padding_bits();
        }
        return result;
    }
    DynInt operator>>(size_t amount) const {
        if (width_ == 0) {
            return DynInt(0);
        }
        if (amount >= width_) {
            if constexpr (SignedRepresentation) {
                return is_negative() ? ~DynInt(width_) : DynInt(width_);
            } else {
                return DynInt(width_);
            }
        }
        DynInt result(*this);
        if (is_native()) {
            if constexpr (SignedRepresentation) {
                result.storage_.native_ = static_cast<NativeUInt>(
                    static_cast<NativeSInt>(storage_.native_) >> amount
                );
            } else {
                result.storage_.native_ >>= amount;
            }
        } else if constexpr (SignedRepresentation) {
            shift_right_arith(result.heap_physical_mut(), amount);
        } else {
            shift_right_logical(result.heap_physical_mut(), amount);
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
        return format_power_of_two(heap_logical_cref(), 1, width_, [](uint8_t d) {
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
            return signed_value
                     ? std::format("{}", static_cast<NativeSInt>(storage_.native_))
                     : std::format("{}", storage_.native_);
        }
        return format_decimal(heap_physical_cref(), signed_value);
    }
    std::string to_hexadecimal_string() const {
        if (is_native()) {
            return width_ == 0
                     ? ""
                     : std::format("{:0{}x}", logical_native_value(), (width_ + 3) / 4);
        }
        char const digits[] = "0123456789abcdef";
        return format_power_of_two(
            heap_logical_cref(), 4, (width_ + 3) / 4, [&](uint8_t d) { return digits[d]; }
        );
    }
    std::string to_octal_string() const {
        if (is_native()) {
            return width_ == 0
                     ? ""
                     : std::format("{:0{}o}", logical_native_value(), (width_ + 2) / 3);
        }
        return format_power_of_two(heap_logical_cref(), 3, (width_ + 2) / 3, [](uint8_t d) {
            return static_cast<char>('0' + d);
        });
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
                    // The growing result width makes these operations
                    // representable in NativeSInt, so native signed arithmetic
                    // cannot overflow here.
                    NativeSInt lhs;
                    NativeSInt rhs;
                    if constexpr (Sa) {
                        lhs = static_cast<NativeSInt>(a.storage_.native_);
                        rhs = static_cast<NativeSInt>(b.storage_.native_);
                    } else {
                        lhs = static_cast<NativeSInt>(a.logical_native_value());
                        rhs = static_cast<NativeSInt>(b.logical_native_value());
                    }
                    if (operation == '+') {
                        result.storage_.native_ = static_cast<NativeUInt>(lhs + rhs);
                    } else if (operation == '-') {
                        result.storage_.native_ = static_cast<NativeUInt>(lhs - rhs);
                    } else {
                        result.storage_.native_ = static_cast<NativeUInt>(lhs * rhs);
                    }
                } else {
                    NativeUInt lhs = a.logical_native_value();
                    NativeUInt rhs = b.logical_native_value();
                    if (operation == '+') {
                        result.storage_.native_ = lhs + rhs;
                    } else if (operation == '-') {
                        result.storage_.native_ = lhs - rhs;
                    } else {
                        result.storage_.native_ = lhs * rhs;
                    }
                }
                return result;
            }
#if defined(__SIZEOF_INT128__)
            if constexpr (SignedRepresentation) {
                __int128_t lhs;
                __int128_t rhs;
                if constexpr (Sa) {
                    lhs = static_cast<NativeSInt>(a.storage_.native_);
                    rhs = static_cast<NativeSInt>(b.storage_.native_);
                } else {
                    lhs = static_cast<__int128_t>(a.logical_native_value());
                    rhs = static_cast<__int128_t>(b.logical_native_value());
                }
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
            return result;
#endif
        }

        NativeBuffer result_buffer{};
        typename DynInt<Sa>::NativeBuffer a_buffer{};
        typename DynInt<Sb>::NativeBuffer b_buffer{};
        auto output = result.physical_mut(result_buffer);
        auto lhs = a.physical_cref(a_buffer);
        auto rhs = b.physical_cref(b_buffer);
        if (operation == '+') {
            add_extended<Sa>(output, lhs, rhs);
        } else if (operation == '-') {
            sub_extended<Sa>(output, lhs, rhs);
        } else if constexpr (Sa) {
            multiply_signed(output, lhs, rhs);
        } else {
            multiply_unsigned(output, lhs, rhs);
        }
        result.finish_output(result_buffer);
        return result;
    }

    static std::pair<DynInt, DynInt> divide(DynInt const& a, DynInt const& b, bool modulo) {
        if (a.is_native() && b.is_native()) {
            if constexpr (SignedRepresentation) {
                NativeSInt lhs = static_cast<NativeSInt>(a.storage_.native_);
                NativeSInt rhs = static_cast<NativeSInt>(b.storage_.native_);
                if (rhs == 0) {
                    throw std::domain_error("Division by zero");
                }
                if (lhs == std::numeric_limits<NativeSInt>::min() && rhs == -1) {
                    return {
                        DynInt(a.width_ + 1, NativeUInt{1} << (sbo_bits - 1)),
                        DynInt(b.width_, NativeSInt{0})
                    };
                }
                NativeSInt quotient = lhs / rhs;
                NativeSInt remainder = lhs % rhs;
                if (modulo && remainder != 0 && (lhs < 0) != (rhs < 0)) {
                    --quotient;
                    remainder += rhs;
                }
                return {DynInt(a.width_ + 1, quotient), DynInt(b.width_, remainder)};
            } else {
                NativeUInt lhs = a.logical_native_value();
                NativeUInt rhs = b.logical_native_value();
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
        NativeBuffer quotient_buffer{};
        NativeBuffer remainder_buffer{};
        NativeBuffer a_buffer{};
        NativeBuffer b_buffer{};
        auto quotient_view = quotient.physical_mut(quotient_buffer);
        auto remainder_view = remainder.physical_mut(remainder_buffer);
        auto lhs = a.physical_cref(a_buffer);
        auto rhs = b.physical_cref(b_buffer);
        if constexpr (SignedRepresentation) {
            if (modulo) {
                divide_modulo(quotient_view, remainder_view, lhs, rhs, scratch.view);
            } else {
                divide_signed(quotient_view, remainder_view, lhs, rhs, scratch.view);
            }
        } else {
            divide_unsigned(quotient_view, remainder_view, lhs, rhs, scratch.view);
        }
        quotient.finish_output(quotient_buffer);
        remainder.finish_output(remainder_buffer);
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
                NativeSInt operand;
                if constexpr (SourceSigned) {
                    operand = static_cast<NativeSInt>(value.storage_.native_);
                } else {
                    operand = static_cast<NativeSInt>(value.logical_native_value());
                }
                result.storage_.native_ = static_cast<NativeUInt>(-operand);
                return result;
            }
#if defined(__SIZEOF_INT128__)
            __int128_t operand;
            if constexpr (SourceSigned) {
                operand = static_cast<NativeSInt>(value.storage_.native_);
            } else {
                operand = static_cast<__int128_t>(value.logical_native_value());
            }
            result.assign_int128(-operand);
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
        NativeUInt native_;
        std::unique_ptr<Word[]> heap_;

        constexpr Storage() : native_(0) {}
        ~Storage() {}
    } storage_;

    using NativeBuffer = std::array<Word, 1>;

    bool is_native() const { return width_ <= sbo_bits; }

    void initialize_storage() {
        if (is_native()) {
            storage_.native_ = 0;
        } else {
            std::construct_at(&storage_.heap_, std::make_unique<Word[]>(num_words()));
        }
    }

    void destroy_storage() {
        if (!is_native()) {
            std::destroy_at(&storage_.heap_);
        }
    }

    std::span<Word> heap_words() {
        assert(!is_native());
        return {storage_.heap_.get(), num_words()};
    }
    std::span<Word const> heap_words() const {
        assert(!is_native());
        return {storage_.heap_.get(), num_words()};
    }

    Word low_word() const { return width_ == 0 ? Word{0} : word(0); }
    Word word(size_t index) const {
        return is_native() ? native_physical_word() : storage_.heap_[index];
    }

    NativeUInt native_mask() const {
        return width_ == 0        ? NativeUInt{0}
             : width_ == sbo_bits ? ~NativeUInt{0}
                                  : (NativeUInt{1} << width_) - 1;
    }

    NativeUInt logical_native_value() const { return storage_.native_ & native_mask(); }

    template <typename IntT>
    static NativeUInt native_from_value(IntT value) {
        if constexpr (SignedRepresentation) {
            return static_cast<NativeUInt>(static_cast<NativeSInt>(value));
        } else {
            return static_cast<NativeUInt>(value);
        }
    }

    NativeUInt native_from_logical_bits(NativeUInt value) const {
        NativeUInt const mask = native_mask();
        value &= mask;
        if constexpr (SignedRepresentation) {
            if (width_ != 0 && width_ != sbo_bits) {
                NativeUInt const sign = NativeUInt{1} << (width_ - 1);
                value = (value ^ sign) - sign;
            }
        }
        return value;
    }

    Word native_physical_word() const {
        if constexpr (SignedRepresentation) {
            return static_cast<Word>(static_cast<NativeSInt>(storage_.native_));
        } else {
            return static_cast<Word>(storage_.native_);
        }
    }

    template <typename IntT>
        requires(std::is_integral_v<IntT> && sizeof(IntT) <= sizeof(Word))
    void assign_native(IntT value) {
        if (is_native()) {
            storage_.native_ = native_from_value(value);
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
            storage_.native_ = native_from_value(value);
            return;
        }
        storage_.heap_[0] = static_cast<Word>(value);
        if (num_words() > 1) {
            storage_.heap_[1] =
                static_cast<Word>(static_cast<__uint128_t>(value) >> word_bits);
        }
        Word extension = value < 0 ? ~Word{0} : Word{0};
        for (size_t i = 2; i < num_words(); ++i) {
            storage_.heap_[i] = extension;
        }
    }

    void assign_uint128(__uint128_t value) {
        if (is_native()) {
            storage_.native_ = native_from_value(value);
            return;
        }
        storage_.heap_[0] = static_cast<Word>(value);
        if (num_words() > 1) {
            storage_.heap_[1] = static_cast<Word>(value >> word_bits);
        }
        for (size_t i = 2; i < num_words(); ++i) {
            storage_.heap_[i] = 0;
        }
    }
#endif

    NativeUInt parse_native(std::string_view str) const {
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

        NativeUInt value = 0;
        NativeUInt maximum = native_mask();
        NativeUInt base = hexadecimal ? 16 : 10;
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
            if (static_cast<NativeUInt>(digit) > maximum
                || value > (maximum - digit) / base)
            {
                throw std::out_of_range(
                    hexadecimal ? "Hexadecimal literal exceeds bit width"
                                : "Decimal literal exceeds bit width"
                );
            }
            value = value * base + digit;
        }
        if (negative) {
            value = (NativeUInt{0} - value) & maximum;
        }
        if constexpr (SignedRepresentation) {
            if (width_ != 0 && width_ != sbo_bits) {
                NativeUInt const sign = NativeUInt{1} << (width_ - 1);
                value = (value ^ sign) - sign;
            }
        }
        return value;
    }

    WordConstSpan heap_logical_cref() const { return WordConstSpan{heap_words(), width_}; }
    WordSpan heap_logical_mut() { return WordSpan{heap_words(), width_}; }
    WordConstSpan heap_physical_cref() const {
        return WordConstSpan{heap_words(), physical_width()};
    }
    WordSpan heap_physical_mut() { return WordSpan{heap_words(), physical_width()}; }

    WordConstSpan physical_cref(NativeBuffer& buffer) const {
        if (is_native()) {
            buffer[0] = width_ == 0 ? Word{0} : native_physical_word();
            return WordConstSpan{
                std::span<Word const>{buffer}.first(num_words()), physical_width()
            };
        }
        return heap_physical_cref();
    }

    WordSpan physical_mut(NativeBuffer& buffer) {
        if (is_native()) {
            buffer[0] = width_ == 0 ? Word{0} : native_physical_word();
            return WordSpan{std::span<Word>{buffer}.first(num_words()), physical_width()};
        }
        return heap_physical_mut();
    }

    void finish_output(NativeBuffer const& buffer) {
        if (is_native() && width_ != 0) {
            storage_.native_ = static_cast<NativeUInt>(buffer[0]);
        }
    }

    void canonicalize_padding_bits() {
        assert(!is_native());
        if (width_ == physical_width()) {
            return;
        }
        unsigned valid_bits = width_ % word_bits;
        Word mask = (Word{1} << valid_bits) - 1;
        Word& top = storage_.heap_[num_words() - 1];
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
                return static_cast<NativeSInt>(storage_.native_)
                     < static_cast<NativeSInt>(other.storage_.native_);
            } else {
                return storage_.native_ < other.storage_.native_;
            }
        }
        NativeBuffer a_buffer{};
        NativeBuffer b_buffer{};
        auto a = physical_cref(a_buffer);
        auto b = other.physical_cref(b_buffer);
        if constexpr (SignedRepresentation) {
            return scompare(a, b) < 0;
        } else {
            return ucompare(a, b) < 0;
        }
    }

    bool words_equal(DynInt const& other) const {
        auto a = heap_words();
        auto b = other.heap_words();
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
    return a.is_negative() ? -a : extended;
}

}  // namespace coconext::types::detail

#endif  // COCONEXT_DYN_INT_BASE_HPP

#ifndef COCONEXT_INT_BASE_HPP
#define COCONEXT_INT_BASE_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <coconext/types/bigint.hpp>
#include <coconext/types/direction.hpp>
#include <coconext/types/logic.hpp>
#include <coconext/types/range.hpp>
#include <coconext/types/resize_mode.hpp>
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

struct mixed_width;

template <size_t W>
class Bits {
  public:
    using IntType = IntTypePicker<W>::type;
    static constexpr bool is_wide = W > (supports_128B ? 128 : 64);

    constexpr Bits() = default;

    // From native ints
    template <NativeInteger IntT>
    constexpr Bits(IntT val) {
        static_assert(W > 0, "Bits<0> has no integer representation; use Bits<0>{}");
        if constexpr (is_wide) {
            load_native(mut(), val);
        } else {
            storage_ = static_cast<IntType>(val);
        }
    }

    template <typename WideWordsT>
        requires std::is_same_v<std::remove_cvref_t<WideWordsT>, WideWords<W>>
    constexpr Bits(WideWordsT&& val)
        requires(is_wide)
        : storage_(std::forward<WideWordsT>(val)) {
        clear_unused_bits(mut());
    }

    constexpr Bits(std::string_view val) {
        static_assert(W > 0, "Bits<0> has no integer representation; use Bits<0>{}");
        if constexpr (is_wide) {
            parse_into(mut(), val);
        } else {
            WideWords<W> parsed{};
            parse_into(WordSpan{parsed, W}, val);
            storage_ = static_cast<IntType>(parsed[0]);
            // This handles native ints larger than Word, e.g. __int128_t
            if constexpr (sizeof(IntType) > sizeof(Word)) {
                if constexpr (parsed.size() > 1) {
                    storage_ |= static_cast<IntType>(parsed[1]) << word_bits;
                }
            }
        }
    }

    // From an initializer list of bits
    template <typename U>
        requires std::convertible_to<U, Bit>
    constexpr Bits(std::initializer_list<U> init) {
        if (init.size() != W) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " does not match Bits width " + std::to_string(W)
            );
        }

        if constexpr (W > 0) {
            storage_ = IntType{};
            size_t bit_pos = W - 1;
            for (auto const& val : init) {
                if (static_cast<bool>(Bit(val))) {
                    set_bit(bit_pos, true);
                }
                if (bit_pos > 0) {
                    bit_pos--;
                }
            }
        }
    }

    template <bool IsConst>
    class IteratorImpl;

    class BitReference {
      private:
        Bits<W>& parent_;
        size_t index_;

        template <bool C>
        friend class IteratorImpl;

      public:
        constexpr BitReference(Bits<W>& parent, size_t index)
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
      private:
        using ParentType = std::conditional_t<IsConst, Bits<W> const, Bits<W>>;
        ParentType* parent_ = nullptr;
        size_t index_ = 0;

      public:
        using iterator_concept = std::random_access_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = Bit;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = std::conditional_t<IsConst, Bit, BitReference>;

        constexpr IteratorImpl() = default;

        constexpr IteratorImpl(ParentType* parent, size_t index)
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
            IteratorImpl tmp = *this;
            ++(*this);
            return tmp;
        }
        constexpr IteratorImpl& operator--() {
            --index_;
            return *this;
        }
        constexpr IteratorImpl operator--(int) {
            IteratorImpl tmp = *this;
            --(*this);
            return tmp;
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

        constexpr bool operator<(IteratorImpl const& other) const {
            return index_ < other.index_;
        }
        constexpr bool operator>(IteratorImpl const& other) const {
            return index_ > other.index_;
        }
        constexpr bool operator<=(IteratorImpl const& other) const {
            return index_ <= other.index_;
        }
        constexpr bool operator>=(IteratorImpl const& other) const {
            return index_ >= other.index_;
        }
    };

    constexpr auto begin() { return IteratorImpl<false>(this, 0); }

    constexpr auto begin() const { return IteratorImpl<true>(this, 0); }

    constexpr auto end() { return IteratorImpl<false>(this, W); }

    constexpr auto end() const { return IteratorImpl<true>(this, W); }

    constexpr auto rbegin() noexcept { return std::make_reverse_iterator(end()); }

    constexpr auto rbegin() const noexcept { return std::make_reverse_iterator(end()); }

    constexpr auto rend() noexcept { return std::make_reverse_iterator(begin()); }

    constexpr auto rend() const noexcept { return std::make_reverse_iterator(begin()); }

    constexpr BitReference operator[](size_t index) {
        if (index >= W) {
            throw std::out_of_range("Bit index out of bounds");
        }
        return BitReference(*this, index);
    }

    constexpr Bit operator[](size_t index) const {
        if (index >= W) {
            throw std::out_of_range("Bit index out of bounds");
        }
        return get_bit(index) ? Bit::_1 : Bit::_0;
    }

    constexpr Bits operator<<(size_t amount) const {
        static_assert(W > 0, "shift on Bits<0> is undefined; no bit positions exist");
        if constexpr (is_wide) {
            IntType result = storage_;
            shift_left(WordSpan{result, W}, amount);
            return Bits<W>(result);
        } else {
            if (amount >= W) {
                return Bits<W>{};
            }
            return Bits<W>(raw() << amount);
        }
    }

    constexpr Bits sra(size_t amount) const {
        static_assert(W > 0, "shift on Bits<0> is undefined; no bit positions exist");
        if constexpr (is_wide) {
            IntType result = storage_;
            shift_right_arith(WordSpan{result, W}, amount);
            return Bits<W>(result);
        } else {
            auto ext = this->sign_extended();
            if (amount >= W) {
                return Bits<W>(ext < 0 ? ~IntType{0} : IntType{0});
            }
            return Bits<W>(ext >> amount);
        }
    }

    constexpr Bits srl(size_t amount) const {
        static_assert(W > 0, "shift on Bits<0> is undefined; no bit positions exist");
        if constexpr (is_wide) {
            IntType result = storage_;
            shift_right_logical(WordSpan{result, W}, amount);
            return Bits<W>(result);
        } else {
            if (amount >= W) {
                return Bits<W>{};
            }
            return Bits<W>(raw() >> amount);
        }
    }

    constexpr bool operator==(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return true;
        } else if constexpr (is_wide) {
            return storage_ == other.storage_;
        } else {
            return (raw() == other.raw());
        }
    }

    constexpr bool operator!=(Bits<W> const& other) const { return !(*this == other); }

    // Bits is sign-agnostic storage, so ordering must name its interpretation:
    // u* treat the bits as unsigned, s* as two's-complement. There is
    // deliberately no operator< / operator<=>; the interpretation is the
    // caller's (matching LLVM APInt's ult/slt API).
    template <size_t Wm>
    constexpr bool ult(Bits<Wm> const& other) const {
        static_assert(
            W > 0 && Wm > 0,
            "ordering on Bits<0> is undefined; the null vector has no value"
        );
        if constexpr (is_wide && Bits<Wm>::is_wide) {
            return detail::ucompare(cref(), other.cref()) < 0;
        } else if constexpr (!is_wide && !Bits<Wm>::is_wide) {
            return raw() < other.raw();
        } else if constexpr (is_wide) {
            auto other_words = other.native_word_array();
            return detail::ucompare(cref(), WordConstSpan{other_words, Wm}) < 0;
        } else {
            auto this_words = native_word_array();
            return detail::ucompare(WordConstSpan{this_words, W}, other.cref()) < 0;
        }
    }

    template <size_t Wm>
    constexpr bool ule(Bits<Wm> const& other) const {
        return !other.ult(*this);
    }
    template <size_t Wm>
    constexpr bool ugt(Bits<Wm> const& other) const {
        return other.ult(*this);
    }
    template <size_t Wm>
    constexpr bool uge(Bits<Wm> const& other) const {
        return !ult(other);
    }

    template <size_t Wm>
    constexpr bool slt(Bits<Wm> const& other) const {
        static_assert(
            W > 0 && Wm > 0,
            "ordering on Bits<0> is undefined; the null vector has no value"
        );
        if constexpr (is_wide && Bits<Wm>::is_wide) {
            return detail::scompare(cref(), other.cref()) < 0;
        } else if constexpr (!is_wide && !Bits<Wm>::is_wide) {
            return sign_extended() < other.sign_extended();
        } else if constexpr (is_wide) {
            auto other_words = other.native_word_array();
            return detail::scompare(cref(), WordConstSpan{other_words, Wm}) < 0;
        } else {
            auto this_words = native_word_array();
            return detail::scompare(WordConstSpan{this_words, W}, other.cref()) < 0;
        }
    }

    template <size_t Wm>
    constexpr bool sle(Bits<Wm> const& other) const {
        return !other.slt(*this);
    }
    template <size_t Wm>
    constexpr bool sgt(Bits<Wm> const& other) const {
        return other.slt(*this);
    }
    template <size_t Wm>
    constexpr bool sge(Bits<Wm> const& other) const {
        return !slt(other);
    }

    constexpr Bits operator&(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            Bits<W> result(*this);
            and_assign(result.mut(), other.cref());
            return result;
        } else {
            return Bits<W>(raw() & other.raw());
        }
    }

    constexpr Bits operator|(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            Bits<W> result(*this);
            or_assign(result.mut(), other.cref());
            return result;
        } else {
            return Bits<W>(raw() | other.raw());
        }
    }

    constexpr Bits operator^(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            Bits<W> result(*this);
            xor_assign(result.mut(), other.cref());
            return result;
        } else {
            return Bits<W>(raw() ^ other.raw());
        }
    }

    constexpr Bits operator~() const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            Bits<W> result(*this);
            bitnot(result.mut());
            return result;
        } else {
            return Bits<W>(~storage_);
        }
    }

    constexpr size_t count_trailing_zeros() const {
        if constexpr (W == 0) {
            return 0;
        } else if constexpr (is_wide) {
            return detail::count_trailing_zeros(cref());
        } else {
            IntType val = raw();
            if (val == 0) {
                return W;
            }

            if constexpr (supports_128B && W > 64) {
                uint64_t lower = static_cast<uint64_t>(val);
                if (lower != 0) {
                    return std::countr_zero(lower);
                }
                return 64 + std::countr_zero(static_cast<uint64_t>(val >> 64));
            } else {
                return std::countr_zero(val);
            }
        }
    }

    constexpr size_t count_leading_zeros() const {
        if constexpr (W == 0) {
            return 0;
        } else if constexpr (is_wide) {
            return detail::count_leading_zeros(cref());
        } else {
            IntType val = raw();
            if (val == 0) {
                return W;
            }

            if constexpr (supports_128B && W > 64) {
                uint64_t upper = static_cast<uint64_t>(val >> 64);
                size_t unused_bits = 128 - W;
                if (upper != 0) {
                    return std::countl_zero(upper) - unused_bits;
                }
                return 64 + std::countl_zero(static_cast<uint64_t>(val)) - unused_bits;
            } else {
                size_t unused_bits = (sizeof(IntType) * 8) - W;
                return std::countl_zero(val) - unused_bits;
            }
        }
    }

    constexpr size_t popcount() const {
        if constexpr (W == 0) {
            return 0;
        } else if constexpr (is_wide) {
            return detail::popcount(cref());
        } else if constexpr (supports_128B && W > 64) {
            // std::popcount rejects __uint128_t; count each half.
            auto val = raw();
            return std::popcount(static_cast<uint64_t>(val))
                 + std::popcount(static_cast<uint64_t>(val >> 64));
        } else {
            return std::popcount(raw());
        }
    }

    constexpr bool get_bit(size_t index) const {
        static_assert(W > 0, "get_bit on Bits<0> is undefined; no bit positions exist");
        if constexpr (!is_wide) {
            return (raw() >> index) & 1;
        } else {
            return detail::get_bit(cref(), index);
        }
    }

    constexpr void set_bit(size_t index, bool val) {
        static_assert(W > 0, "set_bit on Bits<0> is undefined; no bit positions exist");
        if constexpr (!is_wide) {
            IntType mask = static_cast<IntType>(1) << index;
            if (val) {
                storage_ |= mask;
            } else {
                storage_ &= ~mask;
            }
        } else {
            detail::set_bit(mut(), index, val);
        }
    }

    constexpr Bits reverse() const noexcept {
        static_assert(W > 0, "Nothing to reverse for Null Vector");

        if constexpr (!is_wide) {
            constexpr size_t container_bits = sizeof(IntType) * 8;
            IntType reversed = reverse_bits_native(raw());
            return Bits(static_cast<IntType>(reversed >> (container_bits - W)));
        } else {
            Bits<W> result{};
            reverse_bits_bigint(result.mut(), cref());
            return result;
        }
    }

    // On the wide tier this is a non-owning view, not a copy: callers only ever
    // read words out of it, and copying would mean duplicating the whole array.
    using RawType = std::conditional_t<is_wide, WordConstSpan, IntType>;

    constexpr RawType raw() const {
        static_assert(W > 0, "raw() on Bits<0> is undefined; the null vector has no value");
        if constexpr (is_wide) {
            return cref();
        } else {
            return static_cast<IntType>(storage_ & top_mask);
        }
    }

    // Width-changing operations. These are the only cross-width construction
    // path; there is deliberately no cross-width converting constructor.

    template <size_t Wm>
        requires(Wm >= W)
    constexpr Bits<Wm> zero_extend() const {
        return widened<Wm>();
    }

    template <size_t Wm>
        requires(Wm >= W)
    constexpr Bits<Wm> sign_extend() const {
        if constexpr (W == 0) {
            return Bits<Wm>{};
        } else if constexpr (is_wide) {
            Bits<Wm> result{};
            detail::sign_extend(result.mut(), cref());
            return result;
        } else if constexpr (Bits<Wm>::is_wide) {
            Bits<Wm> result{};
            if constexpr (sizeof(IntType) <= sizeof(Word)) {
                load_native(result.mut(), sign_extended());
            } else {
#if defined(__SIZEOF_INT128__)
                load_int128(result.mut(), sign_extended());
#endif
            }
            return result;
        } else {
            using Dest = typename Bits<Wm>::IntType;
            return Bits<Wm>(static_cast<Dest>(sign_extended()));
        }
    }

    template <size_t Wm>
        requires(Wm <= W)
    constexpr Bits<Wm> truncate() const {
        Bits<Wm> result{};
        if constexpr (Wm == 0) {
            return result;
        } else if constexpr (is_wide && Bits<Wm>::is_wide) {
            detail::truncate(result.mut(), cref());
        } else if constexpr (is_wide) {
            using Dest = typename Bits<Wm>::IntType;
            if constexpr (sizeof(Dest) <= sizeof(Word)) {
                result = Bits<Wm>(static_cast<Dest>(storage_[0]));
            } else {
#if defined(__SIZEOF_INT128__)
                __uint128_t value = storage_[0];
                if (storage_.size() > 1) {
                    value |= static_cast<__uint128_t>(storage_[1]) << word_bits;
                }
                result = Bits<Wm>(static_cast<Dest>(value));
#endif
            }
        } else {
            result = Bits<Wm>(static_cast<typename Bits<Wm>::IntType>(raw()));
        }
        return result;
    }

    // Interpret the source as unsigned and clamp to the destination width.
    template <size_t Wm>
    constexpr Bits<Wm> saturate_unsigned() const {
        if constexpr (Wm >= W) {
            return zero_extend<Wm>();
        } else if constexpr (Wm == 0) {
            return Bits<0>{};
        } else if constexpr (is_wide) {
            if (!detail::fits_unsigned(cref(), Wm)) {
                return ~Bits<Wm>{};
            }
            return truncate<Wm>();
        } else {
            IntType target_max = (static_cast<IntType>(1) << Wm) - 1;
            if (raw() > target_max) {
                return ~Bits<Wm>{};
            }
            return truncate<Wm>();
        }
    }

    // Interpret the source as two's-complement and clamp to the destination.
    template <size_t Wm>
    constexpr Bits<Wm> saturate_signed() const {
        if constexpr (Wm >= W) {
            return sign_extend<Wm>();
        } else if constexpr (Wm == 0) {
            return Bits<0>{};
        } else if constexpr (is_wide) {
            bool negative = detail::is_negative(cref());
            if (detail::fits_signed(cref(), Wm)) {
                return truncate<Wm>();
            }
            Bits<Wm> limit{};
            limit.set_bit(Wm - 1, true);
            return negative ? limit : ~limit;
        } else {
            auto value = sign_extended();
            using SType = decltype(value);
            SType target_magnitude = static_cast<SType>(1) << (Wm - 1);
            SType target_min = -target_magnitude;
            SType target_max = target_magnitude - 1;
            if (value >= target_min && value <= target_max) {
                return truncate<Wm>();
            }
            // Clamp to signed min (100..0) or signed max (011..1).
            Bits<Wm> limit{};
            limit.set_bit(Wm - 1, true);
            return value < 0 ? limit : ~limit;
        }
    }

    constexpr size_t highest_set_index() const noexcept {
        if constexpr (!is_wide) {
            return std::bit_width(storage_) - 1;
        } else {
            return std::bit_width(storage_[0]) - 1;
        }
    }

    std::string to_binary_string() const {
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            return std::format("{:0{}b}", static_cast<wide_uint>(raw()), W);
        } else {
            return format_power_of_two(cref(), 1, W, [](uint8_t d) {
                return static_cast<char>('0' + d);
            });
        }
    }

    std::string to_binary_string(size_t decimal_pos) const {
        static_assert(W != 0, "Nothing to represent");
        if (decimal_pos == 0) {
            return to_binary_string();
        }
        std::string res;
        res.reserve(W + 1);
        auto val = raw();
        for (size_t i = W; i > 0; --i) {
            if (i == decimal_pos) {
                res.push_back('.');
            } else {
                size_t bit_idx = i - 1;
                res.push_back(
                    ((val.get_word(bit_idx / 64) >> (bit_idx % 64)) & 1) ? '1' : '0'
                );
            }
        }
        return res;
    }

    std::string to_decimal_string(bool is_signed = false) const {
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            if (is_signed) {
                constexpr size_t total_bits = sizeof(wide_int) * 8;
                wide_int signed_val =
                    static_cast<wide_int>(static_cast<wide_uint>(raw()) << (total_bits - W))
                    >> (total_bits - W);
                return std::format("{}", signed_val);
            }
            return std::format("{}", static_cast<wide_uint>(raw()));
        } else {
            return format_decimal(cref(), is_signed);
        }
    }

    std::string to_hexadecimal_string() const {
        constexpr size_t hex_chars = (W + 3) / 4;
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            return std::format("{:0{}x}", static_cast<wide_uint>(raw()), hex_chars);
        } else {
            char const hex_digits[] = "0123456789abcdef";
            return format_power_of_two(cref(), 4, hex_chars, [&](uint8_t d) {
                return hex_digits[d];
            });
        }
    }

    std::string to_octal_string() const {
        constexpr size_t octal_chars = (W + 2) / 3;
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            return std::format("{:0{}o}", static_cast<wide_uint>(raw()), octal_chars);
        } else {
            return format_power_of_two(cref(), 3, octal_chars, [](uint8_t d) {
                return static_cast<char>('0' + d);
            });
        }
    }

  private:
    template <size_t>
    friend class Bits;
    friend struct mixed_width;

  public:
    // Exact-width primitives used internally by the growing operations.
    // These wrap to W and therefore are not exposed by the numeric frontends.
    constexpr Bits operator+(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            IntType result = storage_;
            add_assign(WordSpan{result, W}, other.cref());
            return Bits<W>(result);
        } else {
            return Bits<W>(static_cast<IntType>(raw() + other.raw()));
        }
    }

    constexpr Bits operator-(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            IntType result = storage_;
            sub_assign(WordSpan{result, W}, other.cref());
            return Bits<W>(result);
        } else {
            return Bits<W>(static_cast<IntType>(raw() - other.raw()));
        }
    }

    constexpr Bits operator*(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            // multiply() needs dst disjoint from both operands.
            IntType result = storage_;
            multiply(WordSpan{result, W}, cref(), other.cref());
            return Bits<W>(result);
        } else {
            return Bits<W>(static_cast<IntType>(raw() * other.raw()));
        }
    }

    constexpr Bits udiv(Bits<W> const& other) const { return udivrem(other).first; }

    constexpr Bits sdiv(Bits<W> const& other) const { return sdivrem(other).first; }

    constexpr Bits umod(Bits<W> const& other) const { return udivrem(other).second; }

    constexpr Bits smod(Bits<W> const& other) const { return sdivrem(other).second; }

    constexpr std::pair<Bits, Bits> udivrem(Bits<W> const& other) const {
        static_assert(
            W > 0, "udivrem on Bits<0> is undefined; the null vector has no value"
        );
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_wide) {
            return divide_wide(other, false, false);
        } else {
            return {Bits<W>(raw() / other.raw()), Bits<W>(raw() % other.raw())};
        }
    }

    constexpr std::pair<Bits, Bits> sdivrem(Bits<W> const& other) const {
        static_assert(
            W > 0, "sdivrem on Bits<0> is undefined; the null vector has no value"
        );
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_wide) {
            return divide_wide(other, true, false);
        } else {
            auto lhs_ext = this->sign_extended();
            auto rhs_ext = other.sign_extended();
            using SType = decltype(lhs_ext);
            if (rhs_ext == -1 && lhs_ext == std::numeric_limits<SType>::min()) {
                return {*this, Bits<W>{}};
            }
            return {Bits<W>(lhs_ext / rhs_ext), Bits<W>(lhs_ext % rhs_ext)};
        }
    }

    constexpr std::pair<Bits, Bits> sdivmod(Bits<W> const& other) const {
        if constexpr (is_wide) {
            return divide_wide(other, true, true);
        } else {
            auto result = sdivrem(other);
            if (result.second != Bits<W>{} && (slt(Bits<W>{}) != other.slt(Bits<W>{}))) {
                result.first = result.first - Bits<W>{1};
                result.second = result.second + other;
            }
            return result;
        }
    }

    constexpr WordConstSpan cref() const
        requires(is_wide)
    {
        return WordConstSpan{std::span<Word const>{storage_}, W};
    }

    constexpr WordSpan mut()
        requires(is_wide)
    {
        return WordSpan{std::span<Word>{storage_}, W};
    }

    constexpr std::pair<Bits, Bits> divide_wide(
        Bits const& rhs, bool is_signed, bool modulo
    ) const
        requires(is_wide)
    {
        constexpr size_t max_limbs = std::tuple_size_v<IntType> * limbs_per_word;
        std::array<DivLimb, max_limbs * 2 + 1> u{};
        std::array<DivLimb, max_limbs> v{};
        std::array<DivLimb, max_limbs * 2> q{};
        std::array<DivLimb, max_limbs> r{};
        DivideScratch scratch{u, v, q, r};
        Bits quotient;
        Bits remainder;
        if (modulo) {
            Bits lhs_magnitude;
            Bits rhs_magnitude;
            detail::divide_modulo(
                quotient.mut(),
                remainder.mut(),
                cref(),
                rhs.cref(),
                lhs_magnitude.mut(),
                rhs_magnitude.mut(),
                scratch
            );
        } else if (is_signed) {
            Bits lhs_magnitude;
            Bits rhs_magnitude;
            detail::divide_signed(
                quotient.mut(),
                remainder.mut(),
                cref(),
                rhs.cref(),
                lhs_magnitude.mut(),
                rhs_magnitude.mut(),
                scratch
            );
        } else {
            detail::divide_unsigned(
                quotient.mut(), remainder.mut(), cref(), rhs.cref(), scratch
            );
        }
        return {quotient, remainder};
    }

  private:
    // Zero-fill widen to Wm. Handles all four tier-crossing combinations; the
    // native-to-native case stays a single cast so the common path is cheap.
    template <size_t Wm>
        requires(Wm >= W)
    constexpr Bits<Wm> widened() const {
        if constexpr (W == 0 || Wm == 0) {
            return Bits<Wm>{};
        } else if constexpr (!is_wide && !Bits<Wm>::is_wide) {
            return Bits<Wm>(static_cast<typename Bits<Wm>::IntType>(raw()));
        } else if constexpr (is_wide && Bits<Wm>::is_wide) {
            Bits<Wm> result{};
            detail::zero_extend(result.mut(), cref());
            return result;
        } else if constexpr (!is_wide && Bits<Wm>::is_wide) {
            Bits<Wm> result{};
            if constexpr (sizeof(IntType) <= sizeof(Word)) {
                load_native(result.mut(), raw());
            } else {
#if defined(__SIZEOF_INT128__)
                load_uint128(result.mut(), raw());
#endif
            }
            return result;
        }
    }

    constexpr auto sign_extended() const {
        static_assert(
            W > 0, "sign_extended on Bits<0> is undefined; the null vector has no value"
        );
        using SType = std::make_signed_t<IntType>;
        constexpr unsigned shift = sizeof(IntType) * 8 - W;
        return static_cast<SType>(raw() << shift) >> shift;
    }

    constexpr WideWords<W> native_word_array() const
        requires(!is_wide && W > 0)
    {
        WideWords<W> result{};
        result[0] = static_cast<Word>(raw());
#if defined(__SIZEOF_INT128__)
        if constexpr (sizeof(IntType) > sizeof(Word)) {
            result[1] = static_cast<Word>(raw() >> word_bits);
        }
#endif
        return result;
    }

    // Mask of the W valid low bits within the native storage word. Only used on
    // the native path; the wide word-array path and the zero-width path have
    // no meaningful mask.
    static constexpr IntType compute_top_mask() {
        if constexpr (W == 0 || is_wide) {
            return IntType{};
        } else if constexpr (W % (sizeof(IntType) * 8) == 0) {
            return ~static_cast<IntType>(0);
        } else {
            return (static_cast<IntType>(1) << (W % (sizeof(IntType) * 8))) - 1;
        }
    }

    static constexpr IntType top_mask = compute_top_mask();

  private:
    IntType storage_{};
};

struct mixed_width {
  private:
    template <size_t W>
    using NativeBuffer =
        std::array<Word, Bits<W>::is_wide ? 0 : (W + word_bits - 1) / word_bits>;

    template <size_t W>
    static constexpr WordConstSpan view(Bits<W> const& value, NativeBuffer<W>& buffer) {
        if constexpr (Bits<W>::is_wide) {
            return value.cref();
        } else {
            buffer = value.native_word_array();
            return WordConstSpan{buffer, W};
        }
    }

    template <size_t W>
    static constexpr WordSpan output_view(Bits<W>& value, NativeBuffer<W>& buffer) {
        if constexpr (Bits<W>::is_wide) {
            return value.mut();
        } else {
            return WordSpan{buffer, W};
        }
    }

    template <size_t W>
    static constexpr void finish_output(Bits<W>& value, NativeBuffer<W> const& buffer) {
        if constexpr (!Bits<W>::is_wide && W > 0) {
            value.storage_ = static_cast<typename Bits<W>::IntType>(buffer[0]);
#if defined(__SIZEOF_INT128__)
            if constexpr (sizeof(typename Bits<W>::IntType) > sizeof(Word)) {
                value.storage_ |= static_cast<typename Bits<W>::IntType>(buffer[1])
                               << word_bits;
            }
#endif
        }
    }

  public:
    template <size_t Wr, size_t Wa, size_t Wb>
    static constexpr Bits<Wr> add(
        Bits<Wa> const& a, Bits<Wb> const& b, bool signed_operands
    ) {
        static_assert(Bits<Wr>::is_wide);
        NativeBuffer<Wa> a_buffer{};
        NativeBuffer<Wb> b_buffer{};
        Bits<Wr> result;
        add_extended(
            result.mut(),
            view(a, a_buffer),
            view(b, b_buffer),
            signed_operands,
            signed_operands
        );
        return result;
    }

    template <size_t Wr, size_t Wa, size_t Wb>
    static constexpr Bits<Wr> sub(
        Bits<Wa> const& a, Bits<Wb> const& b, bool signed_operands
    ) {
        static_assert(Bits<Wr>::is_wide);
        NativeBuffer<Wa> a_buffer{};
        NativeBuffer<Wb> b_buffer{};
        Bits<Wr> result;
        sub_extended(
            result.mut(),
            view(a, a_buffer),
            view(b, b_buffer),
            signed_operands,
            signed_operands
        );
        return result;
    }

    template <size_t Wr, size_t Wa, size_t Wb>
    static constexpr Bits<Wr> mul(
        Bits<Wa> const& a, Bits<Wb> const& b, bool signed_operands
    ) {
        static_assert(Bits<Wr>::is_wide);
        NativeBuffer<Wa> a_buffer{};
        NativeBuffer<Wb> b_buffer{};
        Bits<Wr> result;
        if (signed_operands) {
            detail::multiply_signed(result.mut(), view(a, a_buffer), view(b, b_buffer));
        } else {
            detail::multiply_unsigned(result.mut(), view(a, a_buffer), view(b, b_buffer));
        }
        return result;
    }

    template <size_t Wa, size_t Wb>
    static constexpr std::pair<Bits<Wa + 1>, Bits<Wb>> divrem(
        Bits<Wa> const& a, Bits<Wb> const& b, bool is_signed, bool modulo
    ) {
        constexpr size_t Wm = std::max(Wa, Wb) + 1;
        constexpr size_t max_limbs = (Wm + word_bits - 1) / word_bits * limbs_per_word;
        NativeBuffer<Wa> a_buffer{};
        NativeBuffer<Wb> b_buffer{};
        NativeBuffer<Wa + 1> quotient_buffer{};
        NativeBuffer<Wb> remainder_buffer{};
        Bits<Wa + 1> quotient;
        Bits<Wb> remainder;
        std::array<DivLimb, max_limbs * 2 + 1> u{};
        std::array<DivLimb, max_limbs> v{};
        std::array<DivLimb, max_limbs * 2> q{};
        std::array<DivLimb, max_limbs> r{};
        DivideScratch scratch{u, v, q, r};
        auto quotient_view = output_view(quotient, quotient_buffer);
        auto remainder_view = output_view(remainder, remainder_buffer);
        auto a_view = view(a, a_buffer);
        auto b_view = view(b, b_buffer);
        if (is_signed) {
            Bits<Wm> lhs_magnitude;
            Bits<Wm> rhs_magnitude;
            if (modulo) {
                detail::divide_modulo(
                    quotient_view,
                    remainder_view,
                    a_view,
                    b_view,
                    lhs_magnitude.mut(),
                    rhs_magnitude.mut(),
                    scratch
                );
            } else {
                detail::divide_signed(
                    quotient_view,
                    remainder_view,
                    a_view,
                    b_view,
                    lhs_magnitude.mut(),
                    rhs_magnitude.mut(),
                    scratch
                );
            }
        } else {
            detail::divide_unsigned(quotient_view, remainder_view, a_view, b_view, scratch);
        }
        finish_output(quotient, quotient_buffer);
        finish_output(remainder, remainder_buffer);
        return {quotient, remainder};
    }
};

// Growing arithmetic. The result width is wide enough to hold every value the
// operation can produce, so these never wrap and the width arithmetic lives in
// the return type rather than at each call site. Wide operations consume each
// operand at its original width and apply virtual zero/sign extension.

template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> add_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::add<Wr>(a, b, false);
    } else {
        return a.template zero_extend<Wr>() + b.template zero_extend<Wr>();
    }
}

template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> add_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::add<Wr>(a, b, true);
    } else {
        return a.template sign_extend<Wr>() + b.template sign_extend<Wr>();
    }
}

template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> sub_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::sub<Wr>(a, b, false);
    } else {
        return a.template zero_extend<Wr>() - b.template zero_extend<Wr>();
    }
}

template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> sub_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::sub<Wr>(a, b, true);
    } else {
        return a.template sign_extend<Wr>() - b.template sign_extend<Wr>();
    }
}

template <size_t Wa, size_t Wb>
constexpr Bits<Wa + Wb> mul_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = Wa + Wb;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::mul<Wr>(a, b, false);
    } else {
        return a.template zero_extend<Wr>() * b.template zero_extend<Wr>();
    }
}

template <size_t Wa, size_t Wb>
constexpr Bits<Wa + Wb> mul_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = Wa + Wb;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::mul<Wr>(a, b, true);
    } else {
        return a.template sign_extend<Wr>() * b.template sign_extend<Wr>();
    }
}

// Quotient grows by one bit so signed_min / -1 is representable.

template <size_t Wa, size_t Wb>
constexpr std::pair<Bits<Wa + 1>, Bits<Wb>> divrem_unsigned(
    Bits<Wa> const& a, Bits<Wb> const& b
) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::divrem(a, b, false, false);
    } else {
        auto result = a.template zero_extend<Wr>().udivrem(b.template zero_extend<Wr>());
        return {
            result.first.template truncate<Wa + 1>(), result.second.template truncate<Wb>()
        };
    }
}

template <size_t Wa, size_t Wb>
constexpr std::pair<Bits<Wa + 1>, Bits<Wb>> divrem_signed(
    Bits<Wa> const& a, Bits<Wb> const& b
) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::divrem(a, b, true, false);
    } else {
        auto result = a.template sign_extend<Wr>().sdivrem(b.template sign_extend<Wr>());
        return {
            result.first.template truncate<Wa + 1>(), result.second.template truncate<Wb>()
        };
    }
}

template <size_t Wa, size_t Wb>
constexpr std::pair<Bits<Wa + 1>, Bits<Wb>> divmod_signed(
    Bits<Wa> const& a, Bits<Wb> const& b
) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    if constexpr (Bits<Wr>::is_wide) {
        return mixed_width::divrem(a, b, true, true);
    } else {
        auto result = a.template sign_extend<Wr>().sdivmod(b.template sign_extend<Wr>());
        return {
            result.first.template truncate<Wa + 1>(), result.second.template truncate<Wb>()
        };
    }
}

template <size_t Wa, size_t Wb>
constexpr Bits<Wa + 1> div_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    return divrem_unsigned(a, b).first;
}

template <size_t Wa, size_t Wb>
constexpr Bits<Wa + 1> div_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    return divrem_signed(a, b).first;
}

template <size_t Wa, size_t Wb>
constexpr Bits<Wb> rem_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    return divrem_unsigned(a, b).second;
}

// C-style remainder: the sign follows the dividend.
template <size_t Wa, size_t Wb>
constexpr Bits<Wb> rem_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    return divrem_signed(a, b).second;
}

// VHDL/Python modulo: the sign follows the divisor.
template <size_t Wa, size_t Wb>
constexpr Bits<Wb> mod_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    return divmod_signed(a, b).second;
}

template <size_t W>
constexpr Bits<W + 1> negate_signed(Bits<W> const& a) {
    return Bits<W + 1>{} - a.template sign_extend<W + 1>();
}

template <size_t W>
constexpr Bits<W + 1> abs_signed(Bits<W> const& a) {
    auto ext = a.template sign_extend<W + 1>();
    if constexpr (W == 0) {
        return ext;
    } else {
        return a.get_bit(W - 1) ? Bits<W + 1>{} - ext : ext;
    }
}

template <size_t bits>
constexpr auto max_unsigned() {
    if constexpr ((bits > 64 && !supports_128B) || (bits > 128)) {
        return ~Bits<bits>{};
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

constexpr uint64_t reverse_bits_64(uint64_t w) {
    w = ((w & 0xaaaaaaaaaaaaaaaaULL) >> 1) | ((w & 0x5555555555555555ULL) << 1);
    w = ((w & 0xccccccccccccccccULL) >> 2) | ((w & 0x3333333333333333ULL) << 2);
    w = ((w & 0xf0f0f0f0f0f0f0f0ULL) >> 4) | ((w & 0x0f0f0f0f0f0f0f0fULL) << 4);
    w = ((w & 0xff00ff00ff00ff00ULL) >> 8) | ((w & 0x00ff00ff00ff00ffULL) << 8);
    w = ((w & 0xffff0000ffff0000ULL) >> 16) | ((w & 0x0000ffff0000ffffULL) << 16);
    return (w >> 32) | (w << 32);
}

template <typename T>
constexpr T reverse_bits_native(T x) {
    if constexpr (sizeof(T) == 16) {
        uint64_t lo = reverse_bits_native(static_cast<uint64_t>(x));
        uint64_t hi = reverse_bits_native(static_cast<uint64_t>(x >> 64));
        return (static_cast<T>(lo) << 64) | static_cast<T>(hi);
    } else {
        return static_cast<T>(reverse_bits_64(static_cast<uint64_t>(x)));
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

template <auto... Args>
constexpr Range make_fixed_range() {
    static_assert(
        sizeof...(Args) >= 1 && sizeof...(Args) <= 3,
        "Ufixed/Sfixed takes 1 to 3 range args"
    );
    constexpr auto t = std::tuple{Args...};
    if constexpr (sizeof...(Args) == 1) {
        using First = std::remove_cvref_t<decltype(std::get<0>(t))>;
        static_assert(
            std::is_same_v<First, Range>, "Ufixed/Sfixed only take Range as single argument"
        );
        return std::get<0>(t);
    } else if constexpr (sizeof...(Args) == 2) {
        constexpr Range r{
            static_cast<Range::value_type>(std::get<0>(t)),
            static_cast<Range::value_type>(std::get<1>(t))
        };
        static_assert(
            r.left >= r.right, "Ufixed/Sfixed do not allow direction.right as MSB"
        );
        static_assert(r.length() >= 1, "Range cannot be negative or zero");
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
        Range r{
            static_cast<Range::value_type>(std::get<0>(t)),
            std::get<1>(t),
            static_cast<Range::value_type>(std::get<2>(t))
        };
        return r;
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
    T const& x, overflow_mode ovf, round_mode rnd
) noexcept {
    return auto_resized<T const&>(x, ovf, rnd);
}

// Prvalue (temporary) overload
template <typename T>
    requires(!std::is_lvalue_reference_v<T>)
[[nodiscard]] constexpr auto_resized<T> resize(
    T&& x, overflow_mode ovf, round_mode rnd
) noexcept {
    return auto_resized<T>(std::move(x), ovf, rnd);
}

}  // namespace detail

// Deduced resize for Signed/Unsigned, Sfixed/Ufixed
template <typename X>
    requires(
        detail::is_coconext_unsigned_v<std::remove_cvref_t<X>>
        || detail::is_coconext_signed_v<std::remove_cvref_t<X>>
        || is_fixed<std::remove_cvref_t<X>>
    )
[[nodiscard]] constexpr auto resize(
    X&& x, overflow_mode ovf = overflow_mode::wrap, round_mode rnd = round_mode::truncate
) noexcept {
    if constexpr (is_fixed<std::remove_cvref_t<X>>) {
        return detail::resize(
            std::forward<X>(x), overflow_mode::saturate, round_mode::round_to_even
        );
    } else {
        return detail::resize(std::forward<X>(x), ovf, rnd);
    }
}

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

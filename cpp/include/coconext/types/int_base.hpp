#ifndef COCONEXT_INT_BASE_HPP
#define COCONEXT_INT_BASE_HPP

#include <algorithm>
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

namespace coconext::types {

namespace detail {

struct EmptyStorage {};

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
                        std::conditional_t<(BW <= 128), __uint128_t, BigIntStorage<BW>>
#else
                        BigIntStorage<BW>
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

struct same_width;

template <size_t W>
class Bits {
  public:
    using IntType = IntTypePicker<W>::type;
    static constexpr bool is_wide = std::is_same_v<IntType, BigIntStorage<W>>;

    constexpr Bits() = default;

    // From native ints
    template <NativeInteger IntT>
    constexpr Bits(IntT val) : storage_(val) {
        static_assert(W > 0, "Bits<0> has no integer representation; use Bits<0>{}");
    }

    template <typename BigIntStorageT>
        requires std::is_same_v<std::remove_cvref_t<BigIntStorageT>, BigIntStorage<W>>
    constexpr Bits(BigIntStorageT&& val)
        requires(is_wide)
        : storage_(std::forward<BigIntStorageT>(val)) {}

    constexpr Bits(std::string_view val) {
        static_assert(W > 0, "Bits<0> has no integer representation; use Bits<0>{}");
        if constexpr (is_wide) {
            storage_ = IntType(val);
        } else {
            BigIntStorage<W> parsed(val);
            storage_ = static_cast<IntType>(parsed.get_word(0));
            if constexpr (sizeof(IntType) > sizeof(Word)) {
                if (BigIntStorage<W>::num_of_words > 1) {
                    storage_ |= static_cast<IntType>(parsed.get_word(1)) << word_bits;
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
            BigIntStorage<W> result = storage_;
            shift_left(result, amount);
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
            BigIntStorage<W> result = storage_;
            shift_right_arith(result, amount);
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
            BigIntStorage<W> result = storage_;
            shift_right_logical(result, amount);
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
    constexpr bool ult(Bits<W> const& other) const {
        static_assert(
            W > 0, "ordering on Bits<0> is undefined; the null vector has no value"
        );
        if constexpr (is_wide) {
            return storage_.ucompare(other.storage_) < 0;
        } else {
            return raw() < other.raw();
        }
    }

    constexpr bool ule(Bits<W> const& other) const { return !other.ult(*this); }
    constexpr bool ugt(Bits<W> const& other) const { return other.ult(*this); }
    constexpr bool uge(Bits<W> const& other) const { return !ult(other); }

    constexpr bool slt(Bits<W> const& other) const {
        static_assert(
            W > 0, "ordering on Bits<0> is undefined; the null vector has no value"
        );
        if constexpr (is_wide) {
            return storage_.scompare(other.storage_) < 0;
        } else {
            return sign_extended() < other.sign_extended();
        }
    }

    constexpr bool sle(Bits<W> const& other) const { return !other.slt(*this); }
    constexpr bool sgt(Bits<W> const& other) const { return other.slt(*this); }
    constexpr bool sge(Bits<W> const& other) const { return !slt(other); }

    constexpr Bits operator&(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            return Bits<W>(storage_ & other.storage_);
        } else {
            return Bits<W>(raw() & other.raw());
        }
    }

    constexpr Bits operator|(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            return Bits<W>(storage_ | other.storage_);
        } else {
            return Bits<W>(raw() | other.raw());
        }
    }

    constexpr Bits operator^(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            return Bits<W>(storage_ ^ other.storage_);
        } else {
            return Bits<W>(raw() ^ other.raw());
        }
    }

    constexpr Bits operator~() const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else {
            return Bits<W>(~storage_);
        }
    }

    constexpr size_t count_trailing_zeros() const {
        if constexpr (W == 0) {
            return 0;
        } else if constexpr (is_wide) {
            return storage_.count_trailing_zeros();
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
            return storage_.count_leading_zeros();
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
        } else if constexpr (!is_wide) {
            return std::popcount(raw());
        } else {
            return storage_.popcount();
        }
    }

    constexpr bool get_bit(size_t index) const {
        static_assert(W > 0, "get_bit on Bits<0> is undefined; no bit positions exist");
        if constexpr (!is_wide) {
            return (raw() >> index) & 1;
        } else {
            return srl(index).storage_.get_word(0) & 1;
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
            Bits<W> mask = Bits<W>(1) << index;
            if (val) {
                storage_ = (*this | mask).storage_;
            } else {
                storage_ = (*this & ~mask).storage_;
            }
        }
    }

    // On the wide tier this is a non-owning view, not a copy: callers only ever
    // read words out of it, and copying would mean duplicating the whole array.
    using RawType = std::conditional_t<is_wide, BigIntConstRef, IntType>;

    constexpr RawType raw() const {
        static_assert(W > 0, "raw() on Bits<0> is undefined; the null vector has no value");
        if constexpr (is_wide) {
            return static_cast<BigIntConstRef>(storage_);
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
        Bits<Wm> result = widened<Wm>();
        if constexpr (W > 0 && Wm > W) {
            if (get_bit(W - 1)) {
                for (size_t i = W; i < Wm; ++i) {
                    result.set_bit(i, true);
                }
            }
        }
        return result;
    }

    template <size_t Wm>
        requires(Wm <= W)
    constexpr Bits<Wm> truncate() const {
        Bits<Wm> result{};
        for (size_t i = 0; i < Wm; ++i) {
            if (get_bit(i)) {
                result.set_bit(i, true);
            }
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
        } else {
            // Anything above bit Wm-1 means the value exceeds the target's max.
            for (size_t i = Wm; i < W; ++i) {
                if (get_bit(i)) {
                    return ~Bits<Wm>{};
                }
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
        } else {
            bool negative = W > 0 && get_bit(W - 1);
            // In range iff bits [Wm-1, W) all match the sign bit.
            bool in_range = true;
            for (size_t i = Wm - 1; i < W; ++i) {
                if (get_bit(i) != negative) {
                    in_range = false;
                    break;
                }
            }
            if (in_range) {
                return truncate<Wm>();
            }
            // Clamp to signed min (100..0) or signed max (011..1).
            Bits<Wm> limit{};
            limit.set_bit(Wm - 1, true);
            return negative ? limit : ~limit;
        }
    }

    std::string to_binary_string() const {
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            return std::format("{:0{}b}", static_cast<wide_uint>(raw()), W);
        } else {
            std::string res;
            res.reserve(W);
            auto val = raw();
            for (size_t i = W; i > 0; --i) {
                size_t bit_idx = i - 1;
                res.push_back(((val.word(bit_idx / 64) >> (bit_idx % 64)) & 1) ? '1' : '0');
            }
            return res;
        }
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
            bool negative = is_signed && storage_.is_negative();
            BigIntStorage<W> mag = negative ? -storage_ : storage_;
            if (!static_cast<bool>(mag)) {
                return "0";
            }
            BigIntStorage<W> ten(uint64_t{10});
            std::string digits;
            while (static_cast<bool>(mag)) {
                BigIntStorage<W> rem = mag.umod(ten);
                digits.push_back(static_cast<char>('0' + rem.get_word(0)));
                mag = mag.udiv(ten);
            }
            if (negative) {
                digits.push_back('-');
            }
            std::reverse(digits.begin(), digits.end());
            return digits;
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
            return digits_from_bits<4, hex_chars>([&](uint8_t d) { return hex_digits[d]; });
        }
    }

    std::string to_octal_string() const {
        constexpr size_t octal_chars = (W + 2) / 3;
        if constexpr (W == 0) {
            return "";
        } else if constexpr (!is_wide) {
            return std::format("{:0{}o}", static_cast<wide_uint>(raw()), octal_chars);
        } else {
            return digits_from_bits<3, octal_chars>([](uint8_t d) {
                return static_cast<char>('0' + d);
            });
        }
    }

  private:
    template <size_t>
    friend class Bits;
    friend struct same_width;

    // Same-width, wrapping arithmetic. Reached only through `same_width`, by
    // the growing free functions below and by the tests that cover the
    // division kernels directly. Nothing else should wrap silently.
    constexpr Bits operator+(Bits<W> const& other) const {
        if constexpr (W == 0) {
            return Bits<W>{};
        } else if constexpr (is_wide) {
            IntType result = storage_;
            add_assign(result, other.storage_);
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
            sub_assign(result, other.storage_);
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
            multiply(result, storage_, other.storage_);
            return Bits<W>(result);
        } else {
            return Bits<W>(static_cast<IntType>(raw() * other.raw()));
        }
    }

    constexpr Bits udiv(Bits<W> const& other) const {
        static_assert(W > 0, "udiv on Bits<0> is undefined; the null vector has no value");
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_wide) {
            return Bits<W>(storage_.udiv(other.storage_));
        } else {
            return Bits<W>(this->raw() / other.raw());
        }
    }

    constexpr Bits sdiv(Bits<W> const& other) const {
        static_assert(W > 0, "sdiv on Bits<0> is undefined; the null vector has no value");
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_wide) {
            return Bits<W>(storage_.sdiv(other.storage_));
        } else {
            auto lhs_ext = this->sign_extended();
            auto rhs_ext = other.sign_extended();
            // Guard the sole two's-complement overflow: MIN / -1 == MIN.
            using SType = decltype(lhs_ext);
            if (rhs_ext == -1 && lhs_ext == std::numeric_limits<SType>::min()) {
                return *this;
            }
            return Bits<W>(lhs_ext / rhs_ext);
        }
    }

    constexpr Bits umod(Bits<W> const& other) const {
        static_assert(W > 0, "umod on Bits<0> is undefined; the null vector has no value");
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_wide) {
            return Bits<W>(storage_.umod(other.storage_));
        } else {
            return Bits<W>(this->raw() % other.raw());
        }
    }

    constexpr Bits smod(Bits<W> const& other) const {
        static_assert(W > 0, "smod on Bits<0> is undefined; the null vector has no value");
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_wide) {
            return Bits<W>(storage_.smod(other.storage_));
        } else {
            auto lhs_ext = this->sign_extended();
            auto rhs_ext = other.sign_extended();
            // MIN % -1 == 0; avoid the divide's overflow on that input.
            using SType = decltype(lhs_ext);
            if (rhs_ext == -1 && lhs_ext == std::numeric_limits<SType>::min()) {
                return Bits<W>{};
            }
            return Bits<W>(lhs_ext % rhs_ext);
        }
    }

    IntType storage_{};

    // Zero-fill widen to Wm. Handles all four tier-crossing combinations; the
    // native-to-native case stays a single cast so the common path is cheap.
    template <size_t Wm>
        requires(Wm >= W)
    constexpr Bits<Wm> widened() const {
        if constexpr (W == 0 || Wm == 0) {
            return Bits<Wm>{};
        } else if constexpr (!is_wide && !Bits<Wm>::is_wide) {
            return Bits<Wm>(static_cast<typename Bits<Wm>::IntType>(raw()));
        } else {
            Bits<Wm> result{};
            for (size_t i = 0; i < W; ++i) {
                if (get_bit(i)) {
                    result.set_bit(i, true);
                }
            }
            return result;
        }
    }

    // Mask of the W valid low bits within the native storage word. Only used on
    // the native path; the wide (BigInt) path and the zero-width path have
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

    // Format the wide (BigInt) storage into `num_chars` characters of
    // `bits_per_digit` bits each, most-significant digit first. `digit_to_char`
    // maps a 0..(2^bits_per_digit - 1) digit to its printable form. Shared by
    // to_hexadecimal_string (4) and to_octal_string (3).
    template <size_t bits_per_digit, size_t num_chars, typename DigitToChar>
    std::string digits_from_bits(DigitToChar digit_to_char) const
        requires(is_wide)
    {
        std::string res;
        res.reserve(num_chars);
        auto val = raw();
        for (size_t i = num_chars; i > 0; --i) {
            uint8_t d = 0;
            for (int j = bits_per_digit - 1; j >= 0; --j) {
                size_t bit_idx = (i - 1) * bits_per_digit + j;
                if (bit_idx < W) {
                    d = (d << 1) | ((val.word(bit_idx / 64) >> (bit_idx % 64)) & 1);
                }
            }
            res.push_back(digit_to_char(d));
        }
        return res;
    }

    constexpr auto sign_extended() const {
        static_assert(
            W > 0, "sign_extended on Bits<0> is undefined; the null vector has no value"
        );
        using SType = std::make_signed_t<IntType>;
        constexpr unsigned shift = sizeof(IntType) * 8 - W;
        return static_cast<SType>(raw() << shift) >> shift;
    }
};

// The same-width primitive layer. Wrapping arithmetic is a footgun on a type
// whose whole point is exact widths, so it is not part of Bits' interface;
// the growing free functions below reach it through here, and so do the tests
// that drive the division kernels' edge cases (MIN / -1, divisor > dividend,
// division by zero) at a single width.
struct same_width {
    template <size_t W>
    static constexpr Bits<W> add(Bits<W> const& a, Bits<W> const& b) {
        return a + b;
    }
    template <size_t W>
    static constexpr Bits<W> sub(Bits<W> const& a, Bits<W> const& b) {
        return a - b;
    }
    template <size_t W>
    static constexpr Bits<W> mul(Bits<W> const& a, Bits<W> const& b) {
        return a * b;
    }
    template <size_t W>
    static constexpr Bits<W> udiv(Bits<W> const& a, Bits<W> const& b) {
        return a.udiv(b);
    }
    template <size_t W>
    static constexpr Bits<W> umod(Bits<W> const& a, Bits<W> const& b) {
        return a.umod(b);
    }
    template <size_t W>
    static constexpr Bits<W> sdiv(Bits<W> const& a, Bits<W> const& b) {
        return a.sdiv(b);
    }
    template <size_t W>
    static constexpr Bits<W> smod(Bits<W> const& a, Bits<W> const& b) {
        return a.smod(b);
    }
};

// Growing arithmetic. The result width is wide enough to hold every value the
// operation can produce, so these never wrap and the width arithmetic lives in
// the return type rather than at each call site. Operands are extended to the
// result width first -- zero-extended for the unsigned forms, sign-extended
// for the signed ones -- which is the only place the interpretation matters.

template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> add_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    return same_width::add(a.template zero_extend<Wr>(), b.template zero_extend<Wr>());
}

template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> add_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    return same_width::add(a.template sign_extend<Wr>(), b.template sign_extend<Wr>());
}

template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> sub_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    return same_width::sub(a.template zero_extend<Wr>(), b.template zero_extend<Wr>());
}

template <size_t Wa, size_t Wb>
constexpr Bits<std::max(Wa, Wb) + 1> sub_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    return same_width::sub(a.template sign_extend<Wr>(), b.template sign_extend<Wr>());
}

template <size_t Wa, size_t Wb>
constexpr Bits<Wa + Wb> mul_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = Wa + Wb;
    return same_width::mul(a.template zero_extend<Wr>(), b.template zero_extend<Wr>());
}

template <size_t Wa, size_t Wb>
constexpr Bits<Wa + Wb> mul_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = Wa + Wb;
    return same_width::mul(a.template sign_extend<Wr>(), b.template sign_extend<Wr>());
}

// Quotient grows by one bit so signed_min / -1 is representable.

template <size_t Wa, size_t Wb>
constexpr Bits<Wa + 1> div_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    return same_width::udiv(a.template zero_extend<Wr>(), b.template zero_extend<Wr>())
        .template truncate<Wa + 1>();
}

template <size_t Wa, size_t Wb>
constexpr Bits<Wa + 1> div_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    return same_width::sdiv(a.template sign_extend<Wr>(), b.template sign_extend<Wr>())
        .template truncate<Wa + 1>();
}

// Remainder is bounded by the divisor, so it needs no more than Wb bits.

template <size_t Wa, size_t Wb>
constexpr Bits<Wb> rem_unsigned(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb);
    return same_width::umod(a.template zero_extend<Wr>(), b.template zero_extend<Wr>())
        .template truncate<Wb>();
}

// C-style remainder: the sign follows the dividend.
template <size_t Wa, size_t Wb>
constexpr Bits<Wb> rem_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    return same_width::smod(a.template sign_extend<Wr>(), b.template sign_extend<Wr>())
        .template truncate<Wb>();
}

// VHDL/Python modulo: the sign follows the divisor.
template <size_t Wa, size_t Wb>
constexpr Bits<Wb> mod_signed(Bits<Wa> const& a, Bits<Wb> const& b) {
    constexpr size_t Wr = std::max(Wa, Wb) + 1;
    auto ae = a.template sign_extend<Wr>();
    auto be = b.template sign_extend<Wr>();
    auto r = same_width::smod(ae, be);
    // smod follows the dividend; when the signs disagree and there is a
    // remainder, shift it onto the divisor's side.
    if (r != Bits<Wr>{} && (r.slt(Bits<Wr>{}) != be.slt(Bits<Wr>{}))) {
        r = same_width::add(r, be);
    }
    return r.template truncate<Wb>();
}

template <size_t W>
constexpr Bits<W + 1> negate_signed(Bits<W> const& a) {
    return same_width::sub(Bits<W + 1>{}, a.template sign_extend<W + 1>());
}

template <size_t W>
constexpr Bits<W + 1> abs_signed(Bits<W> const& a) {
    auto ext = a.template sign_extend<W + 1>();
    if constexpr (W == 0) {
        return ext;
    } else {
        return a.get_bit(W - 1) ? same_width::sub(Bits<W + 1>{}, ext) : ext;
    }
}

template <size_t bits>
constexpr auto max_unsigned() {
    if constexpr ((bits > 64 && !supports_128B) || (bits > 128)) {
        return ~BigIntStorage<bits>{};
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

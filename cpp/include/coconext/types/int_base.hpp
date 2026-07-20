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
                        std::conditional_t<(BW <= 128), __uint128_t, BigInt<BW>>
#else
                        BigInt<BW>
#endif
                        >>>>>;
};

#if defined(__SIZEOF_INT128__)
static constexpr bool supports_128B = true;
#else
static constexpr bool supports_128B = false;
#endif

template <size_t W>
class Bits {
  public:
    using IntType = IntTypePicker<W>::type;
    static constexpr bool is_not_native_int = std::is_same_v<IntType, BigInt<W>>;

    constexpr Bits() = default;

    // From native ints
    template <NativeInteger IntT>
    constexpr Bits(IntT val) : storage_(val) {}

    // BigInt from a BigInt
    template <typename BigIntT>
        requires std::is_same_v<std::remove_cvref_t<BigIntT>, BigInt<W>>
    constexpr Bits(BigIntT&& val)
        requires(is_not_native_int)
        : storage_(std::forward<BigIntT>(val)) {}

    // BigInt from a string
    constexpr Bits(std::string_view val)
        requires(is_not_native_int)
        : storage_(val) {}

    // BigInt or native int from initializer list
    template <typename U>
        requires std::convertible_to<U, Bit>
    constexpr Bits(std::initializer_list<U> init) : storage_(0) {
        if (init.size() != W) {
            throw std::invalid_argument(
                "Initializer list of size " + std::to_string(init.size())
                + " does not match Bits width " + std::to_string(W)
            );
        }

        size_t bit_pos = W > 0 ? W - 1 : 0;

        for (auto const& val : init) {
            if (static_cast<bool>(Bit(val))) {
                set_bit(bit_pos, true);
            }
            if (bit_pos > 0) {
                bit_pos--;
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
            return parent_.get_bit(index_) ? Bit('1') : Bit('0');
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
                return parent_->get_bit(bit_pos) ? '1'_b : '0'_b;
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
        return get_bit(index) ? '1'_b : '0'_b;
    }

    constexpr Bits operator+(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return Bits<W>(storage_ + other.storage_);
        } else {
            return Bits<W>(storage_ + other.storage_);
        }
    }

    constexpr Bits operator-(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return Bits<W>(storage_ - other.storage_);
        } else {
            return Bits<W>(storage_ - other.storage_);
        }
    }

    constexpr Bits operator*(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return Bits<W>(storage_ * other.storage_);
        } else {
            return Bits<W>(storage_ * other.storage_);
        }
    }

    constexpr Bits udiv(Bits<W> const& other) const {
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_not_native_int) {
            return Bits<W>(storage_.udiv(other.storage_));
        } else {
            return Bits<W>(this->raw() / other.raw());
        }
    }

    constexpr Bits sdiv(Bits<W> const& other) const {
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_not_native_int) {
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
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_not_native_int) {
            return Bits<W>(storage_.umod(other.storage_));
        } else {
            return Bits<W>(this->raw() % other.raw());
        }
    }

    constexpr Bits smod(Bits<W> const& other) const {
        if (other == Bits<W>{}) {
            throw std::domain_error("Division by zero");
        }
        if constexpr (is_not_native_int) {
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

    constexpr Bits operator<<(size_t amount) const {
        if constexpr (is_not_native_int) {
            BigInt<W> result = storage_;
            shift_left(result, amount);
            return Bits<W>(result);
        } else {
            return Bits<W>(raw() << amount);
        }
    }

    constexpr Bits sra(size_t amount) const {
        if constexpr (is_not_native_int) {
            BigInt<W> result = storage_;
            shift_right_arith(result, amount);
            return Bits<W>(result);
        } else {
            auto ext = this->sign_extended();
            return Bits<W>(ext >> amount);
        }
    }

    constexpr Bits srl(size_t amount) const {
        if constexpr (is_not_native_int) {
            BigInt<W> result = storage_;
            shift_right_logical(result, amount);
            return Bits<W>(result);
        } else {
            return Bits<W>(raw() >> amount);
        }
    }

    constexpr bool operator==(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return storage_ == other.storage_;
        }
        return (raw() == other.raw());
    }

    constexpr bool operator!=(Bits<W> const& other) const { return !(*this == other); }

    // Bits is sign-agnostic storage, so ordering must name its interpretation:
    // u* treat the bits as unsigned, s* as two's-complement. There is
    // deliberately no operator< / operator<=>; the interpretation is the
    // caller's (matching LLVM APInt's ult/slt API).
    constexpr bool ult(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return storage_.ucompare(other.storage_) < 0;
        } else {
            return raw() < other.raw();
        }
    }

    constexpr bool ule(Bits<W> const& other) const { return !other.ult(*this); }
    constexpr bool ugt(Bits<W> const& other) const { return other.ult(*this); }
    constexpr bool uge(Bits<W> const& other) const { return !ult(other); }

    constexpr bool slt(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return storage_.scompare(other.storage_) < 0;
        } else {
            return sign_extended() < other.sign_extended();
        }
    }

    constexpr bool sle(Bits<W> const& other) const { return !other.slt(*this); }
    constexpr bool sgt(Bits<W> const& other) const { return other.slt(*this); }
    constexpr bool sge(Bits<W> const& other) const { return !slt(other); }

    constexpr Bits operator&(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return Bits<W>(storage_ & other.storage_);
        }
        return Bits<W>(raw() & other.raw());
    }

    constexpr Bits operator|(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return Bits<W>(storage_ | other.storage_);
        }
        return Bits<W>(raw() | other.raw());
    }

    constexpr Bits operator^(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return Bits<W>(storage_ ^ other.storage_);
        }
        return Bits<W>(raw() ^ other.raw());
    }

    constexpr Bits operator~() const { return Bits<W>(~storage_); }

    constexpr size_t count_trailing_zeros() const {
        if constexpr (is_not_native_int) {
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
        if constexpr (is_not_native_int) {
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
        if constexpr (!is_not_native_int) {
            return std::popcount(raw());
        } else {
            return storage_.popcount();
        }
    }

    constexpr bool get_bit(size_t index) const {
        if constexpr (!is_not_native_int) {
            return (raw() >> index) & 1;
        } else {
            return srl(index).storage_.get_word(0) & 1;
        }
    }

    constexpr void set_bit(size_t index, bool val) {
        if constexpr (!is_not_native_int) {
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

    constexpr IntType raw() const {
        if constexpr (is_not_native_int) {
            return storage_;
        } else {
            return storage_ & topMask;
        }
    }

    std::string to_binary_string() const {
        if constexpr (W == 0) {
            return "0";
        } else if constexpr (!is_not_native_int) {
            using cast_type = std::conditional_t<supports_128B, __uint128_t, uint64_t>;
            return std::format("{:0{}b}", static_cast<cast_type>(raw()), W);
        } else {
            std::string res;
            res.reserve(W);
            auto val = raw();
            for (size_t i = W; i > 0; --i) {
                size_t bit_idx = i - 1;
                res.push_back(
                    ((val.get_word(bit_idx / 64) >> (bit_idx % 64)) & 1) ? '1' : '0'
                );
            }
            return res;
        }
    }

    std::string to_decimal_string(bool is_signed = false) const {
        if constexpr (W == 0) {
            return "0";
        } else if constexpr (!is_not_native_int) {
            using uint_cast_type = std::conditional_t<supports_128B, __uint128_t, uint64_t>;
            using int_cast_type = std::conditional_t<supports_128B, __int128_t, int64_t>;
            if (is_signed) {
                constexpr size_t total_bits = sizeof(int_cast_type) * 8;
                int_cast_type signed_val =
                    static_cast<int_cast_type>(
                        static_cast<uint_cast_type>(raw()) << (total_bits - W)
                    )
                    >> (total_bits - W);
                return std::format("{}", signed_val);
            }
            return std::format("{}", static_cast<uint_cast_type>(raw()));
        } else {
            bool negative = is_signed && storage_.is_negative();
            BigInt<W> mag = negative ? -storage_ : storage_;
            if (!static_cast<bool>(mag)) {
                return "0";
            }
            BigInt<W> ten(uint64_t{10});
            std::string digits;
            while (static_cast<bool>(mag)) {
                BigInt<W> rem = mag.umod(ten);
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
            return "0";
        } else if constexpr (!is_not_native_int) {
            using cast_type = std::conditional_t<supports_128B, __uint128_t, uint64_t>;
            return std::format("{:0{}x}", static_cast<cast_type>(raw()), hex_chars);
        } else {
            std::string res;
            res.reserve(hex_chars);
            auto val = raw();
            char const hex_digits[] = "0123456789abcdef";

            for (size_t i = hex_chars; i > 0; --i) {
                uint8_t nibble = 0;
                for (int j = 3; j >= 0; --j) {
                    size_t bit_idx = (i - 1) * 4 + j;
                    if (bit_idx < W) {
                        nibble = (nibble << 1)
                               | ((val.get_word(bit_idx / 64) >> (bit_idx % 64)) & 1);
                    }
                }
                res.push_back(hex_digits[nibble]);
            }
            return res;
        }
    }

    std::string to_octal_string() const {
        constexpr size_t octal_chars = (W + 2) / 3;
        if constexpr (W == 0) {
            return "0";
        } else if constexpr (!is_not_native_int) {
            using cast_type = std::conditional_t<supports_128B, __uint128_t, uint64_t>;
            return std::format("{:0{}o}", static_cast<cast_type>(raw()), octal_chars);
        } else {
            std::string res;
            res.reserve(octal_chars);
            auto val = raw();

            for (size_t i = octal_chars; i > 0; --i) {
                uint8_t oct = 0;
                for (int j = 2; j >= 0; --j) {
                    size_t bit_idx = (i - 1) * 3 + j;
                    if (bit_idx < W) {
                        oct = (oct << 1)
                            | ((val.get_word(bit_idx / 64) >> (bit_idx % 64)) & 1);
                    }
                }
                res.push_back(static_cast<char>('0' + oct));
            }
            return res;
        }
    }

  private:
    IntType storage_;

    // Mask of the W valid low bits within the native storage word. Only used on
    // the native path; the wide (BigInt) path masks its own top word.
    static constexpr IntType compute_top_mask() {
        if constexpr (is_not_native_int) {
            return IntType{};
        } else if constexpr (W % (sizeof(IntType) * 8) == 0) {
            return ~static_cast<IntType>(0);
        } else {
            return (static_cast<IntType>(1) << (W % (sizeof(IntType) * 8))) - 1;
        }
    }

    static constexpr IntType topMask = compute_top_mask();

    constexpr auto sign_extended() const {
        using SType = std::make_signed_t<IntType>;
        constexpr unsigned shift = sizeof(IntType) * 8 - W;
        return static_cast<SType>(raw() << shift) >> shift;
    }
};

template <size_t bits>
constexpr auto max_unsigned() {
    if constexpr ((bits > 64 && !supports_128B) || (bits > 128)) {
        return BigInt<bits>(-1, true);
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

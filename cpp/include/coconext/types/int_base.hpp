#ifndef COCONEXT_INT_BASE_HPP
#define COCONEXT_INT_BASE_HPP

#include <algorithm>
#include <array>
#include <coconext/types/direction.hpp>
#include <coconext/types/range.hpp>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#ifdef COCONEXT_USE_APINT
#include <llvm/ADT/APInt.h>
#endif

namespace coconext::types {

namespace detail {

#ifdef COCONEXT_USE_APINT

static constexpr bool using_APInt = true;

template <size_t BitWidth>
class BigInt {
    // TODO
};

#else

static constexpr bool using_APInt = false;

// This class can handle arbitrary precision integer's basic essential
// arithmetic E.g. shifting, bitwise operations, equality e.t.c. For complete arithmetic
// operations, we fall back to APInt based BigInt
template <size_t BitWidth>
class BigInt {
  public:
    using WordType = uint64_t;
    static constexpr unsigned word_width = 64;
    static constexpr unsigned num_of_words = (BitWidth + word_width - 1) / word_width;

  private:
    static constexpr WordType get_last_word_mask() {
        uint8_t valid_bits = BitWidth % word_width;
        if (valid_bits == 0) {
            return ~WordType(0);
        }
        return (WordType(1) << valid_bits) - 1;
    }

    static constexpr WordType last_word_mask = get_last_word_mask();
    std::array<WordType, num_of_words> data{};

    bool isNegative() const {
        unsigned sign_bit = (BitWidth - 1) % 64;
        return (data.back() >> sign_bit) & 1;
    }

  public:
    WordType get_word(size_t index) const { return data[index]; }
    std::array<WordType, num_of_words> get_data() const { return data; }

    BigInt() = default;

    constexpr BigInt(WordType val, bool is_signed = false) {
        data[0] = val;

        if (is_signed && (static_cast<int64_t>(val) < 0)) {
            std::fill(data.begin() + 1, data.end(), ~WordType(0));
        }

        data.back() &= last_word_mask;
    }

    explicit constexpr BigInt(std::string_view str) {
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
        if (i + 1 < str.length() && str[i] == '0'
            && (str[i + 1] == 'x' || str[i + 1] == 'X'))
        {
            is_hex = true;
            i += 2;
        }

        if (is_hex) {
            if (is_neg) {
                throw std::invalid_argument("Hexadecimal value cannot be negative");
            }
            unsigned word_idx = 0;
            unsigned bit_shift = 0;

            for (int j = str.length() - 1; j >= static_cast<int>(i); --j) {
                if (word_idx >= num_of_words) {
                    break;
                }

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

                data[word_idx] |= (val << bit_shift);
                bit_shift += 4;

                if (bit_shift == 64) {
                    bit_shift = 0;
                    word_idx++;
                }
            }
        } else {
            for (; i < str.length(); ++i) {
                char c = str[i];
                if (c < '0' || c > '9') {
                    throw std::invalid_argument("Invalid base-10 character");
                }
                uint64_t digit = c - '0';

                uint64_t carry = digit;
                for (unsigned w = 0; w < num_of_words; ++w) {
                    uint64_t lower = (data[w] & 0xFFFFFFFF) * 10 + carry;
                    uint64_t upper = (data[w] >> 32) * 10 + (lower >> 32);

                    data[w] = (lower & 0xFFFFFFFF) | (upper << 32);
                    carry = upper >> 32;
                }
            }
        }

        if (is_neg) {
            uint64_t carry = 1;
            for (unsigned w = 0; w < num_of_words; ++w) {
                data[w] = ~data[w];
                uint64_t sum = data[w] + carry;
                carry = (sum < data[w]) ? 1 : 0;
                data[w] = sum;
            }
        }
        data.back() &= last_word_mask;
    }

    BigInt& operator=(BigInt const&) = default;
    BigInt& operator=(BigInt&&) noexcept = default;
    BigInt(BigInt const&) = default;
    BigInt(BigInt&&) noexcept = default;

    bool operator==(BigInt const& rhs) const { return data == rhs.data; }

    bool operator!=(BigInt const& rhs) const { return !(*this == rhs); }

    bool operator<(BigInt const& rhs) const {
        bool lhs_neg = isNegative();
        bool rhs_neg = rhs.isNegative();

        if (lhs_neg != rhs_neg) {
            return lhs_neg;
        }

        for (int i = num_of_words - 1; i >= 0; --i) {
            if (data[i] != rhs.data[i]) {
                return data[i] < rhs.data[i];
            }
        }

        return false;
    }

    bool operator>(BigInt const& rhs) const { return rhs < *this; }
    bool operator<=(BigInt const& rhs) const { return !(rhs < *this); }
    bool operator>=(BigInt const& rhs) const { return !(*this < rhs); }

    BigInt operator&(BigInt const& rhs) const {
        BigInt result;

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] & rhs.data[i];
        }

        return result;
    }

    BigInt operator|(BigInt const& rhs) const {
        BigInt result;

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] | rhs.data[i];
        }

        return result;
    }

    BigInt operator^(BigInt const& rhs) const {
        BigInt result;

        for (unsigned i = 0; i < num_of_words; ++i) {
            result.data[i] = data[i] ^ rhs.data[i];
        }

        return result;
    }

    BigInt operator~() const {
        BigInt result(*this);

        for (auto& word : result.data) {
            word = ~word;
        }

        result.data.back() &= last_word_mask;
        return result;
    }

    template <size_t BW>
    friend void shift_right_logical(BigInt<BW>& val, size_t amount);

    template <size_t BW>
    friend void shift_right_arith(BigInt<BW>& val, size_t amount);

    template <size_t BW>
    friend void shift_left(BigInt<BW>& val, size_t amount);
};

template <size_t BitWidth>
inline void shift_right_logical(BigInt<BitWidth>& val, size_t amount) {
    if (amount == 0) {
        return;
    }

    if (amount >= BitWidth) {
        std::fill(val.data.begin(), val.data.end(), 0);
        return;
    }

    size_t const word_shift = amount / 64;
    size_t const bit_shift = amount % 64;
    size_t const size = val.num_of_words;

    if (word_shift > 0) {
        std::rotate(val.data.begin(), val.data.begin() + word_shift, val.data.end());
        std::fill(val.data.end() - word_shift, val.data.end(), 0);
    }

    if (bit_shift > 0) {
        size_t const active_words_end = size - word_shift;
        for (size_t i = 0; i < active_words_end - 1; ++i) {
            val.data[i] =
                (val.data[i] >> bit_shift) | (val.data[i + 1] << (64 - bit_shift));
        }
        val.data[active_words_end - 1] >>= bit_shift;
    }
}

template <size_t BitWidth>
inline void shift_right_arith(BigInt<BitWidth>& val, size_t amount) {
    if (amount == 0) {
        return;
    }

    bool isNegative = val.isNegative();
    shift_right_logical(val, amount);

    if (!isNegative) {
        return;
    }

    size_t bits_to_set = (amount < BitWidth) ? amount : BitWidth;
    if (bits_to_set > 0) {
        size_t start_bit = BitWidth - bits_to_set;
        size_t end_bit = BitWidth - 1;

        size_t start_word = start_bit / 64;
        size_t end_word = end_bit / 64;

        size_t top_bits = (end_bit % 64) + 1;
        uint64_t top_mask =
            (top_bits == 64) ? ~uint64_t(0) : ((uint64_t(1) << top_bits) - 1);

        if (start_word == end_word) {
            val.data[start_word] |= top_mask & (~uint64_t(0) << (start_bit % 64));
        } else {
            val.data[end_word] |= top_mask;

            for (size_t w = start_word + 1; w < end_word; ++w) {
                val.data[w] |= ~uint64_t(0);
            }

            val.data[start_word] |= (~uint64_t(0) << (start_bit % 64));
        }
    }
}

template <size_t BitWidth>
inline void shift_left(BigInt<BitWidth>& val, size_t amount) {
    if (amount == 0) {
        return;
    }

    if (amount >= BitWidth) {
        std::fill(val.data.begin(), val.data.end(), 0);
        return;
    }

    size_t const word_shift = amount / 64;
    size_t const bit_shift = amount % 64;
    size_t const size = val.num_of_words;

    if (word_shift > 0) {
        std::rotate(val.data.rbegin(), val.data.rbegin() + word_shift, val.data.rend());
        std::fill(val.data.begin(), val.data.begin() + word_shift, 0);
    }

    if (bit_shift > 0) {
        for (size_t i = size - 1; i > word_shift; --i) {
            val.data[i] =
                (val.data[i] << bit_shift) | (val.data[i - 1] >> (64 - bit_shift));
        }

        val.data[word_shift] <<= bit_shift;
    }

    val.data.back() &= val.last_word_mask;
}

#endif  // COCONEXT_USE_APINT

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
    static constexpr bool supports_overloaded_op = !is_not_native_int || using_APInt;

    constexpr Bits() = default;

    // native ints
    constexpr Bits(IntType val)
        requires(!is_not_native_int)
        : storage_(std::move(val)) {}

    // BigInt from an BigInt
    constexpr Bits(BigInt<W> val)
        requires is_not_native_int
        : storage_(std::move(val)) {}

    // BigInt from a native uint64_t
    constexpr Bits(uint64_t val, bool is_signed = false)
        requires is_not_native_int
        : storage_(std::move(val), is_signed) {}

    // BigInt from a string
    constexpr Bits(std::string_view val)
        requires is_not_native_int
        : storage_(val) {}

    template <size_t bits>
    constexpr auto max_unsigned_native() {
        static_assert(!is_not_native_int, "Not a native integer");

        if constexpr (bits > 64 && bits <= 128) {
            static_assert(supports_128B, "128 is not a native integer width");
            if constexpr (bits == 128) {
                return ~__uint128_t{0};
            } else {
                return ((__uint128_t)1 << bits) - 1;
            }
        } else if constexpr (bits == 64) {
            return ~uint64_t{0};
        } else if constexpr (bits < 64) {
            return (uint64_t{1} << bits) - 1;
        } else {
            static_assert(bits <= 128, "Not a native integer width");
        }
    }

    template <size_t bits>
    constexpr auto max_unsigned_bigInt() {
        static_assert(is_not_native_int, "Not a BigInt");
        return BigInt<bits>(-1, true);
    }

    constexpr Bits operator+(Bits<W> const& other) const {
        static_assert(
            supports_overloaded_op,
            "operator(+) only supported by native ints and APInt BigInt"
        );
        return Bits<W>(static_cast<IntType>(storage_ + other.storage_));
    }

    constexpr Bits operator-(Bits<W> const& other) const {
        static_assert(
            supports_overloaded_op,
            "operator(-) only supported by native ints and APInt BigInt"
        );
        return Bits<W>(static_cast<IntType>(storage_ - other.storage_));
    }

    constexpr Bits operator*(Bits<W> const& other) const {
        static_assert(
            supports_overloaded_op,
            "operator(*) only supported by native ints and APInt BigInt"
        );
        return Bits<W>(static_cast<IntType>(storage_ * other.storage_));
    }

    constexpr Bits udiv(Bits<W> const& other) const {
        static_assert(
            supports_overloaded_op,
            "Division only supported by native ints and APInt BigInt"
        );
        if (other.raw() == 0) {
            throw std::domain_error("Division by zero");
        }

        if constexpr (using_APInt) {
            // TODO
        } else {
            return Bits<W>(static_cast<IntType>(this->raw() / other.raw()));
        }
    }

    constexpr Bits sdiv(Bits<W> const& other) const {
        static_assert(
            supports_overloaded_op,
            "Division only supported by native ints and APInt BigInt"
        );
        if (other.raw() == 0) {
            throw std::domain_error("Division by zero");
        }

        if constexpr (using_APInt) {
            // TODO
        } else {
            auto lhs_ext = this->sign_extended();
            auto rhs_ext = other.sign_extended();
            return Bits<W>(static_cast<IntType>(lhs_ext / rhs_ext));
        }
    }

    constexpr Bits umod(Bits<W> const& other) const {
        static_assert(
            supports_overloaded_op, "Mod only supported by native ints and APInt BigInt"
        );
        if (other.raw() == 0) {
            throw std::domain_error("Division by zero");
        }

        if constexpr (using_APInt) {
            // TODO
        } else {
            return Bits<W>(static_cast<IntType>(this->raw() % other.raw()));
        }
    }

    constexpr Bits smod(Bits<W> const& other) const {
        static_assert(
            supports_overloaded_op, "Mod only supported by native ints and APInt BigInt"
        );
        if (other.raw() == 0) {
            throw std::domain_error("Division by zero");
        }

        if constexpr (using_APInt) {
            // TODO
        } else {
            auto lhs_ext = this->sign_extended();
            auto rhs_ext = other.sign_extended();
            return Bits<W>(static_cast<IntType>(lhs_ext % rhs_ext));
        }
    }

    constexpr Bits operator<<(size_t amount) const {
        if constexpr (!is_not_native_int) {
            return Bits<W>(static_cast<IntType>(raw() << amount));
        } else if constexpr (using_APInt) {
            // TODO
        } else {
            BigInt<W> result = storage_;
            shift_left(result, amount);
            return Bits<W>(result);
        }
    }

    constexpr Bits sra(size_t amount) const {
        if constexpr (!is_not_native_int) {
            auto ext = this->sign_extended();
            return Bits<W>(static_cast<IntType>(ext >> amount));
        } else if constexpr (using_APInt) {
            // TODO
        } else {
            BigInt<W> result = storage_;
            shift_right_arith(result, amount);
            return Bits<W>(result);
        }
    }

    constexpr Bits srl(size_t amount) const {
        if constexpr (!is_not_native_int) {
            return Bits<W>(static_cast<IntType>(raw() >> amount));
        } else if constexpr (using_APInt) {
            // TODO
        } else {
            BigInt<W> result = storage_;
            shift_right_logical(result, amount);
            return Bits<W>(result);
        }
    }

    constexpr bool operator==(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            return storage_ == other.storage_;
        }
        return (raw() == other.raw());
    }

    constexpr auto operator<=>(Bits<W> const& other) const {
        if constexpr (is_not_native_int) {
            if (storage_ == other.storage_) {
                return std::strong_ordering::equal;
            }
            if (storage_ < other.storage_) {
                return std::strong_ordering::less;
            }
            return std::strong_ordering::greater;
        } else {
            return raw() <=> other.raw();
        }
    }

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

    IntType raw() const {
        if constexpr (is_not_native_int) {
            return storage_;
        } else {
            return static_cast<IntType>(storage_ & topMask);
        }
    }

  private:
    IntType storage_;
    static constexpr IntType topMask =
        (W % (sizeof(IntType) * 8) == 0)
            ? ~static_cast<IntType>(0)
            : (static_cast<IntType>(1) << (W % (sizeof(IntType) * 8))) - 1;

    constexpr auto sign_extended() const {
        using SType = std::make_signed_t<IntType>;
        constexpr unsigned shift = sizeof(IntType) * 8 - W;
        return static_cast<SType>(raw() << shift) >> shift;
    }
};

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

// Result range for a binary op between two fixed-width numeric types: width
// max(width(a), width(b)), normalized to {N-1 DOWNTO 0} (VHDL numeric_std
// convention).
template <Range A, Range B>
inline constexpr Range int_result_range =
    int_downto_range(std::max(A.length(), B.length()));

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

}  // namespace detail

template <typename T>
[[nodiscard]] constexpr detail::auto_reinterpreted<T const&> as(T const& x) noexcept {
    return detail::auto_reinterpreted<T const&>(x);
}

template <typename T>
    requires(!std::is_lvalue_reference_v<T>)
[[nodiscard]] constexpr detail::auto_reinterpreted<T> as(T&& x) noexcept {
    return detail::auto_reinterpreted<T>(std::move(x));
}

}  // namespace coconext::types

#endif  // COCONEXT_INT_BASE_HPP

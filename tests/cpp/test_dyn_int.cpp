// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <algorithm>
#include <coconext/types/dyn_signed.hpp>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>

using coconext::types::Bit;
using coconext::types::BitArray;
using coconext::types::BitVector;
using coconext::types::detail::DynSigned;
using coconext::types::detail::DynSInt;
using coconext::types::detail::DynUInt;
using coconext::types::detail::DynUnsigned;
namespace detail = coconext::types::detail;

template <typename T>
class DynIntegerIteration : public testing::Test {};

using DynIntegerTypes = testing::Types<DynUnsigned, DynSigned>;
TYPED_TEST_SUITE(DynIntegerIteration, DynIntegerTypes);

TYPED_TEST(DynIntegerIteration, forward_reverse_and_mutation) {
    static_assert(std::ranges::random_access_range<TypeParam>);
    static_assert(std::ranges::random_access_range<TypeParam const>);
    static_assert(std::ranges::sized_range<TypeParam>);
    static_assert(std::same_as<std::ranges::range_value_t<TypeParam>, Bit>);

    for (size_t width : {8u, 65u, 129u}) {
        SCOPED_TRACE(width);
        TypeParam value(10, width);
        std::string const expected = std::string(width - 4, '0') + "1010";
        std::string forward;
        for (auto bit : value) {
            forward += static_cast<char>(bit);
        }
        EXPECT_EQ(forward, expected);
        forward.clear();
        for (auto bit : std::as_const(value)) {
            forward += static_cast<char>(bit);
        }
        EXPECT_EQ(forward, expected);
        std::string reverse;
        for (auto it = value.rbegin(); it != value.rend(); ++it) {
            reverse += static_cast<char>(*it);
        }
        EXPECT_EQ(reverse, std::string(expected.rbegin(), expected.rend()));
        reverse.clear();
        for (auto it = std::as_const(value).rbegin(); it != std::as_const(value).rend();
             ++it)
        {
            reverse += static_cast<char>(*it);
        }
        EXPECT_EQ(reverse, std::string(expected.rbegin(), expected.rend()));
        EXPECT_EQ(value.end() - value.begin(), static_cast<std::ptrdiff_t>(width));
        EXPECT_EQ(static_cast<Bit>(value.begin()[width - 4]), Bit::_1);
        *value.begin() = Bit::_1;
        *value.rbegin() = Bit::_1;
        EXPECT_TRUE(value.index(width - 1));
        EXPECT_TRUE(value.index(0));
        EXPECT_EQ(value.width(), width);
    }
}

TYPED_TEST(DynIntegerIteration, empty) {
    TypeParam empty{DynUInt(0)};
    EXPECT_EQ(empty.begin(), empty.end());
    EXPECT_EQ(empty.rbegin(), empty.rend());
    EXPECT_EQ(std::as_const(empty).begin(), std::as_const(empty).end());
    EXPECT_EQ(std::as_const(empty).rbegin(), std::as_const(empty).rend());
}

TEST(DynInt, signed_iteration_preserves_twos_complement) {
    for (size_t width : {8u, 65u, 129u}) {
        DynSigned const value(-10, width);
        std::string bits;
        for (auto bit : value) {
            bits += static_cast<char>(bit);
        }
        EXPECT_EQ(bits, std::string(width - 4, '1') + "0110");
    }
}

template <typename Target, typename Source>
concept CanReinterpret =
    requires(Source&& source) { std::move(source).template as<Target>(); };

template <typename Target, typename Source>
concept CanReinterpretLvalue = requires(Source& source) { source.template as<Target>(); };

template <typename Source>
concept CanDeferLvalue = requires(Source& source) { source.as(); };

static_assert(CanReinterpret<DynUnsigned, BitVector>);
static_assert(CanReinterpret<BitVector, DynSigned>);
static_assert(!CanReinterpret<BitArray<65>, BitVector>);
static_assert(!CanReinterpret<BitVector, BitArray<65>>);
static_assert(!CanReinterpretLvalue<DynUnsigned, BitVector>);
static_assert(!CanDeferLvalue<BitVector>);
static_assert(std::is_convertible_v<decltype(std::declval<BitVector>().as()), DynUnsigned>);
static_assert(
    !std::is_convertible_v<decltype(std::declval<BitVector>().as()), BitArray<65>>
);
static_assert(!std::is_constructible_v<DynUnsigned, BitVector>);
static_assert(!detail::HasStorage<int>);
static_assert(detail::HasStaticStorage<BitArray<65>>);
static_assert(!detail::HasDynamicStorage<BitArray<65>>);
static_assert(detail::HasDynamicStorage<BitVector>);
static_assert(detail::HasDynamicStorage<DynUnsigned>);
static_assert(
    std::same_as<decltype(std::declval<BitVector>().as<DynUnsigned>()), DynUnsigned>
);

TEST(DynInt, runtime_width_storage_and_formatting) {
    static_assert(std::is_same_v<DynUInt::NativeUInt, std::uint64_t>);
    static_assert(std::is_same_v<DynUInt::NativeSInt, std::int64_t>);
    static_assert(DynUInt::sbo_bits == 64);
    static_assert(DynSInt::sbo_bits == 64);
    static_assert(sizeof(DynUInt) == sizeof(size_t) + sizeof(uint64_t));
    static_assert(std::is_nothrow_constructible_v<DynUInt, DynSInt&&>);
    static_assert(std::is_nothrow_constructible_v<DynSInt, DynUInt&&>);

    DynUInt native_value(uint64_t{12345}, DynUInt::sbo_bits);
    DynUInt heap_value(uint64_t{12345}, DynUInt::sbo_bits + 1);
    EXPECT_EQ(native_value.to_decimal_string(), "12345");
    EXPECT_EQ(heap_value.to_decimal_string(), "12345");
    EXPECT_EQ(native_value.popcount(), heap_value.popcount());

    DynUInt hex("0x1234567890ABCDEF1122334455667788AABBCCDD", 200);
    EXPECT_EQ(
        hex.to_hexadecimal_string(), "00000000001234567890abcdef1122334455667788aabbccdd"
    );
    EXPECT_EQ(hex.to_decimal_string(), "103929005307927756724023193881144724129310297309");
    EXPECT_EQ(DynSInt("-1", 200).to_decimal_string(), "-1");
    EXPECT_THROW(DynUInt("999", 8), std::out_of_range);

    EXPECT_EQ(DynSInt(-1, 12).popcount(), 12u);
    EXPECT_EQ(DynSInt(-1, 12).count_leading_zeros(), 0u);
    EXPECT_EQ(DynUInt(1, DynUInt::sbo_bits + 1).count_leading_zeros(), DynUInt::sbo_bits);

    EXPECT_FALSE(DynSInt(0).is_negative());
    EXPECT_FALSE(DynSInt(0, 12).is_negative());
    EXPECT_TRUE(DynSInt(-1, 12).is_negative());
    EXPECT_FALSE(DynSInt(1, 200).is_negative());
    EXPECT_TRUE(DynSInt(-1, 200).is_negative());
}

TEST(DynInt, native_tier_handles_full_64_bit_values) {
    using NativeUInt = DynUInt::NativeUInt;
    using NativeSInt = DynSInt::NativeSInt;
    constexpr NativeUInt unsigned_max = std::numeric_limits<NativeUInt>::max();
    constexpr NativeSInt signed_min = std::numeric_limits<NativeSInt>::min();

    DynUInt u(unsigned_max, DynUInt::sbo_bits);
    EXPECT_EQ(u.to_decimal_string(), std::to_string(unsigned_max));
    EXPECT_EQ(u.to_hexadecimal_string(), std::string(DynUInt::sbo_bits / 4, 'f'));
    EXPECT_EQ((u >> (DynUInt::sbo_bits - 1)).to_decimal_string(), "1");

    DynSInt s(signed_min, DynSInt::sbo_bits);
    EXPECT_EQ(s.to_decimal_string(), std::to_string(signed_min));
    EXPECT_EQ((s >> (DynSInt::sbo_bits - 1)).to_decimal_string(), "-1");
    EXPECT_LT(s, DynSInt(NativeSInt{-1}, DynSInt::sbo_bits));
}

TEST(DynInt, native_operands_produce_wide_results_without_losing_bits) {
    constexpr uint64_t unsigned_max = std::numeric_limits<uint64_t>::max();

    auto sum = DynUInt(unsigned_max, 64) + DynUInt(uint64_t{1}, 64);
    EXPECT_EQ(sum.width(), 65u);
    EXPECT_EQ(sum.to_decimal_string(), "18446744073709551616");

    auto difference = DynUInt(uint64_t{0}, 64) - DynUInt(unsigned_max, 64);
    EXPECT_EQ(difference.width(), 65u);
    EXPECT_EQ(difference.to_decimal_string(), "-18446744073709551615");

    auto product = DynUInt(unsigned_max, 64) * DynUInt(unsigned_max, 64);
    EXPECT_EQ(product.width(), 128u);
    EXPECT_EQ(product.to_decimal_string(), "340282366920938463426481119284349108225");

    auto quotient = DynUInt(unsigned_max, 64) / DynUInt(uint64_t{3}, 64);
    EXPECT_EQ(quotient.width(), 65u);
    EXPECT_EQ(quotient.to_decimal_string(), "6148914691236517205");

    auto signed_overflow_quotient =
        DynSInt(std::numeric_limits<int64_t>::min(), 64) / DynSInt(int64_t{-1}, 64);
    EXPECT_EQ(signed_overflow_quotient.width(), 65u);
    EXPECT_EQ(signed_overflow_quotient.to_decimal_string(), "9223372036854775808");

    auto signed_sum =
        DynSInt(std::numeric_limits<int64_t>::max(), 64) + DynSInt(int64_t{1}, 64);
    EXPECT_EQ(signed_sum.width(), 65u);
    EXPECT_EQ(signed_sum.to_decimal_string(), "9223372036854775808");

    auto signed_product =
        DynSInt(std::numeric_limits<int64_t>::min(), 64) * DynSInt(int64_t{-1}, 64);
    EXPECT_EQ(signed_product.width(), 128u);
    EXPECT_EQ(signed_product.to_decimal_string(), "9223372036854775808");
}

TEST(DynInt, copy_move_and_conversion_cross_native_boundary) {
    using NativeUInt = DynUInt::NativeUInt;
    constexpr NativeUInt unsigned_max = std::numeric_limits<NativeUInt>::max();
    size_t const native_width = DynUInt::sbo_bits;
    size_t const wide_width = native_width + 1;

    DynUInt native(unsigned_max, native_width);
    DynUInt wide(native, wide_width);
    EXPECT_EQ(wide.to_decimal_string(), std::to_string(unsigned_max));
    EXPECT_FALSE(wide.get_bit(native_width));

    DynUInt wide_copy(wide);
    wide_copy.set_bit(0, false);
    EXPECT_TRUE(wide.get_bit(0));
    EXPECT_FALSE(wide_copy.get_bit(0));
    DynUInt wide_move(std::move(wide_copy));
    EXPECT_EQ(wide_move.to_decimal_string(), std::to_string(unsigned_max - 1));

    DynUInt native_copy(wide, native_width);
    DynUInt native_move(std::move(native_copy));
    EXPECT_EQ(native_move.to_decimal_string(), std::to_string(unsigned_max));

    DynUInt assigned_native(uint64_t{0}, wide_width);
    assigned_native = DynUInt(uint64_t{17}, native_width);
    EXPECT_EQ(assigned_native.width(), native_width);
    EXPECT_EQ(assigned_native.to_decimal_string(), "17");

    DynUInt assigned_wide(uint64_t{0}, native_width);
    assigned_wide = DynUInt(unsigned_max, wide_width);
    EXPECT_EQ(assigned_wide.width(), wide_width);
    EXPECT_EQ(assigned_wide.to_decimal_string(), std::to_string(unsigned_max));

    DynSInt signed_native(DynSInt::NativeSInt{-1}, native_width);
    DynSInt signed_wide(signed_native, wide_width);
    EXPECT_EQ(signed_wide.to_decimal_string(), "-1");
    EXPECT_EQ(DynSInt(signed_wide, native_width).to_decimal_string(), "-1");
}

TEST(DynInt, cross_signed_heap_move_canonicalizes_aligned_and_unaligned_storage) {
    size_t const unaligned_width = DynUInt::sbo_bits + 1;
    DynUInt unaligned_source(uint64_t{1}, unaligned_width);
    DynSInt unaligned_target(std::move(unaligned_source));
    EXPECT_EQ(unaligned_source.width(), 0u);
    EXPECT_EQ(unaligned_target.to_decimal_string(), "1");

    DynSInt unaligned_signed_source(-1, unaligned_width);
    DynUInt unaligned_unsigned_target(std::move(unaligned_signed_source));
    EXPECT_EQ(unaligned_signed_source.width(), 0u);
    EXPECT_EQ(unaligned_unsigned_target.popcount(), unaligned_width);

    size_t const aligned_width = DynUInt::sbo_bits * 2;
    DynUInt aligned_source(uint64_t{1}, aligned_width);
    DynSInt aligned_target(std::move(aligned_source));
    EXPECT_EQ(aligned_source.width(), 0u);
    EXPECT_EQ(aligned_target.to_decimal_string(), "1");

    DynSInt aligned_signed_source(-1, aligned_width);
    DynUInt aligned_unsigned_target(std::move(aligned_signed_source));
    EXPECT_EQ(aligned_signed_source.width(), 0u);
    EXPECT_EQ(aligned_unsigned_target.popcount(), aligned_width);
}

TEST(DynInt, bitwise_shift_compare_and_truncate) {
    DynUInt a(0x5F3F4AE, 29);
    DynUInt b(0x4FAA413, 29);
    EXPECT_EQ((a & b).to_hexadecimal_string(), "04f2a402");
    EXPECT_EQ((a | b).to_hexadecimal_string(), "05fbf4bf");
    EXPECT_EQ((a ^ b).to_hexadecimal_string(), "010950bd");
    EXPECT_EQ((a >> 9).to_decimal_string(), "195066");

    DynSInt negative(-268435398, 29);
    EXPECT_EQ((negative >> 9).to_decimal_string(), "-524288");
    EXPECT_LT(DynUInt(1, 8), DynUInt(2, 8));
    EXPECT_LT(DynSInt(-1, 8), DynSInt(1, 8));
    EXPECT_NE(DynUInt(1, 8), DynUInt(1, 200));
    EXPECT_THROW(static_cast<void>(DynUInt(1, 8) < DynUInt(2, 200)), std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(DynSInt(-1, 8) < DynSInt(1, 200)), std::invalid_argument
    );

    EXPECT_EQ(
        DynUInt("0x1234567890ABCDEF", 200).truncate(32).to_hexadecimal_string(), "90abcdef"
    );
    EXPECT_THROW(DynUInt(1, 8).truncate(200), std::invalid_argument);
}

TEST(DynInt, native_operations_preserve_extension_without_signed_overflow) {
    using NativeSInt = DynSInt::NativeSInt;
    constexpr size_t width = DynSInt::sbo_bits - 1;
    constexpr NativeSInt magnitude = NativeSInt{1} << (width - 1);
    constexpr NativeSInt signed_max = magnitude - 1;
    constexpr NativeSInt signed_min = -magnitude;

    auto growing_positive = DynSInt(signed_max, width) + DynSInt(NativeSInt{1}, width);
    EXPECT_EQ(growing_positive.width(), DynSInt::sbo_bits);
    EXPECT_EQ(growing_positive.to_decimal_string(), std::to_string(magnitude));

    auto growing_negative = DynSInt(signed_min, width) + DynSInt(NativeSInt{-1}, width);
    EXPECT_EQ(growing_negative.width(), DynSInt::sbo_bits);
    EXPECT_EQ(growing_negative.to_decimal_string(), std::to_string(signed_min - 1));

    auto shifted_sign = DynSInt(magnitude >> 1, width) << 1;
    EXPECT_EQ(shifted_sign.to_decimal_string(), std::to_string(signed_min));
}

TEST(DynInt, saturation_compares_across_the_storage_boundary) {
    using NativeUInt = DynUInt::NativeUInt;
    using NativeSInt = DynSInt::NativeSInt;
    size_t const native_width = DynUInt::sbo_bits;
    size_t const wide_width = native_width + 1;

    DynUInt wide_unsigned(std::numeric_limits<NativeUInt>::max(), wide_width);
    EXPECT_EQ(
        wide_unsigned.saturate_unsigned(native_width).to_decimal_string(),
        std::to_string(std::numeric_limits<NativeUInt>::max())
    );

    DynSInt wide_positive(std::numeric_limits<NativeUInt>::max(), wide_width);
    EXPECT_EQ(
        wide_positive.saturate_signed(native_width).to_decimal_string(),
        std::to_string(std::numeric_limits<NativeSInt>::max())
    );

    DynSInt below_native_min = DynSInt(std::numeric_limits<NativeSInt>::min(), native_width)
                             - DynSInt(NativeSInt{1}, native_width);
    EXPECT_EQ(
        below_native_min.saturate_signed(native_width).to_decimal_string(),
        std::to_string(std::numeric_limits<NativeSInt>::min())
    );
}

// Growing arithmetic handles differing widths natively -- that is the whole
// point -- so mismatched operands are ordinary here, not an error.
TEST(DynInt, growing_arithmetic_accepts_mixed_widths) {
    DynUInt a(uint64_t{200}, 8);
    DynUInt b(uint64_t{1000}, 16);

    auto sum = a + b;
    EXPECT_EQ(sum.width(), 17u);
    EXPECT_EQ(sum.to_decimal_string(), "1200");

    auto prod = a * b;
    EXPECT_EQ(prod.width(), 24u);
    EXPECT_EQ(prod.to_decimal_string(), "200000");

    // Unsigned subtraction borrows into the extra bit instead of wrapping.
    EXPECT_EQ((a - b).to_decimal_string(true), "-800");

    auto [quotient, remainder] = detail::divrem(a, DynUInt(uint64_t{3}, 200));
    EXPECT_EQ(quotient.width(), 9u);
    EXPECT_EQ(remainder.width(), 200u);
    EXPECT_EQ(quotient.to_decimal_string(), "66");
    EXPECT_EQ(remainder.to_decimal_string(), "2");
}

TEST(DynInt, signed_growing_arithmetic) {
    DynSInt neg(int8_t{-56}, 8);
    DynSInt pos(int8_t{100}, 8);
    DynSInt neg3(int8_t{-3}, 8);
    DynSInt pos56(int8_t{56}, 8);

    EXPECT_EQ((neg + pos).to_decimal_string(true), "44");
    EXPECT_EQ((neg * pos).to_decimal_string(true), "-5600");
    EXPECT_EQ((-neg).to_decimal_string(true), "56");
    EXPECT_EQ(detail::abs(neg).to_decimal_string(true), "56");

    // rem follows the dividend's sign, mod the divisor's.
    EXPECT_EQ((neg % pos).to_decimal_string(true), "-56");
    EXPECT_EQ(detail::mod(neg, pos).to_decimal_string(true), "44");
    EXPECT_EQ((pos56 % neg3).to_decimal_string(true), "2");
    EXPECT_EQ(detail::mod(pos56, neg3).to_decimal_string(true), "-1");

    DynSInt wide_neg(int64_t{-17}, 200);
    auto [truncated, rem] = detail::divrem(wide_neg, DynSInt(int64_t{5}, 8));
    auto [floored, mod] = detail::divmod(wide_neg, DynSInt(int64_t{5}, 8));
    EXPECT_EQ(truncated.to_decimal_string(true), "-3");
    EXPECT_EQ(rem.to_decimal_string(true), "-2");
    EXPECT_EQ(floored.to_decimal_string(true), "-4");
    EXPECT_EQ(mod.to_decimal_string(true), "3");

    auto [negative_divisor_q, negative_divisor_r] =
        detail::divrem(DynSInt(17, 200), DynSInt(-5, 8));
    EXPECT_EQ(negative_divisor_q.to_decimal_string(true), "-3");
    EXPECT_EQ(negative_divisor_r.to_decimal_string(true), "2");

    auto [both_negative_q, both_negative_r] =
        detail::divrem(DynSInt(-17, 200), DynSInt(-5, 8));
    EXPECT_EQ(both_negative_q.to_decimal_string(true), "3");
    EXPECT_EQ(both_negative_r.to_decimal_string(true), "-2");

    auto [smaller_q, smaller_r] = detail::divrem(DynSInt(-5, 8), DynSInt(-17, 200));
    EXPECT_EQ(smaller_q.to_decimal_string(true), "0");
    EXPECT_EQ(smaller_r.to_decimal_string(true), "-5");

    auto [equal_q, equal_r] = detail::divrem(DynSInt(-5, 8), DynSInt(-5, 200));
    EXPECT_EQ(equal_q.to_decimal_string(true), "1");
    EXPECT_EQ(equal_r.to_decimal_string(true), "0");

    // The extra quotient bit keeps signed_min / -1 representable.
    DynSInt min_val(int8_t{-128}, 8);
    DynSInt minus_one(int8_t{-1}, 8);
    EXPECT_EQ((min_val / minus_one).to_decimal_string(true), "128");
}

TEST(DynInt, signed_and_unsigned_have_distinct_values) {
    static_assert(!std::is_same_v<DynUInt, DynSInt>);
    static_assert(sizeof(DynUInt) == sizeof(DynSInt));

    DynUInt unsigned_negative_pattern(0x1FF, 9);
    DynSInt signed_negative_pattern(-1, 9);
    EXPECT_EQ(unsigned_negative_pattern.to_decimal_string(), "511");
    EXPECT_EQ(signed_negative_pattern.to_decimal_string(), "-1");

    DynUInt wide_unsigned(DynSInt(-1, 129), 129);
    DynSInt wide_signed(-1, 129);
    EXPECT_EQ(wide_unsigned, ~DynUInt(129));
    EXPECT_EQ(wide_signed.to_decimal_string(), "-1");

    DynUInt converted_wide_unsigned(DynUInt(0xFF, 8), 129);
    DynSInt converted_wide_signed(DynSInt(-1, 8), 129);
    EXPECT_EQ(converted_wide_unsigned.to_decimal_string(), "255");
    EXPECT_EQ(converted_wide_signed.to_decimal_string(), "-1");

    DynSInt sign_bit(0, 9);
    sign_bit.set_bit(8, true);
    EXPECT_EQ(sign_bit.to_decimal_string(), "-256");
    sign_bit.set_bit(8, false);
    EXPECT_EQ(sign_bit.to_decimal_string(), "0");
    EXPECT_THROW(sign_bit.set_bit(9, true), std::out_of_range);

    size_t const heap_width = DynSInt::sbo_bits + 1;
    DynSInt heap_sign_bit(0, heap_width);
    heap_sign_bit.set_bit(heap_width - 1, true);
    EXPECT_TRUE(heap_sign_bit.get_bit(heap_width - 1));
    EXPECT_EQ((heap_sign_bit >> (heap_width - 1)).to_decimal_string(), "-1");
    heap_sign_bit.set_bit(heap_width - 1, false);
    EXPECT_EQ(heap_sign_bit.to_decimal_string(), "0");

    DynUInt heap_bit(0, heap_width);
    heap_bit.set_bit(heap_width - 1, true);
    EXPECT_TRUE(heap_bit.get_bit(heap_width - 1));
    heap_bit.set_bit(heap_width - 1, false);
    EXPECT_EQ(heap_bit.to_decimal_string(), "0");
}

TEST(DynInt, native_static_and_parsed_construction_preserve_values) {
    DynUInt native_unsigned(0x1FF, 9);
    DynSInt native_signed(-1, 9);
    EXPECT_EQ(native_unsigned.to_decimal_string(), "511");
    EXPECT_EQ(native_signed.to_decimal_string(), "-1");

    DynUInt two_word_unsigned(~uint64_t{0}, 65);
    DynSInt two_word_signed(-1, 65);
    EXPECT_EQ(two_word_unsigned.to_decimal_string(), std::to_string(~uint64_t{0}));
    EXPECT_EQ(two_word_signed.to_decimal_string(), "-1");

    DynUInt from_native_static(detail::UInt<9>(0x1FF));
    DynSInt from_native_static_signed(detail::SInt<9>(-1));
    DynUInt from_wide_static(detail::UInt<129>(detail::SInt<129>(-1)));
    DynSInt from_wide_static_signed(detail::SInt<129>(-1));
    EXPECT_EQ(from_native_static.to_decimal_string(), "511");
    EXPECT_EQ(from_native_static_signed.to_decimal_string(), "-1");
    EXPECT_EQ(from_wide_static, ~DynUInt(129));
    EXPECT_EQ(from_wide_static_signed.to_decimal_string(), "-1");

    DynUInt parsed_unsigned("511", 9);
    DynSInt parsed_signed("-1", 9);
    EXPECT_EQ(parsed_unsigned.to_decimal_string(), "511");
    EXPECT_EQ(parsed_signed.to_decimal_string(), "-1");

    // Exercise the heap-storage assignment path with a narrow signed source.
    DynSInt from_narrow_signed(int8_t{-1}, 200);
    EXPECT_EQ(from_narrow_signed.to_decimal_string(), "-1");
}

TEST(DynInt, runtime_formatting_and_error_paths) {
    DynUInt value(uint16_t{0xABCD}, 16);
    EXPECT_EQ(value.to_binary_string(), "1010101111001101");
    EXPECT_EQ(value.to_octal_string(), "125715");
    EXPECT_THROW(value.get_bit(16), std::out_of_range);
    EXPECT_THROW(value.set_bit(16, true), std::out_of_range);

    EXPECT_EQ(DynUInt("+42", 8).to_decimal_string(), "42");
    EXPECT_EQ(DynUInt("0X_Af", 8).to_decimal_string(), "175");
    EXPECT_THROW(DynSInt("-0x1", 8), std::invalid_argument);
    EXPECT_THROW(DynUInt("0xgg", 8), std::invalid_argument);
    EXPECT_THROW(DynUInt("12x3", 64), std::invalid_argument);

    DynUInt null_value(0);
    EXPECT_EQ(null_value.to_binary_string(), "");
    EXPECT_EQ(null_value.to_decimal_string(), "");
    EXPECT_EQ(null_value.to_hexadecimal_string(), "");
    EXPECT_EQ(null_value.to_octal_string(), "");
    EXPECT_THROW(null_value.to_native_integer<uint8_t>(), std::domain_error);
    EXPECT_THROW(DynUInt(uint64_t{1}, 0), std::invalid_argument);

    DynUInt dividend(uint64_t{5}, 200);
    DynUInt zero(200);
    EXPECT_THROW(dividend / zero, std::domain_error);
    EXPECT_THROW(dividend % zero, std::domain_error);
    EXPECT_EQ(dividend / DynUInt(uint64_t{7}, 200), DynUInt(uint64_t{0}, 201));
    EXPECT_EQ(dividend / dividend, DynUInt(uint64_t{1}, 201));

    EXPECT_EQ((dividend << 200).to_decimal_string(), "0");
    EXPECT_EQ((dividend >> 200).to_decimal_string(), "0");
    EXPECT_EQ((DynSInt(-1, 200) >> 200).to_decimal_string(), "-1");

    EXPECT_EQ(DynUInt(uint64_t{42}, 200).saturate_unsigned(8).to_decimal_string(), "42");
    EXPECT_EQ(DynSInt(int64_t{42}, 200).saturate_signed(8).to_decimal_string(), "42");
    EXPECT_EQ(DynUInt(uint64_t{42}, 8).saturate_unsigned(16).to_decimal_string(), "42");
    EXPECT_EQ(DynSInt(int64_t{42}, 8).saturate_signed(16).to_decimal_string(), "42");
    EXPECT_EQ(DynUInt(uint64_t{42}, 8).saturate_unsigned(0).width(), 0u);
    EXPECT_EQ(DynSInt(int64_t{42}, 8).saturate_signed(0).width(), 0u);

    EXPECT_EQ(DynUInt(uint64_t{255}, 200).to_native_integer<uint8_t>(), 255);
    EXPECT_EQ(DynSInt(int64_t{-128}, 200).to_native_integer<int8_t>(), -128);
    EXPECT_THROW(
        DynUInt(uint64_t{256}, 200).to_native_integer<uint8_t>(), std::out_of_range
    );
    EXPECT_THROW(DynSInt(int64_t{-1}, 200).to_native_integer<uint8_t>(), std::out_of_range);
}

TEST(DynInt, growing_arithmetic_preserves_the_result_invariant) {
    auto unsigned_sum = DynUInt(200, 8) + DynUInt(100, 8);
    auto unsigned_difference = DynUInt(5, 8) - DynUInt(7, 8);
    auto signed_sum = DynSInt(-56, 8) + DynSInt(100, 8);
    auto signed_product = DynSInt(-3, 8) * DynSInt(7, 8);

    EXPECT_EQ(unsigned_sum.width(), 9);
    EXPECT_EQ(unsigned_sum.to_decimal_string(), "300");
    EXPECT_EQ(unsigned_difference.to_decimal_string(), "-2");
    EXPECT_EQ(signed_sum.to_decimal_string(), "44");
    EXPECT_EQ(signed_product.to_decimal_string(), "-21");

    auto [quotient, remainder] = detail::divrem(DynSInt(-17, 8), DynSInt(5, 8));
    EXPECT_EQ(quotient.to_decimal_string(), "-3");
    EXPECT_EQ(remainder.to_decimal_string(), "-2");
}

TEST(DynSigned, remainder_and_modulo_are_distinct) {
    DynSigned negative(-17, 8);
    DynSigned positive(5, 4);
    DynSigned negative_divisor(-5, 4);

    EXPECT_EQ(static_cast<long long>(negative % positive), -2);
    EXPECT_EQ(static_cast<long long>(detail::rem(negative, positive)), -2);
    EXPECT_EQ(static_cast<long long>(detail::mod(negative, positive)), 3);

    EXPECT_EQ(static_cast<long long>(DynSigned(17, 8) % negative_divisor), 2);
    EXPECT_EQ(static_cast<long long>(detail::rem(DynSigned(17, 8), negative_divisor)), 2);
    EXPECT_EQ(static_cast<long long>(detail::mod(DynSigned(17, 8), negative_divisor)), -3);

    negative %= positive;
    EXPECT_EQ(static_cast<long long>(negative), -2);
    EXPECT_EQ(negative.width(), 8u);

    EXPECT_THROW(
        static_cast<void>(detail::rem(negative, DynSigned(0, 4))), std::domain_error
    );
    EXPECT_THROW(
        static_cast<void>(detail::mod(negative, DynSigned(0, 4))), std::domain_error
    );
}

TEST(DynInt, bit_vector_reinterpretation) {
    BitVector bits("10000000000000000000000000000000000000000000000000000000000000001");

    DynSigned signed_value = std::move(bits).as();
    EXPECT_EQ(signed_value.width(), 65U);
    EXPECT_EQ(detail::storage(signed_value).popcount(), 2U);
    EXPECT_EQ(static_cast<long long>(signed_value >> 64), -1);

    DynUnsigned unsigned_value = std::move(signed_value).as<DynUnsigned>();
    EXPECT_EQ(unsigned_value.width(), 65U);
    EXPECT_EQ(detail::storage(unsigned_value).popcount(), 2U);

    BitVector restored = std::move(unsigned_value).as();
    EXPECT_EQ(
        restored,
        BitVector("10000000000000000000000000000000000000000000000000000000000000001")
    );

    auto direct_unsigned = BitVector("10100101").as<DynUnsigned>();
    EXPECT_EQ(detail::storage(direct_unsigned).to_binary_string(), "10100101");
}

// Exercise each operation with native, aligned heap, and partial-word heap
// operands on both sides. DynInt is not constexpr, so these already run at runtime.
template <typename T>
class DynIntStorage : public ::testing::Test {};

using DynIntStorageTypes = ::testing::Types<DynUInt, DynSInt>;
TYPED_TEST_SUITE(DynIntStorage, DynIntStorageTypes);

TYPED_TEST(DynIntStorage, arithmetic_across_storage_tiers) {
    for (size_t lhs_width : {9u, 64u, 65u, 128u, 129u}) {
        for (size_t rhs_width : {9u, 64u, 65u, 128u, 129u}) {
            SCOPED_TRACE(::testing::Message() << lhs_width << ", " << rhs_width);
            TypeParam a(TypeParam::is_signed ? -201 : 201, lhs_width);
            TypeParam b(7, rhs_width);
            auto sum = a + b;
            auto difference = a - b;
            auto product = a * b;
            auto [quotient, remainder] = detail::divrem(a, b);
            EXPECT_EQ(sum.width(), std::max(lhs_width, rhs_width) + 1);
            EXPECT_EQ(difference.width(), sum.width());
            EXPECT_EQ(product.width(), lhs_width + rhs_width);
            EXPECT_EQ(quotient.width(), lhs_width + 1);
            EXPECT_EQ(remainder.width(), rhs_width);
            EXPECT_EQ(sum.to_decimal_string(), TypeParam::is_signed ? "-194" : "208");
            EXPECT_EQ(
                difference.to_decimal_string(), TypeParam::is_signed ? "-208" : "194"
            );
            EXPECT_EQ(product.to_decimal_string(), TypeParam::is_signed ? "-1407" : "1407");
            EXPECT_EQ(quotient.to_decimal_string(), TypeParam::is_signed ? "-28" : "28");
            EXPECT_EQ(remainder.to_decimal_string(), TypeParam::is_signed ? "-5" : "5");
            EXPECT_EQ((-a).width(), lhs_width + 1);
            EXPECT_EQ((-a).to_decimal_string(), TypeParam::is_signed ? "201" : "-201");
        }
    }
}

TYPED_TEST(DynIntStorage, bitwise_shifts_and_formatting_across_storage_tiers) {
    for (size_t width : {8u, 64u, 65u, 128u, 129u}) {
        SCOPED_TRACE(width);
        TypeParam a(0x55, width);
        TypeParam b(0x33, width);
        EXPECT_EQ((a & b).template to_native_integer<int>(), 0x11);
        EXPECT_EQ((a | b).template to_native_integer<int>(), 0x77);
        EXPECT_EQ((a ^ b).template to_native_integer<int>(), 0x66);
        EXPECT_EQ((a << 1).logical_bits().template to_native_integer<unsigned>(), 0xAAu);
        EXPECT_EQ((a >> 1).template to_native_integer<int>(), 0x2A);
        EXPECT_EQ(a.to_binary_string(), std::string(width - 8, '0') + "01010101");
        EXPECT_EQ(a.to_octal_string(), std::string((width + 2) / 3 - 3, '0') + "125");

        TypeParam sign_bit(width);
        sign_bit.set_bit(width - 1, true);
        EXPECT_EQ(sign_bit.popcount(), 1u);
        EXPECT_EQ(sign_bit.count_leading_zeros(), 0u);
        EXPECT_EQ(sign_bit.count_trailing_zeros(), width - 1);
        EXPECT_EQ((sign_bit << 1).popcount(), 0u);
        EXPECT_EQ((sign_bit >> width).popcount(), TypeParam::is_signed ? width : 0u);
        EXPECT_EQ(
            (sign_bit >> (width - 1)).to_decimal_string(), TypeParam::is_signed ? "-1" : "1"
        );
        EXPECT_EQ(TypeParam(width).count_leading_zeros(), width);
        EXPECT_EQ(TypeParam(width).count_trailing_zeros(), width);
        EXPECT_EQ((~TypeParam(width)).popcount(), width);

        TypeParam wrong_width(width + 1);
        EXPECT_THROW(a & wrong_width, std::invalid_argument);
        EXPECT_THROW(a | wrong_width, std::invalid_argument);
        EXPECT_THROW(a ^ wrong_width, std::invalid_argument);
    }
}

TYPED_TEST(DynIntStorage, bit_proxies_and_random_access_iterators) {
    for (size_t width : {8u, 65u, 129u}) {
        SCOPED_TRACE(width);
        TypeParam value(0, width);
        value[0] = Bit::_1;
        value[width - 1] = value[0];
        EXPECT_TRUE(static_cast<bool>(value[width - 1]));
        EXPECT_EQ(static_cast<char>(value[0]), '1');
        EXPECT_EQ(static_cast<char>(value[1]), '0');
        EXPECT_EQ(std::as_const(value)[0], Bit::_1);
        EXPECT_THROW(static_cast<void>(value[width]), std::out_of_range);
        EXPECT_THROW(static_cast<void>(std::as_const(value)[width]), std::out_of_range);

        auto it = value.begin();
        EXPECT_EQ(static_cast<Bit>(*it++), Bit::_1);
        EXPECT_EQ(static_cast<Bit>(*it), Bit::_0);
        EXPECT_EQ(static_cast<Bit>(*--it), Bit::_1);
        it += width - 1;
        EXPECT_EQ(static_cast<Bit>(*it--), Bit::_1);
        EXPECT_EQ(static_cast<Bit>(*it), Bit::_0);
        it -= width - 2;
        EXPECT_EQ(it, value.begin());
        EXPECT_EQ(value.end() - it, static_cast<std::ptrdiff_t>(width));
        EXPECT_EQ(1 + it, it + 1);
        EXPECT_EQ(value.end() - 1, it + (width - 1));
        EXPECT_LT(it, value.end());
        EXPECT_EQ(static_cast<Bit>(it[width - 1]), Bit::_1);
        *it = Bit::_0;
        EXPECT_FALSE(value.get_bit(width - 1));

        std::string forward;
        for (auto bit : std::as_const(value)) {
            forward += static_cast<char>(bit);
        }
        EXPECT_EQ(forward, std::string(width - 1, '0') + '1');
        std::string reverse;
        for (auto rit = std::as_const(value).rbegin(); rit != std::as_const(value).rend();
             ++rit)
        {
            reverse += static_cast<char>(*rit);
        }
        EXPECT_EQ(reverse, '1' + std::string(width - 1, '0'));
        *value.rbegin() = Bit::_0;
        EXPECT_EQ(
            std::distance(value.rbegin(), value.rend()), static_cast<std::ptrdiff_t>(width)
        );
        EXPECT_EQ(value.popcount(), 0u);
    }
}

TYPED_TEST(DynIntStorage, zero_width_and_division_errors) {
    TypeParam empty(0);
    EXPECT_EQ(empty.begin(), empty.end());
    EXPECT_EQ(empty.rbegin(), empty.rend());
    EXPECT_EQ(empty.popcount(), 0u);
    EXPECT_EQ(empty.count_leading_zeros(), 0u);
    EXPECT_EQ(empty.count_trailing_zeros(), 0u);
    EXPECT_EQ((~empty).width(), 0u);
    EXPECT_EQ((empty << 1).width(), 0u);
    EXPECT_EQ((empty >> 1).width(), 0u);
    EXPECT_EQ((empty + empty).to_decimal_string(), "0");
    EXPECT_EQ((empty * TypeParam(42, 8)).to_decimal_string(), "0");
    for (size_t width : {8u, 65u, 129u}) {
        SCOPED_TRACE(width);
        TypeParam one(1, width);
        TypeParam zero(width);
        EXPECT_THROW(detail::divrem(one, zero), std::domain_error);
        EXPECT_THROW(one / empty, std::domain_error);
    }
}

TEST(DynInt, heap_arithmetic_preserves_high_words) {
    DynUInt high("1267650600228229401496703205376", 129);  // 2**100
    DynUInt low(7, 65);
    EXPECT_EQ((high + low).to_decimal_string(), "1267650600228229401496703205383");
    EXPECT_EQ((high - low).to_decimal_string(), "1267650600228229401496703205369");
    EXPECT_EQ((high * low).to_decimal_string(), "8873554201597605810476922437632");
    EXPECT_EQ((-high).to_decimal_string(), "-1267650600228229401496703205376");
    DynSInt negative("-1267650600228229401496703205376", 129);
    DynSInt seven(7, 65);
    EXPECT_EQ((negative + seven).to_decimal_string(), "-1267650600228229401496703205369");
    EXPECT_EQ((negative - seven).to_decimal_string(), "-1267650600228229401496703205383");
    EXPECT_EQ((negative * seven).to_decimal_string(), "-8873554201597605810476922437632");
    EXPECT_EQ(detail::abs(negative).to_decimal_string(), high.to_decimal_string());
    EXPECT_EQ(
        (-DynSInt(std::numeric_limits<int64_t>::min(), 64)).to_decimal_string(),
        "9223372036854775808"
    );
}

#if defined(__SIZEOF_INT128__)
TEST(DynInt, int128_construction_covers_native_and_heap_storage) {
    __uint128_t const high = __uint128_t{1} << 100;
    for (size_t width : {8u, 64u, 65u, 128u, 129u, 200u}) {
        SCOPED_TRACE(width);
        EXPECT_EQ(DynUInt(__uint128_t{42}, width).to_native_integer<int>(), 42);
        EXPECT_EQ(DynSInt(__int128_t{-42}, width).to_native_integer<int>(), -42);
    }
    for (size_t width : {128u, 129u, 200u}) {
        SCOPED_TRACE(width);
        DynUInt positive(high + 7, width);
        DynSInt negative(-static_cast<__int128_t>(high) - 7, width);
        EXPECT_EQ(positive.to_native_integer<__uint128_t>(), high + 7);
        EXPECT_EQ(
            negative.to_native_integer<__int128_t>(), -static_cast<__int128_t>(high) - 7
        );
        EXPECT_EQ(positive.to_decimal_string(), "1267650600228229401496703205383");
        EXPECT_EQ(negative.to_decimal_string(), "-1267650600228229401496703205383");
    }
}
#endif

TEST(DynInt, wrapper_construction_and_shift_errors) {
    EXPECT_THROW(DynUnsigned(0, 0), std::invalid_argument);
    EXPECT_THROW(DynSigned(0, 0), std::invalid_argument);
    EXPECT_THROW(DynUnsigned(256, 8), std::overflow_error);
    EXPECT_THROW(DynUnsigned(-1, 8), std::overflow_error);
    EXPECT_THROW(DynSigned(128, 8), std::overflow_error);
    EXPECT_THROW(DynSigned(-129, 8), std::overflow_error);
    DynSigned negative(-16, 8);
    EXPECT_EQ(static_cast<long long>(negative >> DynSigned(2, 65)), -4);
    EXPECT_EQ(static_cast<long long>(negative >> DynUnsigned(2, 65)), -4);
    EXPECT_THROW(negative >> -1, std::invalid_argument);
    EXPECT_THROW(negative >> DynSigned(-1, 65), std::invalid_argument);
    EXPECT_THROW(negative /= DynSigned(0, 8), std::domain_error);
}

// LCOV_EXCL_BR_STOP

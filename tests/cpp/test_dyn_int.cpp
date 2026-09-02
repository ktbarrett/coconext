// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types/dyn_signed.hpp>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>

using coconext::types::detail::DynSigned;
using coconext::types::detail::DynSInt;
using coconext::types::detail::DynUInt;
namespace detail = coconext::types::detail;

TEST(DynInt, runtime_width_storage_and_formatting) {
    static_assert(std::is_same_v<DynUInt::NativeUInt, std::uint64_t>);
    static_assert(std::is_same_v<DynUInt::NativeSInt, std::int64_t>);
    static_assert(DynUInt::sbo_bits == 64);
    static_assert(DynSInt::sbo_bits == 64);
    static_assert(sizeof(DynUInt) == sizeof(size_t) + sizeof(uint64_t));

    DynUInt native_value(DynUInt::sbo_bits, uint64_t{12345});
    DynUInt heap_value(DynUInt::sbo_bits + 1, uint64_t{12345});
    EXPECT_EQ(native_value.to_decimal_string(), "12345");
    EXPECT_EQ(heap_value.to_decimal_string(), "12345");
    EXPECT_EQ(native_value.popcount(), heap_value.popcount());

    DynUInt hex(200, "0x1234567890ABCDEF1122334455667788AABBCCDD");
    EXPECT_EQ(
        hex.to_hexadecimal_string(), "00000000001234567890abcdef1122334455667788aabbccdd"
    );
    EXPECT_EQ(hex.to_decimal_string(), "103929005307927756724023193881144724129310297309");
    EXPECT_EQ(DynSInt(200, "-1").to_decimal_string(), "-1");
    EXPECT_THROW(DynUInt(8, "999"), std::out_of_range);

    EXPECT_EQ(DynSInt(12, -1).popcount(), 12u);
    EXPECT_EQ(DynSInt(12, -1).count_leading_zeros(), 0u);
    EXPECT_EQ(DynUInt(DynUInt::sbo_bits + 1, 1).count_leading_zeros(), DynUInt::sbo_bits);

    EXPECT_FALSE(DynSInt(0).is_negative());
    EXPECT_FALSE(DynSInt(12, 0).is_negative());
    EXPECT_TRUE(DynSInt(12, -1).is_negative());
    EXPECT_FALSE(DynSInt(200, 1).is_negative());
    EXPECT_TRUE(DynSInt(200, -1).is_negative());
}

TEST(DynInt, native_tier_handles_full_64_bit_values) {
    using NativeUInt = DynUInt::NativeUInt;
    using NativeSInt = DynSInt::NativeSInt;
    constexpr NativeUInt unsigned_max = std::numeric_limits<NativeUInt>::max();
    constexpr NativeSInt signed_min = std::numeric_limits<NativeSInt>::min();

    DynUInt u(DynUInt::sbo_bits, unsigned_max);
    EXPECT_EQ(u.to_decimal_string(), std::to_string(unsigned_max));
    EXPECT_EQ(u.to_hexadecimal_string(), std::string(DynUInt::sbo_bits / 4, 'f'));
    EXPECT_EQ((u >> (DynUInt::sbo_bits - 1)).to_decimal_string(), "1");
    EXPECT_EQ(
        DynUInt::exact_add(u, DynUInt(DynUInt::sbo_bits, NativeUInt{1}))
            .to_decimal_string(),
        "0"
    );

    DynSInt s(DynSInt::sbo_bits, signed_min);
    EXPECT_EQ(s.to_decimal_string(), std::to_string(signed_min));
    EXPECT_EQ((s >> (DynSInt::sbo_bits - 1)).to_decimal_string(), "-1");
    EXPECT_LT(s, DynSInt(DynSInt::sbo_bits, NativeSInt{-1}));
}

TEST(DynInt, native_operands_produce_wide_results_without_losing_bits) {
    constexpr uint64_t unsigned_max = std::numeric_limits<uint64_t>::max();

    auto sum = DynUInt(64, unsigned_max) + DynUInt(64, uint64_t{1});
    EXPECT_EQ(sum.width(), 65u);
    EXPECT_EQ(sum.to_decimal_string(), "18446744073709551616");

    auto difference = DynUInt(64, uint64_t{0}) - DynUInt(64, unsigned_max);
    EXPECT_EQ(difference.width(), 65u);
    EXPECT_EQ(difference.to_decimal_string(), "-18446744073709551615");

    auto product = DynUInt(64, unsigned_max) * DynUInt(64, unsigned_max);
    EXPECT_EQ(product.width(), 128u);
    EXPECT_EQ(product.to_decimal_string(), "340282366920938463426481119284349108225");

    auto quotient = DynUInt(64, unsigned_max) / DynUInt(64, uint64_t{3});
    EXPECT_EQ(quotient.width(), 65u);
    EXPECT_EQ(quotient.to_decimal_string(), "6148914691236517205");

    auto signed_overflow_quotient =
        DynSInt(64, std::numeric_limits<int64_t>::min()) / DynSInt(64, int64_t{-1});
    EXPECT_EQ(signed_overflow_quotient.width(), 65u);
    EXPECT_EQ(signed_overflow_quotient.to_decimal_string(), "9223372036854775808");

    auto signed_sum =
        DynSInt(64, std::numeric_limits<int64_t>::max()) + DynSInt(64, int64_t{1});
    EXPECT_EQ(signed_sum.width(), 65u);
    EXPECT_EQ(signed_sum.to_decimal_string(), "9223372036854775808");

    auto signed_product =
        DynSInt(64, std::numeric_limits<int64_t>::min()) * DynSInt(64, int64_t{-1});
    EXPECT_EQ(signed_product.width(), 128u);
    EXPECT_EQ(signed_product.to_decimal_string(), "9223372036854775808");
}

TEST(DynInt, copy_move_and_conversion_cross_native_boundary) {
    using NativeUInt = DynUInt::NativeUInt;
    constexpr NativeUInt unsigned_max = std::numeric_limits<NativeUInt>::max();
    size_t const native_width = DynUInt::sbo_bits;
    size_t const wide_width = native_width + 1;

    DynUInt native(native_width, unsigned_max);
    DynUInt wide(wide_width, native);
    EXPECT_EQ(wide.to_decimal_string(), std::to_string(unsigned_max));
    EXPECT_FALSE(wide.get_bit(native_width));

    DynUInt wide_copy(wide);
    wide_copy.set_bit(0, false);
    EXPECT_TRUE(wide.get_bit(0));
    EXPECT_FALSE(wide_copy.get_bit(0));
    DynUInt wide_move(std::move(wide_copy));
    EXPECT_EQ(wide_move.to_decimal_string(), std::to_string(unsigned_max - 1));

    DynUInt native_copy(native_width, wide);
    DynUInt native_move(std::move(native_copy));
    EXPECT_EQ(native_move.to_decimal_string(), std::to_string(unsigned_max));

    DynUInt assigned_native(wide_width, uint64_t{0});
    assigned_native = DynUInt(native_width, uint64_t{17});
    EXPECT_EQ(assigned_native.width(), native_width);
    EXPECT_EQ(assigned_native.to_decimal_string(), "17");

    DynUInt assigned_wide(native_width, uint64_t{0});
    assigned_wide = DynUInt(wide_width, unsigned_max);
    EXPECT_EQ(assigned_wide.width(), wide_width);
    EXPECT_EQ(assigned_wide.to_decimal_string(), std::to_string(unsigned_max));

    DynSInt signed_native(native_width, DynSInt::NativeSInt{-1});
    DynSInt signed_wide(wide_width, signed_native);
    EXPECT_EQ(signed_wide.to_decimal_string(), "-1");
    EXPECT_EQ(DynSInt(native_width, signed_wide).to_decimal_string(), "-1");
}

TEST(DynInt, bitwise_shift_compare_and_truncate) {
    DynUInt a(29, 0x5F3F4AE);
    DynUInt b(29, 0x4FAA413);
    EXPECT_EQ((a & b).to_hexadecimal_string(), "04f2a402");
    EXPECT_EQ((a | b).to_hexadecimal_string(), "05fbf4bf");
    EXPECT_EQ((a ^ b).to_hexadecimal_string(), "010950bd");
    EXPECT_EQ((a >> 9).to_decimal_string(), "195066");

    DynSInt negative(29, -268435398);
    EXPECT_EQ((negative >> 9).to_decimal_string(), "-524288");
    EXPECT_LT(DynUInt(8, 1), DynUInt(8, 2));
    EXPECT_LT(DynSInt(8, -1), DynSInt(8, 1));
    EXPECT_NE(DynUInt(8, 1), DynUInt(200, 1));
    EXPECT_THROW(static_cast<void>(DynUInt(8, 1) < DynUInt(200, 2)), std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(DynSInt(8, -1) < DynSInt(200, 1)), std::invalid_argument
    );

    EXPECT_EQ(
        DynUInt(200, "0x1234567890ABCDEF").truncate(32).to_hexadecimal_string(), "90abcdef"
    );
    EXPECT_THROW(DynUInt(8, 1).truncate(200), std::invalid_argument);
}

TEST(DynInt, exact_width_arithmetic_is_explicit) {
    DynUInt a(8, 200);
    DynUInt b(8, 100);
    EXPECT_EQ(DynUInt::exact_add(a, b).to_decimal_string(), "44");
    EXPECT_EQ(DynUInt::exact_sub(b, a).to_decimal_string(), "156");
    EXPECT_EQ(DynUInt::exact_mul(a, b).to_decimal_string(), "32");
    EXPECT_THROW(DynUInt::exact_add(a, DynUInt(16, 1)), std::invalid_argument);
}

TEST(DynInt, native_operations_preserve_extension_without_signed_overflow) {
    using NativeUInt = DynUInt::NativeUInt;
    using NativeSInt = DynSInt::NativeSInt;
    constexpr size_t width = DynSInt::sbo_bits - 1;
    constexpr NativeSInt magnitude = NativeSInt{1} << (width - 1);
    constexpr NativeSInt signed_max = magnitude - 1;
    constexpr NativeSInt signed_min = -magnitude;

    auto growing_positive = DynSInt(width, signed_max) + DynSInt(width, NativeSInt{1});
    EXPECT_EQ(growing_positive.width(), DynSInt::sbo_bits);
    EXPECT_EQ(growing_positive.to_decimal_string(), std::to_string(magnitude));

    auto growing_negative = DynSInt(width, signed_min) + DynSInt(width, NativeSInt{-1});
    EXPECT_EQ(growing_negative.width(), DynSInt::sbo_bits);
    EXPECT_EQ(growing_negative.to_decimal_string(), std::to_string(signed_min - 1));

    auto wrapped =
        DynSInt::exact_add(DynSInt(width, signed_max), DynSInt(width, NativeSInt{1}));
    EXPECT_EQ(wrapped.to_decimal_string(), std::to_string(signed_min));

    NativeUInt const unsigned_max = (NativeUInt{1} << width) - 1;
    auto unsigned_wrapped =
        DynUInt::exact_add(DynUInt(width, unsigned_max), DynUInt(width, NativeUInt{1}));
    EXPECT_EQ(unsigned_wrapped.to_decimal_string(), "0");

    auto shifted_sign = DynSInt(width, magnitude >> 1) << 1;
    EXPECT_EQ(shifted_sign.to_decimal_string(), std::to_string(signed_min));
}

TEST(DynInt, exact_width_heap_operations_restore_only_extension_bits) {
    size_t const width = DynSInt::sbo_bits + 1;

    DynSInt signed_max(width, std::numeric_limits<DynUInt::NativeUInt>::max());
    auto signed_wrapped =
        DynSInt::exact_add(signed_max, DynSInt(width, DynSInt::NativeSInt{1}));
    EXPECT_TRUE(signed_wrapped.get_bit(width - 1));
    EXPECT_EQ(signed_wrapped.popcount(), 1u);
    EXPECT_EQ((signed_wrapped >> (width - 1)).to_decimal_string(), "-1");

    DynUInt unsigned_max = ~DynUInt(width);
    auto unsigned_wrapped =
        DynUInt::exact_add(unsigned_max, DynUInt(width, DynUInt::NativeUInt{1}));
    EXPECT_EQ(unsigned_wrapped.to_decimal_string(), "0");
}

TEST(DynInt, saturation_compares_across_the_storage_boundary) {
    using NativeUInt = DynUInt::NativeUInt;
    using NativeSInt = DynSInt::NativeSInt;
    size_t const native_width = DynUInt::sbo_bits;
    size_t const wide_width = native_width + 1;

    DynUInt wide_unsigned(wide_width, std::numeric_limits<NativeUInt>::max());
    EXPECT_EQ(
        wide_unsigned.saturate_unsigned(native_width).to_decimal_string(),
        std::to_string(std::numeric_limits<NativeUInt>::max())
    );

    DynSInt wide_positive(wide_width, std::numeric_limits<NativeUInt>::max());
    EXPECT_EQ(
        wide_positive.saturate_signed(native_width).to_decimal_string(),
        std::to_string(std::numeric_limits<NativeSInt>::max())
    );

    DynSInt wide_min(wide_width, std::numeric_limits<NativeSInt>::min());
    DynSInt below_native_min =
        DynSInt::exact_sub(wide_min, DynSInt(wide_width, NativeSInt{1}));
    EXPECT_EQ(
        below_native_min.saturate_signed(native_width).to_decimal_string(),
        std::to_string(std::numeric_limits<NativeSInt>::min())
    );
}

// Growing arithmetic handles differing widths natively -- that is the whole
// point -- so mismatched operands are ordinary here, not an error.
TEST(DynInt, growing_arithmetic_accepts_mixed_widths) {
    DynUInt a(8, uint64_t{200});
    DynUInt b(16, uint64_t{1000});

    auto sum = a + b;
    EXPECT_EQ(sum.width(), 17u);
    EXPECT_EQ(sum.to_decimal_string(), "1200");

    auto prod = a * b;
    EXPECT_EQ(prod.width(), 24u);
    EXPECT_EQ(prod.to_decimal_string(), "200000");

    // Unsigned subtraction borrows into the extra bit instead of wrapping.
    EXPECT_EQ((a - b).to_decimal_string(true), "-800");

    auto [quotient, remainder] = detail::divrem(a, DynUInt(200, uint64_t{3}));
    EXPECT_EQ(quotient.width(), 9u);
    EXPECT_EQ(remainder.width(), 200u);
    EXPECT_EQ(quotient.to_decimal_string(), "66");
    EXPECT_EQ(remainder.to_decimal_string(), "2");
}

TEST(DynInt, signed_growing_arithmetic) {
    DynSInt neg(8, int8_t{-56});
    DynSInt pos(8, int8_t{100});
    DynSInt neg3(8, int8_t{-3});
    DynSInt pos56(8, int8_t{56});

    EXPECT_EQ((neg + pos).to_decimal_string(true), "44");
    EXPECT_EQ((neg * pos).to_decimal_string(true), "-5600");
    EXPECT_EQ((-neg).to_decimal_string(true), "56");
    EXPECT_EQ(detail::abs(neg).to_decimal_string(true), "56");

    // rem follows the dividend's sign, mod the divisor's.
    EXPECT_EQ((neg % pos).to_decimal_string(true), "-56");
    EXPECT_EQ(detail::mod(neg, pos).to_decimal_string(true), "44");
    EXPECT_EQ((pos56 % neg3).to_decimal_string(true), "2");
    EXPECT_EQ(detail::mod(pos56, neg3).to_decimal_string(true), "-1");

    DynSInt wide_neg(200, int64_t{-17});
    auto [truncated, rem] = detail::divrem(wide_neg, DynSInt(8, int64_t{5}));
    auto [floored, mod] = detail::divmod(wide_neg, DynSInt(8, int64_t{5}));
    EXPECT_EQ(truncated.to_decimal_string(true), "-3");
    EXPECT_EQ(rem.to_decimal_string(true), "-2");
    EXPECT_EQ(floored.to_decimal_string(true), "-4");
    EXPECT_EQ(mod.to_decimal_string(true), "3");

    auto [negative_divisor_q, negative_divisor_r] =
        detail::divrem(DynSInt(200, 17), DynSInt(8, -5));
    EXPECT_EQ(negative_divisor_q.to_decimal_string(true), "-3");
    EXPECT_EQ(negative_divisor_r.to_decimal_string(true), "2");

    auto [both_negative_q, both_negative_r] =
        detail::divrem(DynSInt(200, -17), DynSInt(8, -5));
    EXPECT_EQ(both_negative_q.to_decimal_string(true), "3");
    EXPECT_EQ(both_negative_r.to_decimal_string(true), "-2");

    auto [smaller_q, smaller_r] = detail::divrem(DynSInt(8, -5), DynSInt(200, -17));
    EXPECT_EQ(smaller_q.to_decimal_string(true), "0");
    EXPECT_EQ(smaller_r.to_decimal_string(true), "-5");

    auto [equal_q, equal_r] = detail::divrem(DynSInt(8, -5), DynSInt(200, -5));
    EXPECT_EQ(equal_q.to_decimal_string(true), "1");
    EXPECT_EQ(equal_r.to_decimal_string(true), "0");

    // The extra quotient bit keeps signed_min / -1 representable.
    DynSInt min_val(8, int8_t{-128});
    DynSInt minus_one(8, int8_t{-1});
    EXPECT_EQ((min_val / minus_one).to_decimal_string(true), "128");
}

TEST(DynInt, signed_and_unsigned_have_distinct_values) {
    static_assert(!std::is_same_v<DynUInt, DynSInt>);
    static_assert(sizeof(DynUInt) == sizeof(DynSInt));

    DynUInt unsigned_negative_pattern(9, 0x1FF);
    DynSInt signed_negative_pattern(9, -1);
    EXPECT_EQ(unsigned_negative_pattern.to_decimal_string(), "511");
    EXPECT_EQ(signed_negative_pattern.to_decimal_string(), "-1");

    DynUInt wide_unsigned(129, DynSInt(129, -1));
    DynSInt wide_signed(129, -1);
    EXPECT_EQ(wide_unsigned, ~DynUInt(129));
    EXPECT_EQ(wide_signed.to_decimal_string(), "-1");

    DynUInt converted_wide_unsigned(129, DynUInt(8, 0xFF));
    DynSInt converted_wide_signed(129, DynSInt(8, -1));
    EXPECT_EQ(converted_wide_unsigned.to_decimal_string(), "255");
    EXPECT_EQ(converted_wide_signed.to_decimal_string(), "-1");

    DynSInt sign_bit(9, 0);
    sign_bit.set_bit(8, true);
    EXPECT_EQ(sign_bit.to_decimal_string(), "-256");
    sign_bit.set_bit(8, false);
    EXPECT_EQ(sign_bit.to_decimal_string(), "0");
    EXPECT_THROW(sign_bit.set_bit(9, true), std::out_of_range);
}

TEST(DynInt, native_static_and_parsed_construction_preserve_values) {
    DynUInt native_unsigned(9, 0x1FF);
    DynSInt native_signed(9, -1);
    EXPECT_EQ(native_unsigned.to_decimal_string(), "511");
    EXPECT_EQ(native_signed.to_decimal_string(), "-1");

    DynUInt two_word_unsigned(65, ~uint64_t{0});
    DynSInt two_word_signed(65, -1);
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

    DynUInt parsed_unsigned(9, "511");
    DynSInt parsed_signed(9, "-1");
    EXPECT_EQ(parsed_unsigned.to_decimal_string(), "511");
    EXPECT_EQ(parsed_signed.to_decimal_string(), "-1");

    // Exercise the heap-storage assignment path with a narrow signed source.
    DynSInt from_narrow_signed(200, int8_t{-1});
    EXPECT_EQ(from_narrow_signed.to_decimal_string(), "-1");
}

TEST(DynInt, runtime_formatting_and_error_paths) {
    DynUInt value(16, uint16_t{0xABCD});
    EXPECT_EQ(value.to_binary_string(), "1010101111001101");
    EXPECT_EQ(value.to_octal_string(), "125715");
    EXPECT_THROW(value.get_bit(16), std::out_of_range);
    EXPECT_THROW(value.set_bit(16, true), std::out_of_range);

    EXPECT_EQ(DynUInt(8, "+42").to_decimal_string(), "42");
    EXPECT_EQ(DynUInt(8, "0X_Af").to_decimal_string(), "175");
    EXPECT_THROW(DynSInt(8, "-0x1"), std::invalid_argument);
    EXPECT_THROW(DynUInt(8, "0xgg"), std::invalid_argument);
    EXPECT_THROW(DynUInt(64, "12x3"), std::invalid_argument);

    DynUInt null_value(0);
    EXPECT_EQ(null_value.to_binary_string(), "");
    EXPECT_EQ(null_value.to_decimal_string(), "");
    EXPECT_EQ(null_value.to_hexadecimal_string(), "");
    EXPECT_EQ(null_value.to_octal_string(), "");
    EXPECT_THROW(null_value.to_native_integer<uint8_t>(), std::domain_error);
    EXPECT_THROW(DynUInt(0, uint64_t{1}), std::invalid_argument);

    DynUInt dividend(200, uint64_t{5});
    DynUInt zero(200);
    EXPECT_THROW(dividend / zero, std::domain_error);
    EXPECT_THROW(dividend % zero, std::domain_error);
    EXPECT_EQ(dividend / DynUInt(200, uint64_t{7}), DynUInt(201, uint64_t{0}));
    EXPECT_EQ(dividend / dividend, DynUInt(201, uint64_t{1}));

    EXPECT_EQ((dividend << 200).to_decimal_string(), "0");
    EXPECT_EQ((dividend >> 200).to_decimal_string(), "0");
    EXPECT_EQ((DynSInt(200, -1) >> 200).to_decimal_string(), "-1");

    EXPECT_EQ(DynUInt(200, uint64_t{42}).saturate_unsigned(8).to_decimal_string(), "42");
    EXPECT_EQ(DynSInt(200, int64_t{42}).saturate_signed(8).to_decimal_string(), "42");
    EXPECT_EQ(DynUInt(8, uint64_t{42}).saturate_unsigned(16).to_decimal_string(), "42");
    EXPECT_EQ(DynSInt(8, int64_t{42}).saturate_signed(16).to_decimal_string(), "42");
    EXPECT_EQ(DynUInt(8, uint64_t{42}).saturate_unsigned(0).width(), 0u);
    EXPECT_EQ(DynSInt(8, int64_t{42}).saturate_signed(0).width(), 0u);

    EXPECT_EQ(DynUInt(200, uint64_t{255}).to_native_integer<uint8_t>(), 255);
    EXPECT_EQ(DynSInt(200, int64_t{-128}).to_native_integer<int8_t>(), -128);
    EXPECT_THROW(
        DynUInt(200, uint64_t{256}).to_native_integer<uint8_t>(), std::out_of_range
    );
    EXPECT_THROW(DynSInt(200, int64_t{-1}).to_native_integer<uint8_t>(), std::out_of_range);
}

TEST(DynInt, growing_arithmetic_preserves_the_result_invariant) {
    auto unsigned_sum = DynUInt(8, 200) + DynUInt(8, 100);
    auto unsigned_difference = DynUInt(8, 5) - DynUInt(8, 7);
    auto signed_sum = DynSInt(8, -56) + DynSInt(8, 100);
    auto signed_product = DynSInt(8, -3) * DynSInt(8, 7);

    EXPECT_EQ(unsigned_sum.width(), 9);
    EXPECT_EQ(unsigned_sum.to_decimal_string(), "300");
    EXPECT_EQ(unsigned_difference.to_decimal_string(), "-2");
    EXPECT_EQ(signed_sum.to_decimal_string(), "44");
    EXPECT_EQ(signed_product.to_decimal_string(), "-21");

    auto [quotient, remainder] = detail::divrem(DynSInt(8, -17), DynSInt(8, 5));
    EXPECT_EQ(quotient.to_decimal_string(), "-3");
    EXPECT_EQ(remainder.to_decimal_string(), "-2");
}

TEST(DynSigned, remainder_and_modulo_are_distinct) {
    DynSigned negative(8, -17);
    DynSigned positive(4, 5);
    DynSigned negative_divisor(4, -5);

    EXPECT_EQ(static_cast<long long>(negative % positive), -2);
    EXPECT_EQ(static_cast<long long>(detail::rem(negative, positive)), -2);
    EXPECT_EQ(static_cast<long long>(detail::mod(negative, positive)), 3);

    EXPECT_EQ(static_cast<long long>(DynSigned(8, 17) % negative_divisor), 2);
    EXPECT_EQ(static_cast<long long>(detail::rem(DynSigned(8, 17), negative_divisor)), 2);
    EXPECT_EQ(static_cast<long long>(detail::mod(DynSigned(8, 17), negative_divisor)), -3);

    negative %= positive;
    EXPECT_EQ(static_cast<long long>(negative), -2);
    EXPECT_EQ(negative.width(), 8u);

    EXPECT_THROW(
        static_cast<void>(detail::rem(negative, DynSigned(4, 0))), std::domain_error
    );
    EXPECT_THROW(
        static_cast<void>(detail::mod(negative, DynSigned(4, 0))), std::domain_error
    );
}

// LCOV_EXCL_BR_STOP

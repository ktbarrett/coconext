// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types/dyn_int_base.hpp>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

using coconext::types::detail::DynSInt;
using coconext::types::detail::DynUInt;
namespace detail = coconext::types::detail;

TEST(DynInt, runtime_width_storage_and_formatting) {
    static_assert(DynUInt::sbo_bits == 64);
    static_assert(DynSInt::sbo_bits == 64);

    DynUInt native_value(64, uint64_t{12345});
    DynUInt heap_value(65, uint64_t{12345});
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
    EXPECT_EQ(DynUInt(65, 1).count_leading_zeros(), 64u);
}

TEST(DynInt, native_tier_handles_full_64_bit_values) {
    constexpr uint64_t unsigned_max = std::numeric_limits<uint64_t>::max();
    constexpr int64_t signed_min = std::numeric_limits<int64_t>::min();

    DynUInt u(64, unsigned_max);
    EXPECT_EQ(u.raw().word(0), unsigned_max);
    EXPECT_EQ(u.to_decimal_string(), "18446744073709551615");
    EXPECT_EQ(u.to_hexadecimal_string(), "ffffffffffffffff");
    EXPECT_EQ((u >> 63).to_decimal_string(), "1");
    EXPECT_EQ(DynUInt::exact_add(u, DynUInt(64, uint64_t{1})).to_decimal_string(), "0");

    DynSInt s(64, signed_min);
    EXPECT_EQ(s.raw().word(0), uint64_t{1} << 63);
    EXPECT_EQ(s.to_decimal_string(), "-9223372036854775808");
    EXPECT_EQ((s >> 63).to_decimal_string(), "-1");
    EXPECT_LT(s, DynSInt(64, int64_t{-1}));
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
    DynUInt native(64, std::numeric_limits<uint64_t>::max());
    DynUInt wide(65, native);
    EXPECT_EQ(wide.raw().word(0), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(wide.raw().word(1), 0u);

    DynUInt wide_copy(wide);
    DynUInt wide_move(std::move(wide_copy));
    EXPECT_EQ(wide_move.to_decimal_string(), "18446744073709551615");

    DynUInt native_copy(64, wide_move);
    DynUInt native_move(std::move(native_copy));
    EXPECT_EQ(native_move.to_decimal_string(), "18446744073709551615");

    DynUInt assigned_native(65, uint64_t{0});
    assigned_native = DynUInt(64, uint64_t{17});
    EXPECT_EQ(assigned_native.width(), 64u);
    EXPECT_EQ(assigned_native.to_decimal_string(), "17");

    DynUInt assigned_wide(64, uint64_t{0});
    assigned_wide = DynUInt(65, std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(assigned_wide.width(), 65u);
    EXPECT_EQ(assigned_wide.to_decimal_string(), "18446744073709551615");

    DynSInt signed_native(64, int64_t{-1});
    DynSInt signed_wide(65, signed_native);
    EXPECT_EQ(signed_wide.raw().word(1), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(DynSInt(64, signed_wide).to_decimal_string(), "-1");
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

TEST(DynInt, signed_and_unsigned_are_distinct_canonical_representations) {
    static_assert(!std::is_same_v<DynUInt, DynSInt>);
    static_assert(sizeof(DynUInt) == sizeof(DynSInt));

    DynUInt unsigned_negative_pattern(9, 0x1FF);
    DynSInt signed_negative_pattern(9, -1);
    EXPECT_EQ(unsigned_negative_pattern.raw().word(0), 0x1FF);
    EXPECT_EQ(signed_negative_pattern.raw().word(0), ~detail::Word{0});

    DynUInt wide_unsigned(129, DynSInt(129, -1));
    DynSInt wide_signed(129, -1);
    EXPECT_EQ(wide_unsigned.raw().word(2), 1);
    EXPECT_EQ(wide_signed.raw().word(2), ~detail::Word{0});

    DynUInt converted_wide_unsigned(129, DynUInt(8, 0xFF));
    DynSInt converted_wide_signed(129, DynSInt(8, -1));
    EXPECT_EQ(converted_wide_unsigned.raw().word(2), 0);
    EXPECT_EQ(converted_wide_signed.raw().word(2), ~detail::Word{0});

    DynSInt sign_bit(9, 0);
    sign_bit.set_bit(8, true);
    EXPECT_EQ(sign_bit.raw().word(0), ~detail::Word{0} << 8);
    sign_bit.set_bit(8, false);
    EXPECT_EQ(sign_bit.raw().word(0), 0);
    EXPECT_THROW(sign_bit.set_bit(9, true), std::out_of_range);
}

TEST(DynInt, native_static_and_parsed_construction_preserve_extension) {
    DynUInt native_unsigned(9, 0x1FF);
    DynSInt native_signed(9, -1);
    EXPECT_EQ(native_unsigned.raw().word(0), 0x1FF);
    EXPECT_EQ(native_signed.raw().word(0), ~detail::Word{0});

    DynUInt two_word_unsigned(65, ~uint64_t{0});
    DynSInt two_word_signed(65, -1);
    EXPECT_EQ(two_word_unsigned.raw().word(1), 0);
    EXPECT_EQ(two_word_signed.raw().word(1), ~detail::Word{0});

    DynUInt from_native_static(detail::UInt<9>(0x1FF));
    DynSInt from_native_static_signed(detail::SInt<9>(-1));
    DynUInt from_wide_static(detail::UInt<129>(detail::SInt<129>(-1)));
    DynSInt from_wide_static_signed(detail::SInt<129>(-1));
    EXPECT_EQ(from_native_static.raw().word(0), 0x1FF);
    EXPECT_EQ(from_native_static_signed.raw().word(0), ~detail::Word{0});
    EXPECT_EQ(from_wide_static.raw().word(2), 1);
    EXPECT_EQ(from_wide_static_signed.raw().word(2), ~detail::Word{0});

    DynUInt parsed_unsigned(9, "511");
    DynSInt parsed_signed(9, "-1");
    EXPECT_EQ(parsed_unsigned.raw().word(0), 0x1FF);
    EXPECT_EQ(parsed_signed.raw().word(0), ~detail::Word{0});
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

// LCOV_EXCL_BR_STOP

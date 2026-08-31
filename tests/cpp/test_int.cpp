// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <array>
#include <coconext/types.hpp>
#include <type_traits>

using namespace coconext::types;
using namespace coconext::literals;

// ---------------------------------------------------------------------------
// Integer representation and growing arithmetic.
// ---------------------------------------------------------------------------

TEST(Int, storage_tiers_and_formatting) {
    static_assert(sizeof(detail::UInt<8>) == 1);
    static_assert(sizeof(detail::UInt<16>) == 2);
    static_assert(sizeof(detail::UInt<32>) == 4);
    static_assert(sizeof(detail::UInt<64>) == 8);
    static_assert(std::is_same_v<detail::UInt<8>::RawType, uint8_t>);
    static_assert(std::is_same_v<detail::UInt<64>::RawType, uint64_t>);
    static_assert(std::is_same_v<detail::UInt<200>::RawType, detail::WordConstSpan>);

    detail::UInt<200> wide("0x1234567890ABCDEF1122334455667788AABBCCDD");
    EXPECT_EQ(
        wide.to_hexadecimal_string(), "00000000001234567890abcdef1122334455667788aabbccdd"
    );
    EXPECT_EQ(wide.to_decimal_string(), "103929005307927756724023193881144724129310297309");
    EXPECT_EQ(detail::SInt<200>("-5").to_decimal_string(), "-5");
    EXPECT_THROW(detail::UInt<8>("999"), std::out_of_range);
}

TEST(Int, native_value_fit_predicate) {
    static_assert(detail::native_value_fits<4, false>(0));
    static_assert(detail::native_value_fits<4, false>(15));
    static_assert(!detail::native_value_fits<4, false>(-1));
    static_assert(!detail::native_value_fits<4, false>(16));

    static_assert(detail::native_value_fits<4, true>(-8));
    static_assert(detail::native_value_fits<4, true>(7));
    static_assert(!detail::native_value_fits<4, true>(-9));
    static_assert(!detail::native_value_fits<4, true>(8));
    static_assert(!detail::native_value_fits<4, true>(uint8_t{8}));

    static_assert(detail::native_value_fits<65, false>(~uint64_t{0}));
    static_assert(detail::native_value_fits<65, true>(std::numeric_limits<int64_t>::min()));

#if defined(__SIZEOF_INT128__)
    static_assert(detail::native_value_fits<100, false>((__uint128_t{1} << 100) - 1));
    static_assert(!detail::native_value_fits<100, false>(__uint128_t{1} << 100));
    static_assert(detail::native_value_fits<100, true>(-(__int128_t{1} << 99)));
    static_assert(!detail::native_value_fits<100, true>(__int128_t{1} << 99));
#endif
}

TEST(Int, packed_bit_operations) {
    detail::UInt<12> value(0);
    value[0] = '1'_b;
    value[3] = '1'_b;
    value[11] = '1'_b;
    EXPECT_EQ(value, detail::UInt<12>(0b100000001001));

    detail::UInt<12> copied(0);
    copied[5] = value[11];
    EXPECT_EQ(copied, detail::UInt<12>(1u << 5));

    detail::UInt<12> mask(0x555);
    EXPECT_EQ(~mask, detail::UInt<12>(0xAAA));
    EXPECT_EQ((value & mask).to_hexadecimal_string(), "001");
    EXPECT_EQ((value | mask).to_hexadecimal_string(), "d5d");
    EXPECT_EQ((value ^ mask).to_hexadecimal_string(), "d5c");

    EXPECT_EQ(value.popcount(), 3u);
    EXPECT_EQ(value.count_leading_zeros(), 0u);
    EXPECT_EQ(value.count_trailing_zeros(), 0u);

    EXPECT_EQ(detail::SInt<12>(-1).popcount(), 12u);
    EXPECT_EQ(detail::SInt<12>(-1).count_leading_zeros(), 0u);
    EXPECT_EQ(detail::UInt<65>(1).count_leading_zeros(), 64u);

    static_assert(!detail::SInt<0>{}.is_negative());
    static_assert(!detail::SInt<12>(0).is_negative());
    static_assert(detail::SInt<12>(-1).is_negative());
    static_assert(!detail::SInt<200>(1).is_negative());
    static_assert(detail::SInt<200>(-1).is_negative());
}

TEST(Int, shifts_and_comparisons_follow_the_representation) {
    detail::UInt<29> logical(0x5F3F4AE);
    EXPECT_EQ(logical >> 9, detail::UInt<29>(0x2F9FA));
    EXPECT_GT(logical, detail::UInt<29>(1));

    detail::SInt<29> negative(-268435398);
    EXPECT_EQ(negative >> 9, detail::SInt<29>(-524288));
    EXPECT_LT(negative, detail::SInt<29>(1));

    static_assert(std::totally_ordered<detail::UInt<8>>);
    static_assert(std::totally_ordered<detail::SInt<8>>);
    static_assert(!std::equality_comparable_with<detail::UInt<8>, detail::UInt<200>>);
    static_assert(!std::three_way_comparable_with<detail::SInt<8>, detail::SInt<200>>);
}

TEST(Int, exact_width_arithmetic_is_explicit) {
    constexpr detail::UInt<8> a(200);
    constexpr detail::UInt<8> b(100);
    static_assert(detail::UInt<8>::exact_add(a, b) == detail::UInt<8>(44));
    static_assert(detail::UInt<8>::exact_sub(b, a) == detail::UInt<8>(156));
    static_assert(detail::UInt<8>::exact_mul(a, b) == detail::UInt<8>(32));

    constexpr detail::SInt<9> maximum(255);
    constexpr detail::SInt<9> minimum(-256);
    constexpr detail::SInt<9> one(1);
    static_assert(detail::SInt<9>::exact_add(maximum, one).raw() == uint16_t{0xFF00});
    static_assert(detail::SInt<9>::exact_sub(minimum, one).raw() == uint16_t{0x00FF});
    static_assert(
        detail::SInt<9>::exact_mul(detail::SInt<9>(-2), detail::SInt<9>(3)).raw()
        == uint16_t{0xFFFA}
    );

    detail::SInt<129> wide_negative(-2);
    auto wide_product = detail::SInt<129>::exact_mul(wide_negative, detail::SInt<129>(3));
    EXPECT_EQ(wide_product, detail::SInt<129>(-6));
    EXPECT_EQ(wide_product.raw().word(2), ~detail::Word{0});
}

TEST(IntKernel, multiply_overwrites_the_complete_destination) {
    std::array<detail::Word, 4> destination{
        ~detail::Word{0}, ~detail::Word{0}, ~detail::Word{0}, ~detail::Word{0}
    };
    std::array<detail::Word, 2> lhs{~detail::Word{0}, 1};
    std::array<detail::Word, 2> rhs{2, 3};
    detail::multiply_unsigned(
        detail::WordSpan{destination, 256},
        detail::WordConstSpan{lhs, 128},
        detail::WordConstSpan{rhs, 128}
    );
    EXPECT_EQ(destination, (std::array<detail::Word, 4>{~detail::Word{1}, 0, 6, 0}));

    destination.fill(~detail::Word{0});
    std::array<detail::Word, 0> empty_rhs{};
    detail::multiply_unsigned(
        detail::WordSpan{destination, 256},
        detail::WordConstSpan{lhs, 128},
        detail::WordConstSpan{empty_rhs, 0}
    );
    EXPECT_EQ(destination, (std::array<detail::Word, 4>{0, 0, 0, 0}));
}

TEST(IntNative, scalar_tiers_cover_the_complete_native_operation) {
    constexpr detail::UInt<16> maximum(uint16_t{0xFFFF});
    static_assert(
        detail::UInt<16>::exact_mul(maximum, maximum) == detail::UInt<16>(uint16_t{1})
    );

    constexpr detail::SInt<32> minimum(std::numeric_limits<int32_t>::min());
    constexpr auto quotient = minimum / detail::SInt<32>(-1);
    static_assert(std::is_same_v<std::remove_cv_t<decltype(quotient)>, detail::SInt<33>>);
    static_assert(quotient.raw() == uint64_t{0x80000000});

    static_assert(detail::SInt<65>(detail::SInt<8>(-1)) < detail::SInt<65>(1));
    static_assert(detail::UInt<9>("0x1ff") == detail::UInt<9>(511));

#if defined(__SIZEOF_INT128__)
    constexpr auto product = detail::UInt<64>(~uint64_t{0}) * detail::UInt<64>(uint64_t{2});
    static_assert(!decltype(product)::is_wide);
    static_assert(product.raw() == __uint128_t{~uint64_t{0}} * 2);
#endif
}

TEST(IntGrowing, additive_grows_by_one_bit) {
    detail::UInt<8> a(uint8_t{200});
    detail::UInt<8> b(uint8_t{100});

    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), detail::UInt<9>>);
    EXPECT_EQ(sum.to_decimal_string(), "300");  // does not wrap at 8 bits

    // Unequal widths grow off the wider operand.
    detail::UInt<16> c(uint16_t{1000});
    auto mixed = a + c;
    static_assert(std::is_same_v<decltype(mixed), detail::UInt<17>>);
    EXPECT_EQ(mixed.to_decimal_string(), "1200");

    // Unsigned subtraction borrows into the extra bit rather than wrapping.
    EXPECT_EQ((b - a).to_decimal_string(), "-100");
    EXPECT_EQ((a - b).to_decimal_string(), "100");
}

TEST(IntGrowing, additive_signed_uses_canonical_storage) {
    detail::SInt<8> neg(int8_t{-56});
    detail::SInt<8> pos(int8_t{100});

    EXPECT_EQ((neg + pos).to_decimal_string(true), "44");
    EXPECT_EQ((neg - pos).to_decimal_string(true), "-156");
}

TEST(IntGrowing, multiply_sums_the_widths) {
    detail::UInt<8> a(uint8_t{200});
    detail::UInt<8> b(uint8_t{100});

    auto p = a * b;
    static_assert(std::is_same_v<decltype(p), detail::UInt<16>>);
    EXPECT_EQ(p.to_decimal_string(), "20000");

    // -56 * 100 == -5600
    EXPECT_EQ((detail::SInt<8>(-56) * detail::SInt<8>(100)).to_decimal_string(), "-5600");
}

TEST(IntGrowing, division_quotient_grows_by_one) {
    detail::UInt<8> a(uint8_t{200});
    detail::UInt<8> b(uint8_t{7});

    auto q = a / b;
    static_assert(std::is_same_v<decltype(q), detail::UInt<9>>);
    EXPECT_EQ(q.to_decimal_string(), "28");

    auto r = a % b;
    static_assert(std::is_same_v<decltype(r), detail::UInt<8>>);
    EXPECT_EQ(r.to_decimal_string(), "4");
}

TEST(IntGrowing, mixed_width_division) {
    detail::UInt<8> dividend(uint8_t{200});
    detail::UInt<200> divisor(uint8_t{3});
    auto [quotient, remainder] = detail::divrem(dividend, divisor);
    EXPECT_EQ(quotient.to_decimal_string(), "66");
    EXPECT_EQ(remainder.to_decimal_string(), "2");

    detail::SInt<200> negative(int8_t{-17});
    detail::SInt<8> five(uint8_t{5});
    auto [truncated, rem] = detail::divrem(negative, five);
    auto [floored, mod] = detail::divmod(negative, five);
    EXPECT_EQ(truncated.to_decimal_string(true), "-3");
    EXPECT_EQ(rem.to_decimal_string(true), "-2");
    EXPECT_EQ(floored.to_decimal_string(true), "-4");
    EXPECT_EQ(mod.to_decimal_string(true), "3");

    auto [narrow_negative_q, narrow_negative_r] =
        detail::divrem(detail::SInt<8>(-17), detail::SInt<200>(5));
    EXPECT_EQ(narrow_negative_q.to_decimal_string(true), "-3");
    EXPECT_EQ(narrow_negative_r.to_decimal_string(true), "-2");

    auto [negative_divisor_q, negative_divisor_r] =
        detail::divrem(detail::SInt<200>(17), detail::SInt<8>(-5));
    EXPECT_EQ(negative_divisor_q.to_decimal_string(true), "-3");
    EXPECT_EQ(negative_divisor_r.to_decimal_string(true), "2");

    auto [both_negative_q, both_negative_r] =
        detail::divrem(detail::SInt<200>(-17), detail::SInt<8>(-5));
    EXPECT_EQ(both_negative_q.to_decimal_string(true), "3");
    EXPECT_EQ(both_negative_r.to_decimal_string(true), "-2");

    auto [smaller_q, smaller_r] =
        detail::divrem(detail::SInt<8>(-5), detail::SInt<200>(-17));
    EXPECT_EQ(smaller_q.to_decimal_string(true), "0");
    EXPECT_EQ(smaller_r.to_decimal_string(true), "-5");

    auto [equal_q, equal_r] = detail::divrem(detail::SInt<8>(-5), detail::SInt<200>(-5));
    EXPECT_EQ(equal_q.to_decimal_string(true), "1");
    EXPECT_EQ(equal_r.to_decimal_string(true), "0");
}

// The extra quotient bit exists so signed_min / -1 stays representable.
TEST(IntGrowing, signed_min_over_minus_one_does_not_overflow) {
    detail::SInt<8> min_val(int8_t{-128});
    detail::SInt<8> minus_one(int8_t{-1});

    EXPECT_EQ((min_val / minus_one).to_decimal_string(true), "128");
}

// rem follows the dividend's sign (C), mod follows the divisor's (VHDL/Python).
TEST(IntGrowing, rem_and_mod_differ_on_mixed_signs) {
    detail::SInt<8> neg56(int8_t{-56});
    detail::SInt<8> pos100(int8_t{100});
    detail::SInt<8> neg3(int8_t{-3});
    detail::SInt<8> pos56(int8_t{56});

    EXPECT_EQ((neg56 % pos100).to_decimal_string(true), "-56");
    EXPECT_EQ(detail::mod(neg56, pos100).to_decimal_string(true), "44");

    EXPECT_EQ((pos56 % neg3).to_decimal_string(true), "2");
    EXPECT_EQ(detail::mod(pos56, neg3).to_decimal_string(true), "-1");

    // Same signs: rem and mod agree.
    EXPECT_EQ((neg56 % neg3).to_decimal_string(true), "-2");
    EXPECT_EQ(detail::mod(neg56, neg3).to_decimal_string(true), "-2");
}

TEST(IntGrowing, unary_negate_and_abs_grow_by_one) {
    detail::SInt<8> neg56(int8_t{-56});

    auto n = -neg56;
    static_assert(std::is_same_v<decltype(n), detail::SInt<9>>);
    EXPECT_EQ(n.to_decimal_string(true), "56");
    EXPECT_EQ(detail::abs(neg56).to_decimal_string(true), "56");

    // The growth is what makes abs(signed_min) representable.
    detail::SInt<8> min_val(int8_t{-128});
    EXPECT_EQ(detail::abs(min_val).to_decimal_string(true), "128");
}

TEST(IntGrowing, wide_operands) {
    detail::UInt<200> a("0x1234567890ABCDEF1122334455667788AABBCCDD");
    detail::UInt<104> b("0xFEDCBA98765432100123456789");

    EXPECT_EQ(
        (a + b).to_decimal_string(), "103929005307927776916288754849918835164314678374"
    );
    EXPECT_EQ(
        (a * b).to_decimal_string(),
        "20985620746650105667944919267254862992694182466051081766005088452816326800540"
        "85"
    );
    EXPECT_EQ((a / b).to_decimal_string(), "5146971002046463");
    EXPECT_EQ((a % b).to_decimal_string(), "20112278405973339191843622874214");
}

// Growing ops on the null vector produce a real (if zero) value rather than
// being a compile error: the result width is genuinely non-zero.
TEST(IntGrowing, zero_width_operands) {
    detail::UInt<0> n{};
    detail::UInt<8> a(uint8_t{42});

    EXPECT_EQ((n + n).to_decimal_string(), "0");
    static_assert(std::is_same_v<decltype(n + n), detail::UInt<1>>);

    auto p = n * a;
    static_assert(std::is_same_v<decltype(p), detail::UInt<8>>);
    EXPECT_EQ(p.to_decimal_string(), "0");

    // A null divisor is a zero divisor.
    EXPECT_THROW((a / n), std::domain_error);
}

TEST(IntGrowing, usable_in_constant_expressions) {
    constexpr detail::UInt<8> a(uint8_t{200});
    constexpr detail::UInt<8> b(uint8_t{7});
    constexpr auto q = a / b;
    constexpr auto r = a % b;
    static_assert(a + b == detail::UInt<9>(uint16_t{207}));
    static_assert(a * b == detail::UInt<16>(uint16_t{1400}));
    static_assert(q == detail::UInt<9>(uint16_t{28}));
    static_assert(r == detail::UInt<8>(uint8_t{4}));
    SUCCEED();
}

TEST(Int, signed_and_unsigned_are_distinct_canonical_representations) {
    static_assert(!std::is_same_v<detail::UInt<9>, detail::SInt<9>>);
    static_assert(sizeof(detail::UInt<9>) == sizeof(detail::SInt<9>));
    static_assert(sizeof(detail::UInt<129>) == sizeof(detail::SInt<129>));

    constexpr detail::UInt<9> unsigned_negative_pattern(0x1FF);
    constexpr detail::SInt<9> signed_negative_pattern(-1);
    static_assert(unsigned_negative_pattern.raw() == uint16_t{0x01FF});
    static_assert(signed_negative_pattern.raw() == uint16_t{0xFFFF});

    constexpr detail::UInt<9> widened_unsigned(detail::UInt<8>(0xFF));
    constexpr detail::SInt<9> widened_signed(detail::SInt<8>(-1));
    static_assert(widened_unsigned.raw() == uint16_t{0x00FF});
    static_assert(widened_signed.raw() == uint16_t{0xFFFF});

    detail::UInt<129> wide_unsigned(detail::SInt<129>(-1));
    detail::SInt<129> wide_signed(-1);
    EXPECT_EQ(wide_unsigned.raw().word(2), 1);
    EXPECT_EQ(wide_signed.raw().word(2), ~detail::Word{0});

    detail::UInt<129> converted_wide_unsigned(detail::UInt<8>(0xFF));
    detail::SInt<129> converted_wide_signed(detail::SInt<8>(-1));
    EXPECT_EQ(converted_wide_unsigned.raw().word(2), 0);
    EXPECT_EQ(converted_wide_signed.raw().word(2), ~detail::Word{0});

    detail::SInt<9> sign_bit(0);
    sign_bit.set_bit(8, true);
    EXPECT_EQ(sign_bit.raw(), uint16_t{0xFF00});
    sign_bit.set_bit(8, false);
    EXPECT_EQ(sign_bit.raw(), uint16_t{0});
    EXPECT_THROW(sign_bit.set_bit(9, true), std::out_of_range);
}

TEST(Int, conversion_and_parsing_preserve_physical_extension) {
    constexpr detail::UInt<9> narrowed_unsigned(detail::UInt<16>(0xFFFF));
    constexpr detail::UInt<9> narrowed_signed(detail::SInt<16>(-1));
    constexpr detail::SInt<9> reinterpreted_unsigned(detail::UInt<9>(0x1FF));
    static_assert(narrowed_unsigned.raw() == uint16_t{0x01FF});
    static_assert(narrowed_signed.raw() == uint16_t{0x01FF});
    static_assert(reinterpreted_unsigned.raw() == uint16_t{0xFFFF});

    constexpr detail::UInt<9> parsed_unsigned("511");
    constexpr detail::SInt<9> parsed_signed("-1");
    static_assert(parsed_unsigned.raw() == uint16_t{0x01FF});
    static_assert(parsed_signed.raw() == uint16_t{0xFFFF});
}

TEST(Int, native_division_results_are_canonical) {
    constexpr auto unsigned_result =
        detail::divrem(detail::UInt<8>(200), detail::UInt<8>(7));
    static_assert(unsigned_result.first.raw() == uint16_t{28});
    static_assert(unsigned_result.second.raw() == uint8_t{4});

    constexpr auto signed_result = detail::divrem(detail::SInt<8>(-17), detail::SInt<8>(5));
    static_assert(signed_result.first.raw() == uint16_t{0xFFFD});
    static_assert(signed_result.second.raw() == uint8_t{0xFE});

    constexpr auto wide_result =
        detail::divrem(detail::SInt<129>(-17), detail::SInt<8>(-5));
    static_assert(wide_result.first == detail::SInt<130>(3));
    static_assert(wide_result.second == detail::SInt<8>(-2));
}

TEST(Int, growing_arithmetic_preserves_the_result_invariant) {
    constexpr auto unsigned_sum = detail::UInt<8>(200) + detail::UInt<8>(100);
    constexpr auto unsigned_difference = detail::UInt<8>(5) - detail::UInt<8>(7);
    constexpr auto signed_sum = detail::SInt<8>(-56) + detail::SInt<8>(100);
    constexpr auto signed_product = detail::SInt<8>(-3) * detail::SInt<8>(7);

    static_assert(
        std::is_same_v<std::remove_cv_t<decltype(unsigned_sum)>, detail::UInt<9>>
    );
    static_assert(
        std::is_same_v<std::remove_cv_t<decltype(unsigned_difference)>, detail::SInt<9>>
    );
    static_assert(std::is_same_v<std::remove_cv_t<decltype(signed_sum)>, detail::SInt<9>>);
    static_assert(
        std::is_same_v<std::remove_cv_t<decltype(signed_product)>, detail::SInt<16>>
    );

    static_assert(unsigned_sum.raw() == uint16_t{300});
    static_assert(unsigned_difference.raw() == uint16_t{0xFFFE});
    static_assert(signed_sum.raw() == uint16_t{44});
    static_assert(signed_product.raw() == uint16_t{0xFFEB});

    auto [quotient, remainder] = detail::divrem(detail::SInt<8>(-17), detail::SInt<8>(5));
    EXPECT_EQ(quotient.to_decimal_string(), "-3");
    EXPECT_EQ(remainder.to_decimal_string(), "-2");
}

// LCOV_EXCL_BR_STOP

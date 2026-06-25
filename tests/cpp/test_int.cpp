// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <type_traits>

using namespace coconext::types;

#ifdef COCONEXT_USE_APINT

// Test detail::Bits with APInt

#else

#if defined(__SIZEOF_INT128__)

TEST(TestBits, JustAbove128) {
    static_assert(std::is_same_v<detail::Bits<127>::IntType, __uint128_t>);
    static_assert(std::is_same_v<detail::Bits<128>::IntType, __uint128_t>);
    static_assert(std::is_same_v<detail::Bits<129>::IntType, detail::BigInt<129>>);
}

TEST(TestBits, single_word_constructor_supports_128) {
    detail::Bits<111> c(-1);
    __uint128_t exp_c_raw = (((__uint128_t)1) << 111) - 1;
    detail::Bits<111> expected_c(exp_c_raw);
    EXPECT_EQ(c, expected_c);

    detail::Bits<127> a0(-1);
    detail::Bits<127> a1 = a0;
    detail::Bits<127> a2(a1);
    detail::Bits<127> a = std::move(a2);
    __uint128_t exp_a_raw = (((__uint128_t)1) << 127) - 1;
    detail::Bits<127> expected_a(exp_a_raw);
    EXPECT_EQ(a, expected_a);

    detail::Bits<128> d(-1);
    __uint128_t exp_d_raw = ~((__uint128_t)0);
    detail::Bits<128> expected_d(exp_d_raw);
    EXPECT_EQ(d, expected_d);
}

TEST(TestBits, not_op_supports_128) {
    detail::Bits<121> a(0);
    __uint128_t exp_not_a_raw =
        ((__uint128_t)0x1FFFFFFFFFFFFFFULL << 64) | 0xFFFFFFFFFFFFFFFFULL;
    detail::Bits<121> expected_not_a(exp_not_a_raw);
    EXPECT_EQ(~a, expected_not_a);

    detail::Bits<128> b(0xAAAFFEULL);
    __uint128_t exp_not_b_raw = ((__uint128_t)0xFFFFFFFFFFFFFFFFULL << 64) | ~0xAAAFFEULL;
    detail::Bits<128> expected_not_b(exp_not_b_raw);
    EXPECT_EQ(~b, expected_not_b);
}

TEST(TestBits, and_or_op_supports_128) {
    __uint128_t z_raw = ((__uint128_t)0xFFFFFFFFFFFFFFFFULL << 64) | 0x0ULL;
    __uint128_t z_raw2 = ((__uint128_t)0x0ULL << 64) | 0xFFFFFFFFFFFFFFFFULL;

    detail::Bits<128> z(z_raw);
    detail::Bits<128> z_(z_raw2);

    detail::Bits<128> z_and = z & z_;
    detail::Bits<128> z_or = z | z_;

    detail::Bits<128> exp_z_and(0);
    __uint128_t exp_z_or_raw =
        ((__uint128_t)0xFFFFFFFFFFFFFFFFULL << 64) | 0xFFFFFFFFFFFFFFFFULL;
    detail::Bits<128> exp_z_or(exp_z_or_raw);

    EXPECT_EQ(z_and, exp_z_and);
    EXPECT_EQ(z_or, exp_z_or);
}

TEST(TestBits, comparison_operations_supports_128) {
    detail::Bits<128> b128_small(0x055ULL);
    __uint128_t large_raw = ((__uint128_t)0xAAAULL << 64) | 0x0ULL;
    detail::Bits<128> b128_large(large_raw);

    EXPECT_TRUE(b128_small < b128_large);
    EXPECT_TRUE(b128_large >= b128_small);

    // 93 bits max: 29 high bits + 64 low bits
    detail::Bits<93> b93_max(-1);
    detail::Bits<93> b93_zero(0);

    EXPECT_TRUE(b93_max > b93_zero);
    EXPECT_TRUE(b93_zero < b93_max);
    EXPECT_TRUE(b93_max != b93_zero);
}

TEST(TestBits, xor_op_supports_128) {
    __uint128_t a_raw = ((__uint128_t)0xFFFFFFFFFFFFFFFFULL << 64) | 0x0ULL;
    __uint128_t b_raw = ((__uint128_t)0x0ULL << 64) | 0xFFFFFFFFFFFFFFFFULL;
    __uint128_t exp_raw =
        ((__uint128_t)0xFFFFFFFFFFFFFFFFULL << 64) | 0xFFFFFFFFFFFFFFFFULL;

    detail::Bits<128> a(a_raw);
    detail::Bits<128> b(b_raw);
    detail::Bits<128> expected(exp_raw);

    detail::Bits<128> result = a ^ b;
    EXPECT_EQ(result, expected);
}

TEST(TestBits, arithmetic_operations_native_ints_supports_128) {
    detail::Bits<128> a(1000);
    detail::Bits<128> b(300);

    EXPECT_EQ(a + b, detail::Bits<128>(1300));
    EXPECT_EQ(a - b, detail::Bits<128>(700));
    EXPECT_EQ(a * b, detail::Bits<128>(300000));
    EXPECT_EQ(a.udiv(b), detail::Bits<128>(3));
    EXPECT_EQ(a.umod(b), detail::Bits<128>(100));
    EXPECT_EQ(a.sdiv(b), detail::Bits<128>(3));
    EXPECT_EQ(a.smod(b), detail::Bits<128>(100));

    detail::Bits<128> neg_val(-100);
    detail::Bits<128> div_val(3);

    EXPECT_EQ(neg_val.sdiv(div_val), detail::Bits<128>(-33));
    EXPECT_EQ(neg_val.smod(div_val), detail::Bits<128>(-1));

    detail::Bits<128> zero(0);

    EXPECT_THROW(neg_val.udiv(zero), std::domain_error);
    EXPECT_THROW(div_val.sdiv(zero), std::domain_error);
}

TEST(TestBits, shift_right_logical_supports_128) {
    __uint128_t all_ones =
        ((__uint128_t)0xFFFFFFFFFFFFFFFFULL << 64) | 0xFFFFFFFFFFFFFFFFULL;
    detail::Bits<128> a(all_ones);

    a = a.srl(64);
    __uint128_t exp_step1 = ((__uint128_t)0x0ULL << 64) | 0xFFFFFFFFFFFFFFFFULL;
    EXPECT_EQ(a, detail::Bits<128>(exp_step1));

    a = a.srl(64);
    EXPECT_EQ(a, detail::Bits<128>(0));
}

TEST(TestBits, shift_right_arithmetic_supports_128) {
    __uint128_t all_ones =
        ((__uint128_t)0xFFFFFFFFFFFFFFFFULL << 64) | 0xFFFFFFFFFFFFFFFFULL;
    detail::Bits<128> neg_val(all_ones);

    detail::Bits<128> arith_val = neg_val;
    arith_val = arith_val.sra(64);

    EXPECT_EQ(arith_val, detail::Bits<128>(all_ones));

    detail::Bits<128> logical_val = neg_val;
    logical_val = logical_val.srl(64);

    __uint128_t expected_logical = ((__uint128_t)0x0ULL << 64) | 0xFFFFFFFFFFFFFFFFULL;
    EXPECT_EQ(logical_val, detail::Bits<128>(expected_logical));
}

TEST(TestBits, shift_left_supports_128) {
    detail::Bits<128> a(0x4F33ULL);
    a = a << 64;
    a = a | detail::Bits<128>(0x9FF0ULL);

    __uint128_t exp = ((__uint128_t)0x4F33ULL << 64) | 0x9FF0ULL;
    EXPECT_EQ(a, detail::Bits<128>(exp));
}

#else

TEST(TestBits, JustAbove64) {
    static_assert(std::is_same_v<detail::Bits<63>::IntType, uint64_t>);
    static_assert(std::is_same_v<detail::Bits<64>::IntType, uint64_t>);
    static_assert(std::is_same_v<detail::Bits<65>::IntType, detail::BigInt<65>>);
}

#endif  // defined(__SIZEOF_INT128__)

TEST(TestBits, single_word_constructor) {
    detail::Bits<5> a0(0xFF);
    detail::Bits<5> a1 = a0;
    detail::Bits<5> a2(a1);
    detail::Bits<5> a = std::move(a2);
    detail::Bits<5> expected_a(0x1F);
    EXPECT_EQ(a, expected_a);

    detail::Bits<61> c(-1);
    detail::Bits<61> expected_c{0x1FFFFFFFFFFFFFFF};
    EXPECT_EQ(c, expected_c);

    detail::Bits<63> d(-1);
    detail::Bits<63> expected_d{0x7FFFFFFFFFFFFFFF};
    EXPECT_EQ(d, expected_d);

    detail::Bits<200> b(0xABCD);
    EXPECT_EQ(b, detail::Bits<200>{0xABCD});

    detail::Bits<200> e0(-1, true);
    detail::Bits<200> e1 = e0;
    detail::Bits<200> e2(e1);
    detail::Bits<200> e = std::move(e2);
    detail::Bits<200> expected_e{"0x"
                                 "FFFFFFFFFFFFFFFFFFFF"
                                 "FFFFFFFFFFFFFFFFFFFF"
                                 "FFFFFFFFFF"};
    EXPECT_EQ(e, expected_e);
}

TEST(TestBits, string_constructor) {
    detail::Bits<232> a{
        "0x10000_4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };
    detail::Bits<232> b{
        "0x10_4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };
    EXPECT_EQ(a, b);

    detail::Bits<264> expected_hex{
        "0x"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"  // 32 'F's
        "FFFFFFFFFF"                        // 10 'F's
        "FFFFFFFF"                          // 8 'F's (Total 50 'F's = 200 bits)
        "FFDCE95615E366F4"                  // 16 hex chars = 64 bits (Total 264 bits)
    };
    detail::Bits<264> neg_str_val{"-9876543217899788"};
    detail::Bits<264> expected_native(-9876543217899788ll, true);
    EXPECT_EQ(neg_str_val, expected_native);
    EXPECT_EQ(neg_str_val, expected_hex);

    // neg hex should throw
    EXPECT_THROW(detail::Bits<248>{"-0x1000"}, std::invalid_argument);
}

TEST(TestBits, comparison_operations) {
    detail::Bits<32> b32_a(0xDEADBEEF);
    detail::Bits<32> b32_copy(0xDEADBEEF);
    detail::Bits<32> b32_diff(0xDEADBEEE);

    EXPECT_TRUE(b32_a == b32_copy);
    EXPECT_FALSE(b32_a != b32_copy);
    EXPECT_TRUE(b32_a != b32_diff);
    EXPECT_FALSE(b32_a == b32_diff);

    detail::Bits<12> b12_small(0x055);
    detail::Bits<12> b12_large(0xAAA);
    EXPECT_TRUE(b12_small < b12_large);
    EXPECT_TRUE(b12_large >= b12_small);

    detail::Bits<63> b63_max(0x7FFFFFFFFFFFFFFF);
    detail::Bits<63> b63_zero(0);

    EXPECT_TRUE(b63_max > b63_zero);
    EXPECT_TRUE(b63_zero < b63_max);
    EXPECT_TRUE(b63_max != b63_zero);

    detail::Bits<200> a{"0xABCDEF01_00000000_00000000"};
    detail::Bits<200> a_copy{"0xABCDEF01_00000000_00000000"};
    detail::Bits<200> a_diff{"0xABCDEF01_00000000_00000001"};

    EXPECT_TRUE(a == a_copy);
    EXPECT_FALSE(a != a_copy);

    EXPECT_TRUE(a != a_diff);
    EXPECT_FALSE(a == a_diff);

    detail::Bits<256> u_small{"0x11111111_00000000_00000000_00000000"};
    detail::Bits<256> u_large{"0x11111111_00000000_00000000_00000001"};

    detail::Bits<256> u_massive{"0x22222222_00000000_00000000_00000000"};

    EXPECT_TRUE(u_small < u_large);
    EXPECT_TRUE(u_large > u_small);
    EXPECT_TRUE(u_large < u_massive);
    EXPECT_TRUE(u_massive > u_large);

    EXPECT_TRUE(u_small <= u_large);
    EXPECT_TRUE(u_large >= u_small);

    EXPECT_TRUE(u_small <= u_small);
    EXPECT_TRUE(u_small >= u_small);
    EXPECT_FALSE(u_small < u_small);
    EXPECT_FALSE(u_small > u_small);

    detail::Bits<150> pos("5000000");
    detail::Bits<150> neg("-5000000");
    detail::Bits<150> zero(0);

    EXPECT_TRUE(neg < pos);
    EXPECT_TRUE(pos > neg);
    EXPECT_TRUE(neg <= pos);
    EXPECT_TRUE(pos >= neg);

    EXPECT_TRUE(neg < zero);
    EXPECT_TRUE(pos > zero);

    detail::Bits<150> neg_10("-10");
    detail::Bits<150> neg_20("-20");

    EXPECT_TRUE(neg_10 > neg_20);
    EXPECT_TRUE(neg_20 < neg_10);
    EXPECT_TRUE(neg_10 >= neg_20);
    EXPECT_TRUE(neg_20 <= neg_10);
}

TEST(TestBits, and_or_op) {
    detail::Bits<29> z(0x5F3F4AE);
    detail::Bits<29> z_(0x4FAA413);

    detail::Bits<29> z_and = z & z_;
    detail::Bits<29> z_or = z | z_;
    detail::Bits<29> exp_z_and(0x4F2A402);
    detail::Bits<29> exp_z_or(0x5FBF4BF);

    EXPECT_EQ(z_and, exp_z_and);
    EXPECT_EQ(z_or, exp_z_or);

    detail::Bits<233> a{
        "0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };
    detail::Bits<233> b{
        "0xABCF_000000000000_997B_000000000000_BD93_000000000000_0AF8_000000"
    };
    detail::Bits<233> expected_and{
        "0x0B03_000000000000_9970_000000000000_BD13_000000000000_0AF0_000000"
    };
    detail::Bits<233> expected_or{
        "0xEFFF_000000000000_9FFB_000000000000_BDF3_000000000000_9AF8_000000"
    };

    detail::Bits<233> result_and = a & b;
    detail::Bits<233> result_or = a | b;

    EXPECT_EQ(result_and, expected_and);
    EXPECT_EQ(result_or, expected_or);
}

TEST(TestBits, xor_op) {
    detail::Bits<29> z(0x5F3F4AE);
    detail::Bits<29> z_(0x4FAA413);

    detail::Bits<29> z_and = z & z_;
    detail::Bits<29> z_or = z | z_;
    detail::Bits<29> exp_z_and(0x4F2A402);
    detail::Bits<29> exp_z_or(0x5FBF4BF);

    EXPECT_EQ(z_and, exp_z_and);
    EXPECT_EQ(z_or, exp_z_or);

    detail::Bits<233> a{
        "0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };
    detail::Bits<233> b{
        "0xABCF_000000000000_997B_000000000000_BD93_000000000000_0AF8_000000"
    };
    detail::Bits<233> expected{
        "0xE4FC_000000000000_068B_000000000000_00E0_000000000000_9008_000000"
    };

    detail::Bits<233> result = a ^ b;
    EXPECT_EQ(result, expected);
}

TEST(TestBits, not_op) {
    detail::Bits<12> b(0xAAA);
    detail::Bits<12> expected_not_b(0x555);
    EXPECT_EQ(~b, expected_not_b);

    detail::Bits<170> a(0);
    detail::Bits<170> not_a = ~a;

    detail::Bits<170> expected_not_a{
        "0x3FFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF"
    };
    EXPECT_EQ(not_a, expected_not_a);
}

TEST(TestBits, arithmetic_operations_native_ints) {
    detail::Bits<64> a(1000);
    detail::Bits<64> b(300);

    EXPECT_EQ(a + b, detail::Bits<64>(1300));
    EXPECT_EQ(a - b, detail::Bits<64>(700));
    EXPECT_EQ(a * b, detail::Bits<64>(300000));
    EXPECT_EQ(a.udiv(b), detail::Bits<64>(3));
    EXPECT_EQ(a.umod(b), detail::Bits<64>(100));
    EXPECT_EQ(a.sdiv(b), detail::Bits<64>(3));
    EXPECT_EQ(a.smod(b), detail::Bits<64>(100));

    detail::Bits<45> neg_val(-100);
    detail::Bits<45> div_val(3);

    EXPECT_EQ(neg_val.sdiv(div_val), detail::Bits<45>(-33));
    EXPECT_EQ(neg_val.smod(div_val), detail::Bits<45>(-1));

    EXPECT_EQ(neg_val.udiv(div_val), detail::Bits<45>(11728124029577ULL));
    EXPECT_EQ(neg_val.umod(div_val), detail::Bits<45>(1));

    detail::Bits<45> zero(0);

    EXPECT_THROW(neg_val.udiv(zero), std::domain_error);
    EXPECT_THROW(div_val.sdiv(zero), std::domain_error);
}

TEST(TestBits, shift_right_logical) {
    detail::Bits<29> z(0x5F3F4AE);
    z = z.srl(9);
    detail::Bits<29> exp_z(0x2F9FA);
    EXPECT_EQ(z, exp_z);

    detail::Bits<233> a{
        "0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };

    a = a.srl(24);
    detail::Bits<233> expected_step1{
        "0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0"
    };
    EXPECT_EQ(a, expected_step1);

    a = a.srl(64);
    detail::Bits<233> expected_step2{"0x4F33_000000000000_9FF0_000000000000_BD73"};
    EXPECT_EQ(a, expected_step2);

    a = a.srl(64);
    detail::Bits<233> expected_step3{"0x4F33_000000000000_9FF0"};
    EXPECT_EQ(a, expected_step3);
}

TEST(TestBits, shift_right_arithmetic) {
    detail::Bits<29> z(0x5F3F4AE);
    z = z.sra(9);
    detail::Bits<29> exp_z(0x2F9FA);
    EXPECT_EQ(z, exp_z);

    detail::Bits<29> z_(-268435398);
    z_ = z_.sra(9);
    detail::Bits<29> exp_z_(0x1FF80000);  // or -524288
    EXPECT_EQ(z_, exp_z_);

    detail::Bits<233> arith_str{
        "0x1"
        "FFFFFFFFFFFFFFFFFFFFFFFF"            // 24 'F's = 96 bits (Total 97 ones)
        "0000000000000000000000000000000000"  // 34 '0's = 136 bits
    };

    detail::Bits<233> all_ones = ~detail::Bits<233>(0);

    detail::Bits<233> neg_val = all_ones;
    neg_val = neg_val << 200;

    detail::Bits<233> arith_val = neg_val;
    arith_val = arith_val.sra(64);

    detail::Bits<233> expected_arith = all_ones;
    expected_arith = expected_arith << 136;

    EXPECT_EQ(arith_val, expected_arith);
    EXPECT_EQ(arith_val, arith_str);

    detail::Bits<233> logical_str{
        "0x0000000000000000"                  // 16 '0's = 64 zeroes
        "1FFFFFFFF"                           // 1 '1' + 8 'F's = 33 ones
        "0000000000000000000000000000000000"  // 34 '0's = 136 zeroes
    };

    detail::Bits<233> logical_val = neg_val;
    logical_val = logical_val.srl(64);

    detail::Bits<233> expected_logical = all_ones;
    expected_logical = expected_logical.srl(200);
    expected_logical = expected_logical << 136;

    EXPECT_EQ(logical_val, expected_logical);
    EXPECT_EQ(logical_val, logical_str);
}

TEST(TestBits, shift_left) {
    // 16 bits (4F33) + 48 bits (zeros) +
    // 16 bits (9FF0) + 48 bits (zeros) +
    // 16 bits (BD73) + 48 bits (zeros) +
    // 16 bits (9AF0) + 24 bits (zeros)
    // total = 232 bits

    detail::Bits<29> z(0x5F3F4AE);
    z = z << 9;
    detail::Bits<29> exp_z(0x7E95C00);
    EXPECT_EQ(z, exp_z);

    detail::Bits<233> a(0x4F33);
    a = a << 64;
    a = a | detail::Bits<233>(0x9FF0);
    a = a << 64;
    a = a | detail::Bits<233>(0xBD73);
    a = a << 64;
    a = a | detail::Bits<233>(0x9AF0);
    a = a << 24;

    detail::Bits<233> b{
        "0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };

    EXPECT_EQ(a, b);
}

#endif  // COCONEXT_USE_APINT

// LCOV_EXCL_BR_STOP

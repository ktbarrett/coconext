// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <type_traits>

using namespace coconext::types;

#ifdef COCONEXT_USE_APINT

// Test SInt/UInt with APInt IntBackend

#else

#if defined(__SIZEOF_INT128__)

TEST(TestIntBackend, JustAbove128) {
    static_assert(std::is_same_v<SInt<127>, detail::IntBackend<127, true>>);
    static_assert(std::is_same_v<UInt<127>, detail::IntBackend<127, false>>);

    static_assert(std::is_same_v<SInt<128>, __int128_t>);
    static_assert(std::is_same_v<UInt<128>, __uint128_t>);

    static_assert(std::is_same_v<SInt<129>, detail::IntBackend<129, true>>);
    static_assert(std::is_same_v<UInt<129>, detail::IntBackend<129, false>>);
}

#else

TEST(TestIntBackend, JustAbove64) {
    static_assert(std::is_same_v<SInt<63>, detail::IntBackend<63, true>>);
    static_assert(std::is_same_v<UInt<63>, detail::IntBackend<63, false>>);

    static_assert(std::is_same_v<SInt<64>, int64_t>);
    static_assert(std::is_same_v<UInt<64>, uint64_t>);

    static_assert(std::is_same_v<SInt<65>, detail::IntBackend<65, true>>);
    static_assert(std::is_same_v<UInt<65>, detail::IntBackend<65, false>>);
}

#endif  // defined(__SIZEOF_INT128__)

TEST(TestIntBackend, single_word_constructor) {
    UInt<5> a0(0xFF);
    UInt<5> a1 = a0;
    UInt<5> a2(a1);
    UInt<5> a = std::move(a2);
    EXPECT_EQ(a, UInt<5>(0x1F));

    UInt<200> b(0xABCD);
    EXPECT_EQ(b, UInt<200>{"0xABCD"});

    SInt<100> c(-1);
    SInt<100> expected_c{"0xFFFFFFFFFFFFFFFFFFFFFFFFF"};
    EXPECT_EQ(c, expected_c);
}

TEST(TestIntBackend, string_constructor) {

    // Both a & b occupy same number of words and must be equal.
    // the leading "0000" in a and "10" in b respectively does not
    // matter because anything beyond actual specified bits is masked out
    // by constructor in the last 64 bit word which is storing MSBs
    SInt<248> a{
        "0x10000_4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };
    SInt<232> b{"0x10_4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"};
    EXPECT_EQ(a.get_data(), b.get_data());

    SInt<264> expected_hex{
        "0x"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"  // 32 'F's
        "FFFFFFFFFF"                        // 10 'F's
        "FFFFFFFF"                          // 8 'F's (Total 50 'F's = 200 bits)
        "FFDCE95615E366F4"                  // 16 hex chars = 64 bits (Total 264 bits)
    };
    SInt<264> neg_str_val{"-9876543217899788"};
    SInt<264> expected_native(-9876543217899788ll);
    EXPECT_EQ(neg_str_val, expected_native);
    EXPECT_EQ(neg_str_val, expected_hex);

    // neg hex should throw
    EXPECT_THROW(SInt<248>{"-0x1000"}, std::invalid_argument);
}

TEST(TestIntBackend, comparison_operations) {
    SInt<200> a{"0xABCDEF01_00000000_00000000"};
    SInt<200> a_copy{"0xABCDEF01_00000000_00000000"};
    SInt<200> a_diff{"0xABCDEF01_00000000_00000001"};

    EXPECT_TRUE(a == a_copy);
    EXPECT_FALSE(a != a_copy);

    EXPECT_TRUE(a != a_diff);
    EXPECT_FALSE(a == a_diff);

    UInt<256> u_small{"0x11111111_00000000_00000000_00000000"};
    UInt<256> u_large{"0x11111111_00000000_00000000_00000001"};

    UInt<256> u_massive{"0x22222222_00000000_00000000_00000000"};

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

    SInt<150> pos("5000000");
    SInt<150> neg("-5000000");
    SInt<150> zero(0);

    EXPECT_TRUE(neg < pos);
    EXPECT_TRUE(pos > neg);
    EXPECT_TRUE(neg <= pos);
    EXPECT_TRUE(pos >= neg);

    EXPECT_TRUE(neg < zero);
    EXPECT_TRUE(pos > zero);

    SInt<150> neg_10("-10");
    SInt<150> neg_20("-20");

    EXPECT_TRUE(neg_10 > neg_20);
    EXPECT_TRUE(neg_20 < neg_10);
    EXPECT_TRUE(neg_10 >= neg_20);
    EXPECT_TRUE(neg_20 <= neg_10);
}

TEST(TestIntBackend, and_or_op) {
    SInt<233> a{"0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"};
    SInt<233> b{"0xABCF_000000000000_997B_000000000000_BD93_000000000000_0AF8_000000"};
    SInt<233> expected_and{
        "0x0B03_000000000000_9970_000000000000_BD13_000000000000_0AF0_000000"
    };
    SInt<233> expected_or{
        "0xEFFF_000000000000_9FFB_000000000000_BDF3_000000000000_9AF8_000000"
    };

    SInt<233> result_and = a & b;
    SInt<233> result_or = a | b;

    EXPECT_EQ(result_and, expected_and);
    EXPECT_EQ(result_or, expected_or);
}

TEST(TestIntBackend, xor_op) {
    SInt<233> a{"0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"};
    SInt<233> b{"0xABCF_000000000000_997B_000000000000_BD93_000000000000_0AF8_000000"};
    SInt<233> expected{
        "0xE4FC_000000000000_068B_000000000000_00E0_000000000000_9008_000000"
    };

    SInt<233> result = a ^ b;
    EXPECT_EQ(result, expected);
}

TEST(TestIntBackend, not_op) {
    UInt<70> a(0);
    UInt<70> not_a = ~a;

    UInt<70> expected_not_a{"0x3F'FFFF'FFFF'FFFF'FFFF"};
    EXPECT_EQ(not_a, expected_not_a);

    UInt<12> b(0xAAA);
    UInt<12> expected_not_b(0x555);
    EXPECT_EQ(~b, expected_not_b);
}

TEST(TestIntBackend, arithmetic_operations) {
    SInt<64> a(1000);
    SInt<64> b(300);

    EXPECT_EQ(a + b, SInt<64>(1300));
    EXPECT_EQ(a - b, SInt<64>(700));
    EXPECT_EQ(a * b, SInt<64>(300000));

    SInt<45> neg_val(-100);
    SInt<45> div_val(3);

    EXPECT_EQ(neg_val / div_val, SInt<45>(-33));
    EXPECT_EQ(neg_val % div_val, SInt<45>(-1));

    SInt<64> zero(0);

    EXPECT_THROW(neg_val / zero, std::domain_error);
    EXPECT_THROW(div_val % zero, std::domain_error);
}

TEST(TestIntBackend, shift_right_logical) {
    SInt<233> a{"0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"};

    detail::shift_right_logical(a, 24);
    SInt<233> expected_step1{
        "0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0"
    };
    EXPECT_EQ(a, expected_step1);

    detail::shift_right_logical(a, 64);
    SInt<233> expected_step2{"0x4F33_000000000000_9FF0_000000000000_BD73"};
    EXPECT_EQ(a, expected_step2);

    detail::shift_right_logical(a, 64);
    SInt<233> expected_step3{"0x4F33_000000000000_9FF0"};
    EXPECT_EQ(a, expected_step3);
}

TEST(TestIntBackend, shift_right_arithmetic) {
    SInt<233> arith_str{
        "0x1"
        "FFFFFFFFFFFFFFFFFFFFFFFF"            // 24 'F's = 96 bits (Total 97 ones)
        "0000000000000000000000000000000000"  // 34 '0's = 136 bits
    };

    SInt<233> all_ones = ~SInt<233>(0);

    SInt<233> neg_val = all_ones;
    detail::shift_left(neg_val, 200);

    SInt<233> arith_val = neg_val;
    detail::shift_right_arith(arith_val, 64);

    SInt<233> expected_arith = all_ones;
    detail::shift_left(expected_arith, 136);

    EXPECT_EQ(arith_val, expected_arith);
    EXPECT_EQ(arith_val, arith_str);

    SInt<233> logical_str{
        "0x0000000000000000"                  // 16 '0's = 64 zeroes
        "1FFFFFFFF"                           // 1 '1' + 8 'F's = 33 ones
        "0000000000000000000000000000000000"  // 34 '0's = 136 zeroes
    };

    SInt<233> logical_val = neg_val;
    detail::shift_right_logical(logical_val, 64);

    SInt<233> expected_logical = all_ones;
    detail::shift_right_logical(expected_logical, 200);
    detail::shift_left(expected_logical, 136);

    EXPECT_EQ(logical_val, expected_logical);
    EXPECT_EQ(logical_val, logical_str);
}

TEST(TestIntBackend, shift_left) {
    // 16 bits (4F33) + 48 bits (zeros) +
    // 16 bits (9FF0) + 48 bits (zeros) +
    // 16 bits (BD73) + 48 bits (zeros) +
    // 16 bits (9AF0) + 24 bits (zeros)
    // total = 232 bits

    SInt<233> a(0x4F33);
    detail::shift_left(a, 64);
    a = a | SInt<233>(0x9FF0);
    detail::shift_left(a, 64);
    a = a | SInt<233>(0xBD73);
    detail::shift_left(a, 64);
    a = a | SInt<233>(0x9AF0);
    detail::shift_left(a, 24);

    SInt<233> b{"0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"};

    EXPECT_EQ(a, b);
}

#endif  // COCONEXT_USE_APINT

// LCOV_EXCL_BR_STOP

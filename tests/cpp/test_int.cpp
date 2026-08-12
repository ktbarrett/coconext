// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <type_traits>

using namespace coconext::types;

// Bits' same-width arithmetic is private -- the growing free functions are the
// public surface. These tests drive the primitives directly because the
// division kernels need edge cases the growing ops cannot reach.
using SW = detail::same_width;

#if defined(__SIZEOF_INT128__)

TEST(TestBits, JustAbove128) {
    static_assert(std::is_same_v<detail::Bits<127>::IntType, __uint128_t>);
    static_assert(std::is_same_v<detail::Bits<128>::IntType, __uint128_t>);
    static_assert(std::is_same_v<detail::Bits<129>::IntType, detail::WideWords<129>>);
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

    EXPECT_TRUE(b128_small.ult(b128_large));
    EXPECT_TRUE(b128_large.uge(b128_small));

    // 93 bits max: 29 high bits + 64 low bits
    detail::Bits<93> b93_max(-1);
    detail::Bits<93> b93_zero(0);

    EXPECT_TRUE(b93_max.ugt(b93_zero));
    EXPECT_TRUE(b93_zero.ult(b93_max));
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

    EXPECT_EQ(SW::add(a, b), detail::Bits<128>(1300));
    EXPECT_EQ(SW::sub(a, b), detail::Bits<128>(700));
    EXPECT_EQ(SW::mul(a, b), detail::Bits<128>(300000));
    EXPECT_EQ(SW::udiv(a, b), detail::Bits<128>(3));
    EXPECT_EQ(SW::umod(a, b), detail::Bits<128>(100));
    EXPECT_EQ(SW::sdiv(a, b), detail::Bits<128>(3));
    EXPECT_EQ(SW::smod(a, b), detail::Bits<128>(100));

    detail::Bits<128> neg_val(-100);
    detail::Bits<128> div_val(3);

    EXPECT_EQ(SW::sdiv(neg_val, div_val), detail::Bits<128>(-33));
    EXPECT_EQ(SW::smod(neg_val, div_val), detail::Bits<128>(-1));

    detail::Bits<128> zero(0);

    EXPECT_THROW(SW::udiv(neg_val, zero), std::domain_error);
    EXPECT_THROW(SW::sdiv(div_val, zero), std::domain_error);
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
    static_assert(std::is_same_v<detail::Bits<65>::IntType, detail::WideWords<65>>);
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

    detail::Bits<200> e0(-1);
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
    // Same in-range value written with and without leading zeros.
    detail::Bits<232> a{
        "0x0000_4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };
    detail::Bits<232> b{
        "0x4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
    };
    EXPECT_EQ(a, b);

    // A literal wider than the type throws rather than silently truncating.
    EXPECT_THROW(
        (detail::Bits<232>{
            "0x10000_4F33_000000000000_9FF0_000000000000_BD73_000000000000_9AF0_000000"
        }),
        std::out_of_range
    );
    // A decimal literal that does not fit also throws.
    EXPECT_THROW(
        (detail::Bits<130>{"1361129467683753853853498429727072845824"}), std::out_of_range
    );

    detail::Bits<264> expected_hex{
        "0x"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"  // 32 'F's
        "FFFFFFFFFF"                        // 10 'F's
        "FFFFFFFF"                          // 8 'F's (Total 50 'F's = 200 bits)
        "FFDCE95615E366F4"                  // 16 hex chars = 64 bits (Total 264 bits)
    };
    detail::Bits<264> neg_str_val{"-9876543217899788"};
    detail::Bits<264> expected_native(-9876543217899788ll);
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

    // Ordering is via named unsigned (u*) / signed (s*) comparisons; Bits has
    // no operator< because the interpretation of the bit pattern is the
    // caller's. This mirrors LLVM APInt's ult/slt API.
    detail::Bits<12> b12_small(0x055);
    detail::Bits<12> b12_large(0xAAA);
    EXPECT_TRUE(b12_small.ult(b12_large));
    EXPECT_TRUE(b12_large.uge(b12_small));

    detail::Bits<63> b63_max(0x7FFFFFFFFFFFFFFF);
    detail::Bits<63> b63_zero(0);

    EXPECT_TRUE(b63_max.ugt(b63_zero));
    EXPECT_TRUE(b63_zero.ult(b63_max));
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

    EXPECT_TRUE(u_small.ult(u_large));
    EXPECT_TRUE(u_large.ugt(u_small));
    EXPECT_TRUE(u_large.ult(u_massive));
    EXPECT_TRUE(u_massive.ugt(u_large));

    EXPECT_TRUE(u_small.ule(u_large));
    EXPECT_TRUE(u_large.uge(u_small));

    EXPECT_TRUE(u_small.ule(u_small));
    EXPECT_TRUE(u_small.uge(u_small));
    EXPECT_FALSE(u_small.ult(u_small));
    EXPECT_FALSE(u_small.ugt(u_small));

    // A negative literal stores its two's-complement pattern. Under the signed
    // interpretation (s*) it orders below a positive value; under the unsigned
    // interpretation (u*) its high bit makes it the larger magnitude.
    detail::Bits<150> pos("5000000");
    detail::Bits<150> neg("-5000000");
    detail::Bits<150> zero(0);

    EXPECT_TRUE(neg.slt(pos));
    EXPECT_TRUE(pos.sgt(neg));
    EXPECT_TRUE(neg.sle(pos));
    EXPECT_TRUE(pos.sge(neg));
    EXPECT_TRUE(neg.slt(zero));
    EXPECT_TRUE(pos.sgt(zero));

    EXPECT_TRUE(pos.ult(neg));  // unsigned: neg's pattern is larger
    EXPECT_TRUE(neg.ugt(pos));

    // Signed: -10 > -20. Unsigned: -10's pattern is also larger than -20's.
    detail::Bits<150> neg_10("-10");
    detail::Bits<150> neg_20("-20");

    EXPECT_TRUE(neg_10.sgt(neg_20));
    EXPECT_TRUE(neg_20.slt(neg_10));
    EXPECT_TRUE(neg_10.sge(neg_20));
    EXPECT_TRUE(neg_20.sle(neg_10));

    EXPECT_TRUE(neg_10.ugt(neg_20));
    EXPECT_TRUE(neg_20.ult(neg_10));
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
        "0x3FF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF"
    };
    EXPECT_EQ(not_a, expected_not_a);
}

TEST(TestBits, arithmetic_operations_native_ints) {
    detail::Bits<64> a(1000);
    detail::Bits<64> b(300);

    EXPECT_EQ(SW::add(a, b), detail::Bits<64>(1300));
    EXPECT_EQ(SW::sub(a, b), detail::Bits<64>(700));
    EXPECT_EQ(SW::mul(a, b), detail::Bits<64>(300000));
    EXPECT_EQ(SW::udiv(a, b), detail::Bits<64>(3));
    EXPECT_EQ(SW::umod(a, b), detail::Bits<64>(100));
    EXPECT_EQ(SW::sdiv(a, b), detail::Bits<64>(3));
    EXPECT_EQ(SW::smod(a, b), detail::Bits<64>(100));

    detail::Bits<45> neg_val(-100);
    detail::Bits<45> div_val(3);

    EXPECT_EQ(SW::sdiv(neg_val, div_val), detail::Bits<45>(-33));
    EXPECT_EQ(SW::smod(neg_val, div_val), detail::Bits<45>(-1));

    EXPECT_EQ(SW::udiv(neg_val, div_val), detail::Bits<45>(11728124029577ULL));
    EXPECT_EQ(SW::umod(neg_val, div_val), detail::Bits<45>(1));

    detail::Bits<45> zero(0);

    EXPECT_THROW(SW::udiv(neg_val, zero), std::domain_error);
    EXPECT_THROW(SW::sdiv(div_val, zero), std::domain_error);
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

// Wide word-array arithmetic. Reference values computed with Python's
// arbitrary-precision integers.
TEST(TestBits, arithmetic_operations_wide) {
    detail::Bits<200> a{"0x1234567890ABCDEF1122334455667788AABBCCDD"};
    detail::Bits<200> b{"0xFEDCBA98765432100123456789"};

    EXPECT_EQ(
        SW::add(a, b).to_decimal_string(),
        "103929005307927776916288754849918835164314678374"
    );
    EXPECT_EQ(
        SW::sub(a, b).to_decimal_string(),
        "103929005307927736531757632912370613094305916244"
    );
    EXPECT_EQ(
        SW::mul(a, b).to_decimal_string(),
        "329963546613616313339723066835445579609796160260803833793861"
    );
    EXPECT_EQ(SW::udiv(a, b).to_decimal_string(), "5146971002046463");
    EXPECT_EQ(SW::umod(a, b).to_decimal_string(), "20112278405973339191843622874214");

    // Division identity holds.
    EXPECT_EQ(SW::add(SW::mul(SW::udiv(a, b), b), SW::umod(a, b)), a);

    // Degenerate cases.
    detail::Bits<200> zero(0);
    detail::Bits<200> one(1);
    EXPECT_EQ(SW::udiv(a, a), one);
    EXPECT_EQ(SW::umod(a, a), zero);
    EXPECT_EQ(SW::udiv(b, a), zero);  // b < a
    EXPECT_EQ(SW::umod(b, a), b);
    EXPECT_EQ(SW::udiv(zero, a), zero);
    EXPECT_EQ(SW::udiv(a, one), a);

    EXPECT_THROW(SW::udiv(a, zero), std::domain_error);
    EXPECT_THROW(SW::umod(a, zero), std::domain_error);
    EXPECT_THROW(SW::sdiv(a, zero), std::domain_error);
    EXPECT_THROW(SW::smod(a, zero), std::domain_error);
}

TEST(TestBits, signed_arithmetic_wide) {
    detail::Bits<200> neg{"-1000000000000000000000"};
    detail::Bits<200> pos{"7"};

    EXPECT_EQ(SW::sdiv(neg, pos).to_decimal_string(true), "-142857142857142857142");
    EXPECT_EQ(SW::smod(neg, pos).to_decimal_string(true), "-6");

    detail::Bits<200> neg2{"-3"};
    EXPECT_EQ(SW::sdiv(neg, neg2).to_decimal_string(true), "333333333333333333333");
    EXPECT_EQ(SW::smod(neg, neg2).to_decimal_string(true), "-1");

    // MIN / -1 wraps to MIN rather than invoking UB. On the wide path MIN is
    // -2^(W-1); dividing by -1 yields the same pattern back.
    detail::Bits<200> min = detail::Bits<200>(1) << 199;  // sign bit only
    detail::Bits<200> neg_one = ~detail::Bits<200>(0);    // all ones == -1
    EXPECT_EQ(SW::sdiv(min, neg_one), min);
    EXPECT_EQ(SW::smod(min, neg_one), detail::Bits<200>(0));
}

TEST(TestBits, division_pairs) {
    detail::Bits<200> minus_five(-5);
    detail::Bits<200> three(3);
    detail::Bits<200> minus_three(-3);

    auto [q1, r1] = SW::sdivrem(minus_five, three);
    EXPECT_EQ(q1.to_decimal_string(true), "-1");
    EXPECT_EQ(r1.to_decimal_string(true), "-2");

    auto [q2, m2] = SW::sdivmod(minus_five, three);
    EXPECT_EQ(q2.to_decimal_string(true), "-2");
    EXPECT_EQ(m2.to_decimal_string(true), "1");

    auto [q3, m3] = SW::sdivmod(detail::Bits<200>(5), minus_three);
    EXPECT_EQ(q3.to_decimal_string(true), "-2");
    EXPECT_EQ(m3.to_decimal_string(true), "-1");

    auto [uq, ur] = SW::udivrem(detail::Bits<200>(17), detail::Bits<200>(5));
    EXPECT_EQ(uq, detail::Bits<200>(3));
    EXPECT_EQ(ur, detail::Bits<200>(2));
}

TEST(TestBits, string_overflow_throws_wide) {
    // Exactly-fitting literals are accepted.
    EXPECT_NO_THROW((detail::Bits<200>{"0x" + std::string(50, 'F')}));  // 200 ones
    // One nibble too many throws.
    EXPECT_THROW((detail::Bits<200>{"0x" + std::string(51, 'F')}), std::out_of_range);
}

TEST(TestBits, to_string_wide) {
    detail::Bits<200> a{"0x1234567890ABCDEF1122334455667788AABBCCDD"};
    EXPECT_EQ(
        a.to_hexadecimal_string(), "00000000001234567890abcdef1122334455667788aabbccdd"
    );
    EXPECT_EQ(a.to_decimal_string(), "103929005307927756724023193881144724129310297309");

    detail::Bits<200> neg{"-5"};
    EXPECT_EQ(neg.to_decimal_string(true), "-5");
    EXPECT_EQ(detail::Bits<200>(0).to_decimal_string(), "0");
}

TEST(TestBits, bit_reference_write_native) {
    detail::Bits<12> a(0);
    a[0] = '1'_b;
    a[3] = '1'_b;
    a[11] = '1'_b;
    EXPECT_EQ(a, detail::Bits<12>(0b100000001001));

    a[0] = '0'_b;
    EXPECT_EQ(a, detail::Bits<12>(0b100000001000));

    // proxy-to-proxy assignment: copy bit 11 into bit 5
    detail::Bits<12> b(0);
    b[5] = a[11];
    EXPECT_EQ(b, detail::Bits<12>(1u << 5));
}

TEST(TestBits, bit_reference_write_wide) {
    detail::Bits<200> a(0);
    a[0] = '1'_b;
    a[199] = '1'_b;
    a[100] = '1'_b;
    detail::Bits<200> expected = (detail::Bits<200>(1) << 199)
                               | (detail::Bits<200>(1) << 100) | detail::Bits<200>(1);
    EXPECT_EQ(a, expected);

    a[100] = '0'_b;
    EXPECT_EQ(a, (detail::Bits<200>(1) << 199) | detail::Bits<200>(1));

    // proxy-to-proxy assignment across the wide path
    detail::Bits<200> b(0);
    b[50] = a[199];
    EXPECT_EQ(b, detail::Bits<200>(1) << 50);
}

TEST(TestBits, popcount_and_count_zeros_odd_widths) {
    // Odd non-word-aligned widths exercise the unused_bits math on native path.
    detail::Bits<45> a(0);
    EXPECT_EQ(a.popcount(), 0u);
    EXPECT_EQ(a.count_leading_zeros(), 45u);
    EXPECT_EQ(a.count_trailing_zeros(), 45u);

    detail::Bits<45> b(0b1010);
    EXPECT_EQ(b.popcount(), 2u);
    EXPECT_EQ(b.count_leading_zeros(), 45u - 4u);
    EXPECT_EQ(b.count_trailing_zeros(), 1u);

    // MSB set - CLZ must be zero across the unused_bits correction.
    detail::Bits<45> c = detail::Bits<45>(1) << 44;
    EXPECT_EQ(c.count_leading_zeros(), 0u);
    EXPECT_EQ(c.popcount(), 1u);

#if defined(__SIZEOF_INT128__)
    // Width 93 sits in the __uint128_t split branch.
    detail::Bits<93> d(1);
    EXPECT_EQ(d.count_leading_zeros(), 92u);
    EXPECT_EQ(d.count_trailing_zeros(), 0u);
    EXPECT_EQ(d.popcount(), 1u);

    detail::Bits<93> e = detail::Bits<93>(1) << 92;
    EXPECT_EQ(e.count_leading_zeros(), 0u);
    EXPECT_EQ(e.count_trailing_zeros(), 92u);
    EXPECT_EQ(e.popcount(), 1u);

    detail::Bits<128> f(0);
    EXPECT_EQ(f.popcount(), 0u);
    EXPECT_EQ(f.count_leading_zeros(), 128u);
    EXPECT_EQ(f.count_trailing_zeros(), 128u);

    detail::Bits<128> g = ~detail::Bits<128>(0);
    EXPECT_EQ(g.popcount(), 128u);
    EXPECT_EQ(g.count_leading_zeros(), 0u);
    EXPECT_EQ(g.count_trailing_zeros(), 0u);
#endif

    // Wide word-array path parity check.
    detail::Bits<200> h(0);
    EXPECT_EQ(h.popcount(), 0u);
    EXPECT_EQ(h.count_leading_zeros(), 200u);
    EXPECT_EQ(h.count_trailing_zeros(), 200u);

    detail::Bits<200> i = detail::Bits<200>(1) << 199;
    EXPECT_EQ(i.count_leading_zeros(), 0u);
    EXPECT_EQ(i.count_trailing_zeros(), 199u);
    EXPECT_EQ(i.popcount(), 1u);
}

// The native tier must stay exactly as wide as it claims, so arrays of the
// small integer types keep their size and vectorize.
TEST(TestBits, native_storage_is_exact_width) {
    static_assert(sizeof(detail::Bits<8>) == 1);
    static_assert(sizeof(detail::Bits<16>) == 2);
    static_assert(sizeof(detail::Bits<32>) == 4);
    static_assert(sizeof(detail::Bits<64>) == 8);
    SUCCEED();
}

// raw() hands back a non-owning view on the wide tier rather than copying the
// whole word array; the native tier still returns the storage integer.
TEST(TestBits, raw_type_by_tier) {
    static_assert(std::is_same_v<detail::Bits<8>::RawType, uint8_t>);
    static_assert(std::is_same_v<detail::Bits<64>::RawType, uint64_t>);
    static_assert(std::is_same_v<detail::Bits<200>::RawType, detail::WordConstSpan>);
    SUCCEED();
}

// The string constructor is no longer restricted to the wide tier.
TEST(TestBits, string_ctor_at_every_tier) {
    EXPECT_EQ(detail::Bits<16>("1000").to_decimal_string(), "1000");
    EXPECT_EQ(detail::Bits<64>("42").to_decimal_string(), "42");
    EXPECT_EQ(detail::Bits<64>("0xDEADBEEF").to_decimal_string(), "3735928559");
    EXPECT_EQ(detail::Bits<200>("42").to_decimal_string(), "42");

    // Overflow throws at narrow widths too, rather than truncating.
    EXPECT_THROW(detail::Bits<8>("999"), std::out_of_range);
    EXPECT_NO_THROW(detail::Bits<8>("255"));
}

#if defined(__SIZEOF_INT128__)
// Proof that the wide path remains fully usable in constant expressions:
// division, multiplication and comparison all evaluate at compile time, and
// the division identity holds.
TEST(TestBits, constexpr_wide) {
    constexpr detail::Bits<200> a{"0xFEDCBA9876543210FEDCBA98"};
    constexpr detail::Bits<200> b(uint64_t{1000000007});
    constexpr auto qr = SW::udivrem(a, b);
    constexpr auto q = qr.first;
    constexpr auto r = qr.second;
    static_assert(SW::add(SW::mul(q, b), r) == a, "division identity at compile time");
    static_assert(a.ugt(b));
    static_assert(SW::umod(a, a) == detail::Bits<200>(0));
    SUCCEED();
}
#endif

// A zero-width Bits is the sole VHDL "null" representation. It is not the
// integer 0 - it has no value at all. Size-agnostic operations (arithmetic,
// bitwise, popcount) return the null vector; size-dependent ones (shifts,
// orderings, divides, raw, get/set_bit, native-int ctor) are compile errors.
TEST(TestBits, zero_width) {
    detail::Bits<0> a{};
    detail::Bits<0> b{};

    // equality: two null vectors are equal
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    // arithmetic and bitwise short-circuit to null
    EXPECT_TRUE(SW::add(a, b) == detail::Bits<0>{});
    EXPECT_TRUE(SW::sub(a, b) == detail::Bits<0>{});
    EXPECT_TRUE(SW::mul(a, b) == detail::Bits<0>{});
    EXPECT_TRUE((a & b) == detail::Bits<0>{});
    EXPECT_TRUE((a | b) == detail::Bits<0>{});
    EXPECT_TRUE((a ^ b) == detail::Bits<0>{});
    EXPECT_TRUE(~a == detail::Bits<0>{});

    // bit-counts on the empty vector are all 0
    EXPECT_EQ(a.popcount(), 0u);
    EXPECT_EQ(a.count_leading_zeros(), 0u);
    EXPECT_EQ(a.count_trailing_zeros(), 0u);

    // iteration is empty
    EXPECT_EQ(a.begin(), a.end());
    EXPECT_EQ(a.rbegin(), a.rend());

    // empty initializer list is the only valid init-list at W=0
    detail::Bits<0> c{std::initializer_list<Bit>{}};
    EXPECT_TRUE(c == a);
    EXPECT_THROW((detail::Bits<0>{'1'_b}), std::invalid_argument);

    // to_*_string returns "" universally at W=0
    EXPECT_EQ(a.to_binary_string(), "");
    EXPECT_EQ(a.to_decimal_string(), "");
    EXPECT_EQ(a.to_hexadecimal_string(), "");
    EXPECT_EQ(a.to_octal_string(), "");

    // any index is out of range
    EXPECT_THROW(a[0], std::out_of_range);
}

// ---------------------------------------------------------------------------
// Width-changing operations
// ---------------------------------------------------------------------------

TEST(TestBitsResize, zero_extend_fills_with_zero) {
    detail::Bits<8> a(uint8_t{0xFF});
    auto w = a.zero_extend<16>();
    static_assert(std::is_same_v<decltype(w), detail::Bits<16>>);
    EXPECT_EQ(w.to_decimal_string(), "255");

    // Identity when the width does not change.
    EXPECT_TRUE(a.zero_extend<8>() == a);

    // Across the native/wide tier boundary.
    detail::Bits<64> b(~uint64_t{0});
    EXPECT_EQ(b.zero_extend<200>().to_decimal_string(), "18446744073709551615");
}

TEST(TestBitsResize, sign_extend_replicates_the_sign_bit) {
    detail::Bits<8> neg(uint8_t{0xFF});  // -1
    EXPECT_EQ(neg.sign_extend<16>().to_decimal_string(true), "-1");
    EXPECT_EQ(neg.sign_extend<200>().to_decimal_string(true), "-1");

    detail::Bits<8> pos(uint8_t{0x7F});  // 127
    EXPECT_EQ(pos.sign_extend<16>().to_decimal_string(true), "127");
    EXPECT_EQ(pos.sign_extend<200>().to_decimal_string(true), "127");

    EXPECT_TRUE(neg.sign_extend<8>() == neg);
}

TEST(TestBitsResize, truncate_drops_the_high_bits) {
    detail::Bits<16> a(uint16_t{0xABCD});
    EXPECT_EQ(a.truncate<8>().to_hexadecimal_string(), "cd");
    EXPECT_TRUE(a.truncate<16>() == a);

    // Wide down to native.
    detail::Bits<200> w("0x1234567890ABCDEF");
    EXPECT_EQ(w.truncate<32>().to_hexadecimal_string(), "90abcdef");
}

TEST(TestBitsResize, saturate_unsigned_clamps_at_the_max) {
    detail::Bits<16> big(uint16_t{5000});
    EXPECT_EQ(big.saturate_unsigned<8>().to_decimal_string(), "255");

    detail::Bits<16> fits(uint16_t{200});
    EXPECT_EQ(fits.saturate_unsigned<8>().to_decimal_string(), "200");

    // Widening degenerates to zero_extend.
    EXPECT_EQ(fits.saturate_unsigned<32>().to_decimal_string(), "200");
}

TEST(TestBitsResize, saturate_signed_clamps_at_both_ends) {
    detail::Bits<16> big(uint16_t{5000});
    EXPECT_EQ(big.saturate_signed<8>().to_decimal_string(true), "127");

    detail::Bits<16> very_neg(uint16_t{0x8000});  // -32768
    EXPECT_EQ(very_neg.saturate_signed<8>().to_decimal_string(true), "-128");

    detail::Bits<16> fits(uint16_t{0xFFFF});  // -1
    EXPECT_EQ(fits.saturate_signed<8>().to_decimal_string(true), "-1");

    // Widening degenerates to sign_extend.
    EXPECT_EQ(fits.saturate_signed<32>().to_decimal_string(true), "-1");
}

// ---------------------------------------------------------------------------
// Growing arithmetic. Reference values computed with Python.
// ---------------------------------------------------------------------------

TEST(TestBitsGrowing, additive_grows_by_one_bit) {
    detail::Bits<8> a(uint8_t{200});
    detail::Bits<8> b(uint8_t{100});

    auto sum = detail::add_unsigned(a, b);
    static_assert(std::is_same_v<decltype(sum), detail::Bits<9>>);
    EXPECT_EQ(sum.to_decimal_string(), "300");  // does not wrap at 8 bits

    // Unequal widths grow off the wider operand.
    detail::Bits<16> c(uint16_t{1000});
    auto mixed = detail::add_unsigned(a, c);
    static_assert(std::is_same_v<decltype(mixed), detail::Bits<17>>);
    EXPECT_EQ(mixed.to_decimal_string(), "1200");

    // Unsigned subtraction borrows into the extra bit rather than wrapping.
    EXPECT_EQ(detail::sub_unsigned(b, a).to_decimal_string(), "412");
    EXPECT_EQ(detail::sub_unsigned(a, b).to_decimal_string(), "100");
}

TEST(TestBitsGrowing, additive_signed_uses_sign_extension) {
    detail::Bits<8> neg(uint8_t{200});  // -56
    detail::Bits<8> pos(uint8_t{100});  // 100

    EXPECT_EQ(detail::add_signed(neg, pos).to_decimal_string(true), "44");
    EXPECT_EQ(detail::sub_signed(neg, pos).to_decimal_string(true), "-156");
}

TEST(TestBitsGrowing, multiply_sums_the_widths) {
    detail::Bits<8> a(uint8_t{200});
    detail::Bits<8> b(uint8_t{100});

    auto p = detail::mul_unsigned(a, b);
    static_assert(std::is_same_v<decltype(p), detail::Bits<16>>);
    EXPECT_EQ(p.to_decimal_string(), "20000");

    // -56 * 100 == -5600
    EXPECT_EQ(detail::mul_signed(a, b).to_decimal_string(true), "-5600");
}

TEST(TestBitsGrowing, division_quotient_grows_by_one) {
    detail::Bits<8> a(uint8_t{200});
    detail::Bits<8> b(uint8_t{7});

    auto q = detail::div_unsigned(a, b);
    static_assert(std::is_same_v<decltype(q), detail::Bits<9>>);
    EXPECT_EQ(q.to_decimal_string(), "28");

    auto r = detail::rem_unsigned(a, b);
    static_assert(std::is_same_v<decltype(r), detail::Bits<8>>);
    EXPECT_EQ(r.to_decimal_string(), "4");
}

// The extra quotient bit exists so signed_min / -1 stays representable.
TEST(TestBitsGrowing, signed_min_over_minus_one_does_not_overflow) {
    detail::Bits<8> min_val(uint8_t{0x80});  // -128
    detail::Bits<8> minus_one(uint8_t{0xFF});

    EXPECT_EQ(detail::div_signed(min_val, minus_one).to_decimal_string(true), "128");
}

// rem follows the dividend's sign (C), mod follows the divisor's (VHDL/Python).
TEST(TestBitsGrowing, rem_and_mod_differ_on_mixed_signs) {
    detail::Bits<8> neg56(uint8_t{200});   // -56
    detail::Bits<8> pos100(uint8_t{100});  // 100
    detail::Bits<8> neg3(uint8_t{0xFD});   // -3
    detail::Bits<8> pos56(uint8_t{56});

    EXPECT_EQ(detail::rem_signed(neg56, pos100).to_decimal_string(true), "-56");
    EXPECT_EQ(detail::mod_signed(neg56, pos100).to_decimal_string(true), "44");

    EXPECT_EQ(detail::rem_signed(pos56, neg3).to_decimal_string(true), "2");
    EXPECT_EQ(detail::mod_signed(pos56, neg3).to_decimal_string(true), "-1");

    // Same signs: rem and mod agree.
    EXPECT_EQ(detail::rem_signed(neg56, neg3).to_decimal_string(true), "-2");
    EXPECT_EQ(detail::mod_signed(neg56, neg3).to_decimal_string(true), "-2");
}

TEST(TestBitsGrowing, unary_negate_and_abs_grow_by_one) {
    detail::Bits<8> neg56(uint8_t{200});

    auto n = detail::negate_signed(neg56);
    static_assert(std::is_same_v<decltype(n), detail::Bits<9>>);
    EXPECT_EQ(n.to_decimal_string(true), "56");
    EXPECT_EQ(detail::abs_signed(neg56).to_decimal_string(true), "56");

    // The growth is what makes abs(signed_min) representable.
    detail::Bits<8> min_val(uint8_t{0x80});
    EXPECT_EQ(detail::abs_signed(min_val).to_decimal_string(true), "128");
}

TEST(TestBitsGrowing, wide_operands) {
    detail::Bits<200> a("0x1234567890ABCDEF1122334455667788AABBCCDD");
    detail::Bits<104> b("0xFEDCBA98765432100123456789");

    EXPECT_EQ(
        detail::add_unsigned(a, b).to_decimal_string(),
        "103929005307927776916288754849918835164314678374"
    );
    EXPECT_EQ(
        detail::mul_unsigned(a, b).to_decimal_string(),
        "20985620746650105667944919267254862992694182466051081766005088452816326800540"
        "85"
    );
    EXPECT_EQ(detail::div_unsigned(a, b).to_decimal_string(), "5146971002046463");
    EXPECT_EQ(
        detail::rem_unsigned(a, b).to_decimal_string(), "20112278405973339191843622874214"
    );
}

// Growing ops on the null vector produce a real (if zero) value rather than
// being a compile error: the result width is genuinely non-zero.
TEST(TestBitsGrowing, zero_width_operands) {
    detail::Bits<0> n{};
    detail::Bits<8> a(uint8_t{42});

    EXPECT_EQ(detail::add_unsigned(n, n).to_decimal_string(), "0");
    static_assert(std::is_same_v<decltype(detail::add_unsigned(n, n)), detail::Bits<1>>);

    auto p = detail::mul_unsigned(n, a);
    static_assert(std::is_same_v<decltype(p), detail::Bits<8>>);
    EXPECT_EQ(p.to_decimal_string(), "0");

    // A null divisor is a zero divisor.
    EXPECT_THROW(detail::div_unsigned(a, n), std::domain_error);
}

TEST(TestBitsGrowing, usable_in_constant_expressions) {
    constexpr detail::Bits<8> a(uint8_t{200});
    constexpr detail::Bits<8> b(uint8_t{7});
    constexpr auto q = detail::div_unsigned(a, b);
    constexpr auto r = detail::rem_unsigned(a, b);
    static_assert(detail::add_unsigned(a, b) == detail::Bits<9>(uint16_t{207}));
    static_assert(detail::mul_unsigned(a, b) == detail::Bits<16>(uint16_t{1400}));
    static_assert(q == detail::Bits<9>(uint16_t{28}));
    static_assert(r == detail::Bits<8>(uint8_t{4}));
    SUCCEED();
}

// LCOV_EXCL_BR_STOP

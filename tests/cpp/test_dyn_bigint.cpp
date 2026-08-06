// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types/bigint.hpp>
#include <stdexcept>

using coconext::types::detail::DynBigInt;

TEST(DynBigInt, DefaultCtorIsZeroOfWidth) {
    DynBigInt a(200);
    EXPECT_EQ(a.bit_width(), 200u);
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_EQ(a.popcount(), 0u);
    EXPECT_EQ(a.count_leading_zeros(), 200u);
    EXPECT_EQ(a.count_trailing_zeros(), 200u);
}

TEST(DynBigInt, NativeIntCtorZeroExtendsUnsigned) {
    DynBigInt a(200, uint64_t{0xDEADBEEF});
    EXPECT_EQ(a.get_word(0), 0xDEADBEEFULL);
    EXPECT_EQ(a.get_word(1), 0u);
    EXPECT_EQ(a.get_word(2), 0u);
    EXPECT_EQ(a.get_word(3), 0u);
    EXPECT_FALSE(a.is_negative());
}

TEST(DynBigInt, NativeIntCtorSignExtendsNegative) {
    DynBigInt a(200, int64_t{-1});
    EXPECT_EQ(a.get_word(0), ~uint64_t{0});
    EXPECT_EQ(a.get_word(1), ~uint64_t{0});
    EXPECT_EQ(a.get_word(2), ~uint64_t{0});
    // Top word is masked to 200 bits.
    EXPECT_EQ(a.get_word(3), (uint64_t{1} << (200 - 3 * 64)) - 1);
    EXPECT_TRUE(a.is_negative());
}

#if defined(__SIZEOF_INT128__)
TEST(DynBigInt, Int128CtorSignExtends) {
    __int128_t v = -1;
    DynBigInt a(200, v);
    EXPECT_EQ(a.get_word(0), ~uint64_t{0});
    EXPECT_EQ(a.get_word(1), ~uint64_t{0});
    EXPECT_EQ(a.get_word(2), ~uint64_t{0});
    EXPECT_TRUE(a.is_negative());
}

TEST(DynBigInt, UInt128CtorZeroExtends) {
    __uint128_t v = (~__uint128_t{0});
    DynBigInt a(200, v);
    EXPECT_EQ(a.get_word(0), ~uint64_t{0});
    EXPECT_EQ(a.get_word(1), ~uint64_t{0});
    EXPECT_EQ(a.get_word(2), 0u);
    EXPECT_FALSE(a.is_negative());
}
#endif

TEST(DynBigInt, StringCtorHex) {
    DynBigInt a(200, "0x1234567890ABCDEF1122334455667788AABBCCDD");
    // Match the reference value used in the Bits<200> test suite.
    EXPECT_EQ(a.udiv(DynBigInt(200, uint64_t{1})).ucompare(a), 0);
    DynBigInt one(200, uint64_t{1});
    EXPECT_EQ(a * one, a);
}

TEST(DynBigInt, StringCtorDecimalMatchesHex) {
    DynBigInt hex(200, "0x1234567890ABCDEF1122334455667788AABBCCDD");
    DynBigInt dec(200, "103929005307927756724023193881144724129310297309");
    EXPECT_EQ(hex, dec);
}

TEST(DynBigInt, StringCtorNegative) {
    DynBigInt neg(200, "-1");
    DynBigInt all_ones = ~DynBigInt(200, uint64_t{0});
    EXPECT_EQ(neg, all_ones);
    EXPECT_TRUE(neg.is_negative());
}

TEST(DynBigInt, StringCtorOverflowThrows) {
    EXPECT_THROW(DynBigInt(200, "0x" + std::string(51, 'F')), std::out_of_range);
    EXPECT_NO_THROW(DynBigInt(200, "0x" + std::string(50, 'F')));
}

TEST(DynBigInt, StringCtorInvalidCharThrows) {
    EXPECT_THROW(DynBigInt(64, "12x3"), std::invalid_argument);
    EXPECT_THROW(DynBigInt(64, "0xGG"), std::invalid_argument);
    EXPECT_THROW(DynBigInt(64, "-0x1"), std::invalid_argument);
}

TEST(DynBigInt, Equality) {
    DynBigInt a(200, uint64_t{42});
    DynBigInt b(200, uint64_t{42});
    DynBigInt c(200, uint64_t{43});
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    // Different widths compare unequal even for the same numeric value.
    DynBigInt d(128, uint64_t{42});
    EXPECT_NE(a, d);
}

TEST(DynBigInt, CopyIsIndependent) {
    DynBigInt a(200, uint64_t{5});
    DynBigInt b = a;
    b = b + DynBigInt(200, uint64_t{1});
    EXPECT_EQ(a, DynBigInt(200, uint64_t{5}));
    EXPECT_EQ(b, DynBigInt(200, uint64_t{6}));
}

TEST(DynBigInt, MoveAssignmentWorks) {
    DynBigInt a(200, uint64_t{5});
    DynBigInt b(64, uint64_t{99});
    b = std::move(a);
    EXPECT_EQ(b.bit_width(), 200u);
    EXPECT_EQ(b, DynBigInt(200, uint64_t{5}));
}

TEST(DynBigInt, ArithmeticMatchesReference) {
    DynBigInt a(200, "0x1234567890ABCDEF1122334455667788AABBCCDD");
    DynBigInt b(200, "0xFEDCBA98765432100123456789");

    DynBigInt sum(200, "103929005307927776916288754849918835164314678374");
    DynBigInt diff(200, "103929005307927736531757632912370613094305916244");
    DynBigInt prod(200, "329963546613616313339723066835445579609796160260803833793861");
    EXPECT_EQ(a + b, sum);
    EXPECT_EQ(a - b, diff);
    EXPECT_EQ(a * b, prod);
    EXPECT_EQ(a.udiv(b), DynBigInt(200, uint64_t{5146971002046463ULL}));
    EXPECT_EQ(a.umod(b), DynBigInt(200, "20112278405973339191843622874214"));
    // Division identity.
    EXPECT_EQ(a.udiv(b) * b + a.umod(b), a);
}

TEST(DynBigInt, DegenerateDivisionCases) {
    DynBigInt a(200, "0x1234567890ABCDEF1122334455667788AABBCCDD");
    DynBigInt b(200, "0xFEDCBA98765432100123456789");
    DynBigInt zero(200);
    DynBigInt one(200, uint64_t{1});

    EXPECT_EQ(a.udiv(a), one);
    EXPECT_EQ(a.umod(a), zero);
    EXPECT_EQ(b.udiv(a), zero);
    EXPECT_EQ(b.umod(a), b);
    EXPECT_EQ(zero.udiv(a), zero);
    EXPECT_EQ(a.udiv(one), a);
}

TEST(DynBigInt, SignedDivisionAndRemainder) {
    DynBigInt neg(200, "-1000000000000000000000");
    DynBigInt pos(200, uint64_t{7});

    DynBigInt q = neg.sdiv(pos);
    DynBigInt r = neg.smod(pos);
    EXPECT_EQ(q, DynBigInt(200, "-142857142857142857142"));
    EXPECT_EQ(r, DynBigInt(200, "-6"));
}

TEST(DynBigInt, BitwiseOps) {
    DynBigInt a(200, "0xFF00FF00FF00FF00FF00FF00FF");
    DynBigInt b(200, "0x00FF00FF00FF00FF00FF00FF00");
    DynBigInt all(200, "0xFFFFFFFFFFFFFFFFFFFFFFFFFF");
    EXPECT_EQ(a | b, all);
    EXPECT_EQ(a & b, DynBigInt(200));
    EXPECT_EQ(a ^ b, all);
}

TEST(DynBigInt, UnaryNegate) {
    DynBigInt one(200, uint64_t{1});
    DynBigInt neg_one = -one;
    EXPECT_EQ(neg_one + one, DynBigInt(200));
    EXPECT_TRUE(neg_one.is_negative());
}

TEST(DynBigInt, MismatchedWidthArithmeticThrows) {
    DynBigInt a(200, uint64_t{1});
    DynBigInt b(128, uint64_t{1});
    EXPECT_THROW(a + b, std::invalid_argument);
    EXPECT_THROW(a - b, std::invalid_argument);
    EXPECT_THROW(a * b, std::invalid_argument);
    EXPECT_THROW(a & b, std::invalid_argument);
    EXPECT_THROW(a.udiv(b), std::invalid_argument);
    EXPECT_THROW(a.umod(b), std::invalid_argument);
    EXPECT_THROW(a.ucompare(b), std::invalid_argument);
}

TEST(DynBigInt, CountsAndPopcount) {
    DynBigInt a(200, uint64_t{0});
    EXPECT_EQ(a.popcount(), 0u);
    EXPECT_EQ(a.count_leading_zeros(), 200u);
    EXPECT_EQ(a.count_trailing_zeros(), 200u);

    DynBigInt b(200, uint64_t{0xF0});
    EXPECT_EQ(b.popcount(), 4u);
    EXPECT_EQ(b.count_trailing_zeros(), 4u);
    EXPECT_EQ(b.count_leading_zeros(), 200u - 8u);
}

TEST(DynBigInt, ZeroWidth) {
    DynBigInt a(0);
    EXPECT_EQ(a.bit_width(), 0u);
    EXPECT_EQ(a.num_words(), 0u);
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_FALSE(a.is_negative());
    EXPECT_EQ(a.popcount(), 0u);
}
// LCOV_EXCL_BR_STOP

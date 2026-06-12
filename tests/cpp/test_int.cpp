// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <type_traits>

using namespace coconext::types;

#ifdef COCONEXT_USE_APINT

// Test SInt/UInt with APInt as backend

#else

#if defined(__SIZEOF_INT128__)

TEST(TestBigInt, JustAbove128) {
    static_assert(
        std::is_same_v<detail::IntBackend<129, false>::StorageType, detail::BigInt>
    );
    static_assert(
        std::is_same_v<detail::IntBackend<129, true>::StorageType, detail::BigInt>
    );

    static_assert(
        !std::is_same_v<detail::IntBackend<127, false>::StorageType, detail::BigInt>
    );
    static_assert(
        !std::is_same_v<detail::IntBackend<128, true>::StorageType, detail::BigInt>
    );
}

#else

TEST(TestBigInt, JustAbove64) {
    static_assert(
        std::is_same_v<detail::IntBackend<65, false>::StorageType, detail::BigInt>
    );
    static_assert(
        std::is_same_v<detail::IntBackend<65, true>::StorageType, detail::BigInt>
    );

    static_assert(std::is_same_v<detail::IntBackend<63, false>::StorageType, uint64_t>);
    static_assert(std::is_same_v<detail::IntBackend<64, true>::StorageType, uint64_t>);
}

#endif  // defined(__SIZEOF_INT128__)

TEST(TestBigIntBackend, Equality) {
    detail::BigInt a(347, false, 0x1234);
    detail::BigInt b(347, false, 0x1234);
    detail::BigInt c(347, false, 0x5678);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);

    detail::BigInt d(347, false, 0x1234);
    detail::BigInt e(347, false, 0x1234);
    detail::BigInt f(347, false, 0x5678);

    EXPECT_TRUE(d == e);
    EXPECT_FALSE(d == f);
    EXPECT_TRUE(d != f);
}

TEST(TestBigIntBackend, not_op) {
    // a = 0x[23 unused bits]4F33[48 0s]9FF0[48 0s]BD73[48 0s]9AF0[25 0s]
    // expected = 0x[23 unused bits]B0CC[48 1s]600F[48 1s]428C[48 1s]650F[25 1s]

    detail::BigInt a(233, false, 0x4F33);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0x9FF0);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0xBD73);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0x9AF0);
    a = detail::shift_left(a, 25);

    detail::BigInt expected(233, false, 0xFFFFFFFFFFFFB0CCULL);
    expected = detail::shift_left(expected, 48);
    expected = expected | detail::BigInt(233, false, 0xFFFFFFFF600FULL);
    expected = detail::shift_left(expected, 48);
    expected = expected | detail::BigInt(233, false, 0xFFFFFFFF428CULL);
    expected = detail::shift_left(expected, 48);
    expected = expected | detail::BigInt(233, false, 0xFFFFFFFF650FULL);
    expected = detail::shift_left(expected, 25);
    expected = expected | detail::BigInt(233, false, 0x1FFFFFFULL);

    detail::BigInt result = ~a;
    EXPECT_EQ(result, expected);
}

TEST(TestBigIntBackend, xor) {
    // a = 0x[23 unused bits]4F33[48 0s]9FF0[48 0s]BD73[48 0s]9AF0[25 0s]
    // b = 0x[23 unused bits]A6D2[48 0s]73C1[48 0s]E84F[48 0s]15B9[25 0s]
    // c = 0x[23 unused bits]E9E1[48 0s]EC31[48 0s]553C[48 0s]8F49[25 0s]

    detail::BigInt a(233, false, 0x4F33);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0x9FF0);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0xBD73);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0x9AF0);
    a = detail::shift_left(a, 25);

    detail::BigInt b(233, false, 0xA6D2);
    b = detail::shift_left(b, 48);
    b = b | detail::BigInt(233, false, 0x73C1);
    b = detail::shift_left(b, 48);
    b = b | detail::BigInt(233, false, 0xE84F);
    b = detail::shift_left(b, 48);
    b = b | detail::BigInt(233, false, 0x15B9);
    b = detail::shift_left(b, 25);

    detail::BigInt c(233, false, 0xE9E1);
    c = detail::shift_left(c, 48);
    c = c | detail::BigInt(233, false, 0xEC31);
    c = detail::shift_left(c, 48);
    c = c | detail::BigInt(233, false, 0x553C);
    c = detail::shift_left(c, 48);
    c = c | detail::BigInt(233, false, 0x8F49);
    c = detail::shift_left(c, 25);

    detail::BigInt result(a ^ b);

    EXPECT_EQ(result, c);
}

TEST(TestBigIntBackend, shift_left) {
    detail::BigInt a(233, false, 0xABCD);

    detail::BigInt expected(233, false, 0);
    expected.data[0] = 0;
    expected.data[1] = 0xABCDULL << 16;

    detail::BigInt result = detail::shift_left(a, 80);
    EXPECT_EQ(result, expected);

    detail::BigInt zero(233, false, 0);
    EXPECT_EQ(detail::shift_left(a, 233), zero);
    EXPECT_EQ(detail::shift_left(a, 250), zero);
}

TEST(TestBigIntBackend, shift_right) {
    detail::BigInt a(233, false, 0);
    a.data[1] = 0xABCDULL << 16;

    detail::BigInt expected_logical(233, false, 0xABCD);
    detail::BigInt result_logical = detail::shift_right_logical(a, 80);
    EXPECT_EQ(result_logical, expected_logical);

    detail::BigInt zero(233, false, 0);
    EXPECT_EQ(detail::shift_right_logical(a, 233), zero);
    EXPECT_EQ(detail::shift_right_logical(a, 250), zero);

    detail::BigInt b(233, true, -5);

    detail::BigInt expected_arith(233, true, -2);
    detail::BigInt result_arith = detail::shift_right_arith(b, 2);
    EXPECT_EQ(result_arith, expected_arith);

    detail::BigInt expected_arith_max(233, true, -1);
    EXPECT_EQ(detail::shift_right_arith(b, 233), expected_arith_max);
    EXPECT_EQ(detail::shift_right_arith(b, 250), expected_arith_max);

    detail::BigInt c(233, true, 0xABCD);
    c = detail::shift_left(c, 80);

    detail::BigInt result_arith_pos = detail::shift_right_arith(c, 80);
    detail::BigInt expected_arith_pos(233, true, 0xABCD);
    EXPECT_EQ(result_arith_pos, expected_arith_pos);
}

TEST(TestBigIntBackend, completeWidthOperations) {
    // a = 0x[23 unused bits]4F33[48 0s]9FF0[48 0s]BD73[48 0s]9AF0[25 0s]
    // b = 0x[23 unused bits]A6D2[48 0s]73C1[48 0s]E84F[48 0s]15B9[25 0s]
    // c = 0x[23 unused bits]0612[48 0s]13C0[48 0s]A843[48 0s]10B0[25 0s]

    detail::BigInt a(233, false, 0x4F33);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0x9FF0);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0xBD73);
    a = detail::shift_left(a, 48);
    a = a | detail::BigInt(233, false, 0x9AF0);
    a = detail::shift_left(a, 25);

    detail::BigInt b0(233, false, 0xA6D2);
    detail::BigInt b(b0);
    b = detail::shift_left(b, 48);
    b = b | detail::BigInt(233, false, 0x73C1);
    b = detail::shift_left(b, 48);
    b = b | detail::BigInt(233, false, 0xE84F);
    b = detail::shift_left(b, 48);
    b = b | detail::BigInt(233, false, 0x15B9);
    b = detail::shift_left(b, 25);

    detail::BigInt c0(233, false, 0x0612);
    detail::BigInt c = c0;
    c = detail::shift_left(c, 48);
    c = c | detail::BigInt(233, false, 0x13C0);
    c = detail::shift_left(c, 48);
    c = c | detail::BigInt(233, false, 0xA843);
    c = detail::shift_left(c, 48);
    c = c | detail::BigInt(233, false, 0x10B0);
    c = detail::shift_left(c, 25);

    detail::BigInt d(std::move(c));

    detail::BigInt result(a & b);
    EXPECT_EQ(result, d);

    bool check = b >= a && b > a && d < a && d <= a;
    EXPECT_EQ(check, true);
}

#endif  // COCONEXT_USE_APINT

// LCOV_EXCL_BR_STOP

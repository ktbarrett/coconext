// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

using namespace coconext::types;

TEST(TestUnsigned, Constructors) {
    static_assert(!std::is_convertible_v<int, Unsigned<6>>);
    // static_assert(!std::is_convertible_v<int, Signed<6>>);

    Unsigned<4> a(10);
    EXPECT_EQ(static_cast<uint8_t>(a), 10U);
    EXPECT_EQ(a.size(), 4U);

    BitArray<5> arr_a({'0'_b, '1'_b, '0'_b, '0'_b, '1'_b});
    Unsigned<5> u_arr_a(arr_a);
    EXPECT_EQ(static_cast<uint32_t>(u_arr_a), 9U);

    //
}

TEST(TestUnsigned, ImplicitBitArrayConversion) {
    Unsigned<6> a(31);
    BitArray<5, 0> b = a;

    //
}

TEST(TestUnsigned, as_overloads) {
    BitArray<5> arr_a({'0'_b, '1'_b, '0'_b, '0'_b, '1'_b});

    auto a = as<Unsigned<5>>(arr_a);
    BitArray<4, 0> arr_exp = a;
    static_assert(std::is_same_v<decltype(a), Unsigned<5>>);
    EXPECT_EQ(static_cast<uint8_t>(a), 9U);
    EXPECT_EQ(arr_a, arr_exp);

    Unsigned<5> a1;
    a1 = as(arr_a);
    BitArray<4, 0> arr_exp_a1 = a1;
    EXPECT_EQ(static_cast<uint8_t>(a1), 9U);
    EXPECT_EQ(arr_a, arr_exp_a1);
}

TEST(TestUnsigned, Comparisons) {
    Unsigned<8> a(10);
    Unsigned<8> b(10);
    Unsigned<8> c(20);
    Unsigned<8> d(5);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);

    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a != b);

    EXPECT_TRUE(d < a);
    EXPECT_FALSE(a < d);
    EXPECT_FALSE(a < b);

    EXPECT_TRUE(d <= a);
    EXPECT_TRUE(a <= b);
    EXPECT_FALSE(c <= a);

    EXPECT_TRUE(c > a);
    EXPECT_FALSE(a > c);
    EXPECT_FALSE(a > b);

    EXPECT_TRUE(c >= a);
    EXPECT_TRUE(a >= b);
    EXPECT_FALSE(d >= a);

    EXPECT_EQ(a <=> b, std::strong_ordering::equal);
    EXPECT_EQ(d <=> a, std::strong_ordering::less);
    EXPECT_EQ(c <=> a, std::strong_ordering::greater);
}

// TEST(TestUnsigned, RangeAccessor) {
//     // Length-only sugar defaults to DOWNTO (HDL convention).
//     EXPECT_EQ(Unsigned<8>::range(), (Range{7, Direction::DOWNTO, 0}));
// }

// TEST(TestUnsigned, RangeFormsMirrorLogicArray) {
//     // Length-only -> DOWNTO.
//     static_assert(
//         std::is_same_v<Unsigned<8>, detail::Unsigned<Range{7, Direction::DOWNTO, 0}>>
//     );
//     // 2-arg with L > R: auto-DOWNTO (same as generic).
//     static_assert(
//         std::is_same_v<Unsigned<7, 0>, detail::Unsigned<Range{7, Direction::DOWNTO, 0}>>
//     );
//     // 2-arg L == R: defaults to DOWNTO (the case where it differs from generic).
//     static_assert(
//         std::is_same_v<Unsigned<3, 3>, detail::Unsigned<Range{3, Direction::DOWNTO, 3}>>
//     );
//     // 2-arg L < R: keeps generic TO auto-direction.
//     static_assert(
//         std::is_same_v<Unsigned<0, 7>, detail::Unsigned<Range{0, Direction::TO, 7}>>
//     );
//     // 3-arg explicit direction is respected.
//     static_assert(std::is_same_v<
//                   Unsigned<0, Direction::TO, 7>,
//                   detail::Unsigned<Range{0, Direction::TO, 7}>>);
//     // Explicit Range NTTP passes through.
//     static_assert(std::is_same_v<
//                   Unsigned<Range{15, Direction::DOWNTO, 8}>,
//                   detail::Unsigned<Range{15, Direction::DOWNTO, 8}>>);
// }

// TEST(TestUnsigned, ConstructZeroDefault) {
//     Unsigned<8> a;
//     EXPECT_EQ(a.value(), 0U);
// }

// TEST(TestUnsigned, ConstructBoundary) {
//     Unsigned<4> a(15);
//     EXPECT_EQ(a.value(), 15U);
// }

// TEST(TestUnsigned, ConstructOverflowThrows) {
//     EXPECT_THROW(Unsigned<4>(16), std::out_of_range);
//     EXPECT_THROW(Unsigned<4>(100), std::out_of_range);
// }

// TEST(TestUnsigned, ConstructNegativeThrows) {
//     EXPECT_THROW(Unsigned<4>(-1), std::out_of_range);
// }

// TEST(TestUnsigned, ConstructFullWidth) {
//     Unsigned<64> a(std::numeric_limits<uint64_t>::max());
//     EXPECT_EQ(a.value(), std::numeric_limits<uint64_t>::max());
// }

// TEST(TestUnsigned, CrossWidthConstruct) {
//     Unsigned<4> small(5);
//     Unsigned<8> big(small);
//     EXPECT_EQ(big.value(), 5U);
//     EXPECT_EQ(big.width(), 8U);
// }

// TEST(TestUnsigned, CrossWidthNarrowingThrows) {
//     Unsigned<8> big(200);
//     EXPECT_THROW(Unsigned<4>{big}, std::out_of_range);
// }

// // -- Conversion out --------------------------------------------------------

// TEST(TestUnsigned, ToNativeInt) {
//     Unsigned<8> a(200);
//     EXPECT_EQ(a.to<int>(), 200);
//     EXPECT_EQ(a.to<uint64_t>(), 200U);
// }

// TEST(TestUnsigned, ToNativeOverflowThrows) {
//     Unsigned<16> a(40000);
//     EXPECT_THROW((void)a.to<int8_t>(), std::out_of_range);
//     EXPECT_THROW((void)a.to<uint8_t>(), std::out_of_range);
// }

// // -- Arithmetic wrap -------------------------------------------------------

// TEST(TestUnsigned, AddWraps) {
//     EXPECT_EQ((Unsigned<4>(15) + Unsigned<4>(1)).value(), 0U);
//     EXPECT_EQ((Unsigned<4>(8) + Unsigned<4>(9)).value(), 1U);
// }

// TEST(TestUnsigned, SubWraps) { EXPECT_EQ((Unsigned<4>(0) - Unsigned<4>(1)).value(), 15U);
// }

// TEST(TestUnsigned, MulWraps) {
//     EXPECT_EQ((Unsigned<4>(6) * Unsigned<4>(3)).value(), 2U);  // 18 mod 16
// }

// TEST(TestUnsigned, DivFloor) { EXPECT_EQ((Unsigned<8>(17) / Unsigned<8>(5)).value(), 3U);
// }

// TEST(TestUnsigned, ModRemainder) {
//     EXPECT_EQ((Unsigned<8>(17) % Unsigned<8>(5)).value(), 2U);
// }

// TEST(TestUnsigned, DivByZeroThrows) {
//     EXPECT_THROW(Unsigned<8>(1) / Unsigned<8>(0), std::domain_error);
// }

// TEST(TestUnsigned, ModByZeroThrows) {
//     EXPECT_THROW(Unsigned<8>(1) % Unsigned<8>(0), std::domain_error);
// }

// TEST(TestUnsigned, FullWidthArithmeticNoOp) {
//     Unsigned<64> a(std::numeric_limits<uint64_t>::max());
//     EXPECT_EQ((a + Unsigned<64>(1)).value(), 0U);  // wraps at 2^64
// }

// // -- Mixed-width result type -----------------------------------------------

// TEST(TestUnsigned, MixedWidthResultIsMax) {
//     auto c = Unsigned<4>(3) + Unsigned<8>(200);
//     static_assert(std::is_same_v<decltype(c), Unsigned<8>>);
//     EXPECT_EQ(c.value(), 203U);
//     EXPECT_EQ(c.range(), (Range{7, Direction::DOWNTO, 0}));
// }

// TEST(TestUnsigned, MixedWidthWrapsToMaxWidth) {
//     auto c = Unsigned<4>(15) + Unsigned<8>(250);
//     static_assert(std::is_same_v<decltype(c), Unsigned<8>>);
//     EXPECT_EQ(c.value(), 9U);  // 265 mod 256
// }

// // -- Unary -----------------------------------------------------------------

// TEST(TestUnsigned, UnaryNegateWraps) {
//     EXPECT_EQ((-Unsigned<4>(1)).value(), 15U);
//     EXPECT_EQ((-Unsigned<4>(0)).value(), 0U);
// }

// TEST(TestUnsigned, UnaryPlus) { EXPECT_EQ((+Unsigned<4>(7)).value(), 7U); }

// TEST(TestUnsigned, BitwiseNot) {
//     EXPECT_EQ((~Unsigned<4>(0)).value(), 15U);
//     EXPECT_EQ((~Unsigned<4>(10)).value(), 5U);
// }

// TEST(TestUnsigned, IncrementDecrement) {
//     Unsigned<4> a(15);
//     EXPECT_EQ((++a).value(), 0U);
//     EXPECT_EQ((a++).value(), 0U);
//     EXPECT_EQ(a.value(), 1U);
//     Unsigned<4> b(0);
//     EXPECT_EQ((--b).value(), 15U);
//     EXPECT_EQ((b--).value(), 15U);
//     EXPECT_EQ(b.value(), 14U);
// }

// // -- Shifts ----------------------------------------------------------------

// TEST(TestUnsigned, ShiftLeftDropsHighBits) {
//     EXPECT_EQ((Unsigned<4>(0b0011) << 2).value(), 0b1100U);
//     EXPECT_EQ((Unsigned<4>(0b0011) << 3).value(), 0b1000U);  // top bit dropped
// }

// TEST(TestUnsigned, ShiftRightLogical) {
//     EXPECT_EQ((Unsigned<4>(0b1100) >> 2).value(), 0b0011U);
// }

// TEST(TestUnsigned, ShiftPastWidthIsZero) {
//     EXPECT_EQ((Unsigned<4>(15) << 64).value(), 0U);
//     EXPECT_EQ((Unsigned<4>(15) >> 64).value(), 0U);
// }

// TEST(TestUnsigned, ShiftNegativeThrows) {
//     EXPECT_THROW(Unsigned<4>(1) << -1, std::invalid_argument);
//     EXPECT_THROW(Unsigned<4>(1) >> -1, std::invalid_argument);
// }

// // -- Bitwise ---------------------------------------------------------------

// TEST(TestUnsigned, BitwiseOps) {
//     EXPECT_EQ((Unsigned<4>(0b1100) & Unsigned<4>(0b1010)).value(), 0b1000U);
//     EXPECT_EQ((Unsigned<4>(0b1100) | Unsigned<4>(0b1010)).value(), 0b1110U);
//     EXPECT_EQ((Unsigned<4>(0b1100) ^ Unsigned<4>(0b1010)).value(), 0b0110U);
// }

// // -- Compound assignment ---------------------------------------------------

// TEST(TestUnsigned, CompoundAssign) {
//     Unsigned<4> a(10);
//     a += Unsigned<4>(7);
//     EXPECT_EQ(a.value(), 1U);  // 17 mod 16
//     a -= Unsigned<4>(2);
//     EXPECT_EQ(a.value(), 15U);
//     a *= Unsigned<4>(2);
//     EXPECT_EQ(a.value(), 14U);  // 30 mod 16
//     a <<= 1;
//     EXPECT_EQ(a.value(), 12U);  // 28 mod 16
//     a >>= 2;
//     EXPECT_EQ(a.value(), 3U);
// }

// TEST(TestUnsigned, CompoundAssignKeepsLhsWidth) {
//     Unsigned<4> a(15);
//     a += Unsigned<8>(250);  // result wrapped to width 4
//     EXPECT_EQ(a.value(), 9U);
//     EXPECT_EQ(a.width(), 4U);
// }

// // -- Comparisons -----------------------------------------------------------

// TEST(TestUnsigned, Comparisons) {
//     EXPECT_TRUE(Unsigned<4>(3) == Unsigned<8>(3));
//     EXPECT_TRUE(Unsigned<4>(3) != Unsigned<4>(4));
//     EXPECT_TRUE(Unsigned<4>(3) < Unsigned<4>(4));
//     EXPECT_TRUE(Unsigned<4>(4) <= Unsigned<8>(4));
//     EXPECT_TRUE(Unsigned<8>(200) > Unsigned<4>(15));
//     EXPECT_TRUE(Unsigned<4>(15) >= Unsigned<4>(15));
// }

// // -- Formatter / hash ------------------------------------------------------

// TEST(TestUnsigned, Formatter) { EXPECT_EQ(std::format("{}", Unsigned<8>(200)), "200"); }

// TEST(TestUnsigned, Hashable) {
//     std::hash<Unsigned<8>> h;
//     EXPECT_EQ(h(Unsigned<8>(5)), h(Unsigned<8>(5)));
//     std::unordered_set<Unsigned<8>> s{Unsigned<8>(1), Unsigned<8>(1), Unsigned<8>(2)};
//     EXPECT_EQ(s.size(), 2U);
// }

// LCOV_EXCL_BR_STOP

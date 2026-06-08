// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

using namespace coconext::types;

// -- Construction ----------------------------------------------------------

TEST(TestSigned, ConstructInRange) {
    Signed<4> a(-3);
    EXPECT_EQ(a.value(), -3);
    EXPECT_EQ(a.width(), 4U);
}

TEST(TestSigned, RangeAccessor) {
    EXPECT_EQ(Signed<8>::range(), (Range{7, Direction::DOWNTO, 0}));
}

TEST(TestSigned, RangeFormsMirrorLogicArray) {
    static_assert(
        std::is_same_v<Signed<8>, detail::Signed<Range{7, Direction::DOWNTO, 0}>>
    );
    static_assert(
        std::is_same_v<Signed<7, 0>, detail::Signed<Range{7, Direction::DOWNTO, 0}>>
    );
    static_assert(
        std::is_same_v<Signed<3, 3>, detail::Signed<Range{3, Direction::DOWNTO, 3}>>
    );
    static_assert(std::is_same_v<Signed<0, 7>, detail::Signed<Range{0, Direction::TO, 7}>>);
}

TEST(TestSigned, ConstructZeroDefault) {
    Signed<8> a;
    EXPECT_EQ(a.value(), 0);
}

TEST(TestSigned, ConstructBoundaries) {
    EXPECT_EQ(Signed<4>(7).value(), 7);    // max
    EXPECT_EQ(Signed<4>(-8).value(), -8);  // min
}

TEST(TestSigned, ConstructOverflowThrows) {
    EXPECT_THROW(Signed<4>(8), std::out_of_range);
    EXPECT_THROW(Signed<4>(-9), std::out_of_range);
}

TEST(TestSigned, ConstructFullWidth) {
    Signed<64> a(std::numeric_limits<int64_t>::min());
    EXPECT_EQ(a.value(), std::numeric_limits<int64_t>::min());
    Signed<64> b(std::numeric_limits<int64_t>::max());
    EXPECT_EQ(b.value(), std::numeric_limits<int64_t>::max());
}

TEST(TestSigned, ConstructFromLargeUnsignedThrows) {
    auto const big = std::numeric_limits<uint64_t>::max();
    EXPECT_THROW(Signed<8>{big}, std::out_of_range);
}

TEST(TestSigned, CrossWidthConstruct) {
    Signed<4> small(-3);
    Signed<8> big(small);
    EXPECT_EQ(big.value(), -3);
    EXPECT_EQ(big.width(), 8U);
}

TEST(TestSigned, CrossWidthNarrowingThrows) {
    Signed<8> big(100);
    EXPECT_THROW(Signed<4>{big}, std::out_of_range);
}

// -- Conversion out --------------------------------------------------------

TEST(TestSigned, ToNativeInt) {
    Signed<8> a(-100);
    EXPECT_EQ(a.to<int>(), -100);
    EXPECT_EQ(a.to<int64_t>(), -100);
}

TEST(TestSigned, ToNativeOverflowThrows) {
    Signed<16> a(-30000);
    EXPECT_THROW((void)a.to<int8_t>(), std::out_of_range);
    EXPECT_THROW((void)a.to<uint8_t>(), std::out_of_range);  // negative into unsigned
}

// -- Arithmetic wrap -------------------------------------------------------

TEST(TestSigned, AddWraps) {
    EXPECT_EQ((Signed<4>(7) + Signed<4>(1)).value(), -8);  // overflow wraps
    EXPECT_EQ((Signed<4>(3) + Signed<4>(2)).value(), 5);
}

TEST(TestSigned, SubWraps) {
    EXPECT_EQ((Signed<4>(-8) - Signed<4>(1)).value(), 7);  // underflow wraps
}

TEST(TestSigned, MulWraps) {
    EXPECT_EQ((Signed<4>(4) * Signed<4>(4)).value(), 0);  // 16 wraps to 0
    EXPECT_EQ((Signed<4>(-2) * Signed<4>(3)).value(), -6);
}

TEST(TestSigned, DivTruncatesTowardZero) {
    EXPECT_EQ((Signed<8>(-7) / Signed<8>(2)).value(), -3);
}

TEST(TestSigned, ModFollowsCpp) { EXPECT_EQ((Signed<8>(-7) % Signed<8>(2)).value(), -1); }

TEST(TestSigned, DivByZeroThrows) {
    EXPECT_THROW(Signed<8>(1) / Signed<8>(0), std::domain_error);
}

TEST(TestSigned, ModByZeroThrows) {
    EXPECT_THROW(Signed<8>(1) % Signed<8>(0), std::domain_error);
}

TEST(TestSigned, MinDivNegOneWraps) {
    // -8 / -1 = 8, which wraps to -8 in 4-bit two's complement.
    EXPECT_EQ((Signed<4>(-8) / Signed<4>(-1)).value(), -8);
}

TEST(TestSigned, FullWidthArithmeticNoOp) {
    Signed<64> a(std::numeric_limits<int64_t>::max());
    EXPECT_EQ((a + Signed<64>(1)).value(), std::numeric_limits<int64_t>::min());
}

// -- Mixed-width result type -----------------------------------------------

TEST(TestSigned, MixedWidthResultIsMax) {
    auto c = Signed<4>(-3) + Signed<8>(100);
    static_assert(std::is_same_v<decltype(c), Signed<8>>);
    EXPECT_EQ(c.value(), 97);
}

TEST(TestSigned, MixedWidthNegativePreserved) {
    // Narrower negative operand sign-extends into the wider result.
    auto c = Signed<4>(-1) + Signed<8>(0);
    static_assert(std::is_same_v<decltype(c), Signed<8>>);
    EXPECT_EQ(c.value(), -1);
}

// -- Unary -----------------------------------------------------------------

TEST(TestSigned, UnaryNegate) {
    EXPECT_EQ((-Signed<4>(3)).value(), -3);
    EXPECT_EQ((-Signed<4>(-8)).value(), -8);  // negating min wraps to itself
}

TEST(TestSigned, UnaryPlus) { EXPECT_EQ((+Signed<4>(-5)).value(), -5); }

TEST(TestSigned, BitwiseNot) {
    EXPECT_EQ((~Signed<4>(0)).value(), -1);  // ~0 = all ones = -1
    EXPECT_EQ((~Signed<4>(-1)).value(), 0);
}

TEST(TestSigned, IncrementDecrement) {
    Signed<4> a(7);
    EXPECT_EQ((++a).value(), -8);  // wraps
    Signed<4> b(-8);
    EXPECT_EQ((--b).value(), 7);  // wraps
    EXPECT_EQ((b--).value(), 7);
    EXPECT_EQ(b.value(), 6);
}

// -- Shifts ----------------------------------------------------------------

TEST(TestSigned, ShiftLeftDropsHighBits) {
    EXPECT_EQ((Signed<4>(3) << 1).value(), 6);
    EXPECT_EQ((Signed<4>(3) << 2).value(), -4);  // 0b1100 = -4
}

TEST(TestSigned, ShiftRightArithmetic) {
    EXPECT_EQ((Signed<8>(-8) >> 1).value(), -4);  // sign-extends
    EXPECT_EQ((Signed<8>(-1) >> 3).value(), -1);  // stays all ones
    EXPECT_EQ((Signed<8>(16) >> 2).value(), 4);
}

TEST(TestSigned, ShiftRightPastWidthIsSign) {
    EXPECT_EQ((Signed<8>(-1) >> 64).value(), -1);  // negative -> all ones
    EXPECT_EQ((Signed<8>(5) >> 64).value(), 0);    // positive -> 0
}

TEST(TestSigned, ShiftNegativeThrows) {
    EXPECT_THROW(Signed<4>(1) << -1, std::invalid_argument);
    EXPECT_THROW(Signed<4>(1) >> -1, std::invalid_argument);
}

// -- Bitwise ---------------------------------------------------------------

TEST(TestSigned, BitwiseOps) {
    EXPECT_EQ((Signed<4>(0b0110) & Signed<4>(0b0011)).value(), 0b0010);
    EXPECT_EQ((Signed<4>(0b0100) | Signed<4>(0b0001)).value(), 0b0101);
    EXPECT_EQ((Signed<4>(0b0110) ^ Signed<4>(0b0011)).value(), 0b0101);
}

// -- Compound assignment ---------------------------------------------------

TEST(TestSigned, CompoundAssign) {
    Signed<8> a(10);
    a += Signed<8>(5);
    EXPECT_EQ(a.value(), 15);
    a -= Signed<8>(20);
    EXPECT_EQ(a.value(), -5);
    a *= Signed<8>(-2);
    EXPECT_EQ(a.value(), 10);
}

TEST(TestSigned, CompoundAssignKeepsLhsWidth) {
    Signed<4> a(7);
    a += Signed<8>(1);  // wraps to width 4
    EXPECT_EQ(a.value(), -8);
    EXPECT_EQ(a.width(), 4U);
}

// -- Comparisons -----------------------------------------------------------

TEST(TestSigned, Comparisons) {
    EXPECT_TRUE(Signed<4>(-3) == Signed<8>(-3));
    EXPECT_TRUE(Signed<4>(-3) != Signed<4>(3));
    EXPECT_TRUE(Signed<4>(-3) < Signed<4>(2));
    EXPECT_TRUE(Signed<8>(-100) < Signed<4>(0));
    EXPECT_TRUE(Signed<4>(7) > Signed<4>(-8));
    EXPECT_TRUE(Signed<4>(5) >= Signed<4>(5));
}

// -- Formatter / hash ------------------------------------------------------

TEST(TestSigned, Formatter) { EXPECT_EQ(std::format("{}", Signed<8>(-100)), "-100"); }

TEST(TestSigned, Hashable) {
    std::hash<Signed<8>> h;
    EXPECT_EQ(h(Signed<8>(-5)), h(Signed<8>(-5)));
    std::unordered_set<Signed<8>> s{Signed<8>(-1), Signed<8>(-1), Signed<8>(2)};
    EXPECT_EQ(s.size(), 2U);
}

// -- DynSigned -------------------------------------------------------------

TEST(TestDynSigned, Construct) {
    DynSigned a(-3, 4);
    EXPECT_EQ(a.value(), -3);
    EXPECT_EQ(a.width(), 4U);
    EXPECT_EQ(a.range(), (Range{3, Direction::DOWNTO, 0}));
}

TEST(TestDynSigned, ConstructFromExplicitRange) {
    DynSigned a(-3, Range(15, Direction::DOWNTO, 12));
    EXPECT_EQ(a.value(), -3);
    EXPECT_EQ(a.range(), (Range{15, Direction::DOWNTO, 12}));
}

TEST(TestDynSigned, ConstructOverflowThrows) {
    EXPECT_THROW(DynSigned(8, 4), std::out_of_range);
    EXPECT_THROW(DynSigned(-9, 4), std::out_of_range);
}

TEST(TestDynSigned, BadWidthThrows) {
    EXPECT_THROW(DynSigned(0, 0), std::out_of_range);
    EXPECT_THROW(DynSigned(0, 65), std::out_of_range);
}

TEST(TestDynSigned, ArithmeticWrapAndMaxWidth) {
    auto c = DynSigned(7, 4) + DynSigned(1, 8);
    EXPECT_EQ(c.width(), 8U);
    EXPECT_EQ(c.value(), 8);  // no wrap at width 8
}

TEST(TestDynSigned, AddWrapsAtOwnWidth) {
    EXPECT_EQ((DynSigned(7, 4) + DynSigned(1, 4)).value(), -8);
}

TEST(TestDynSigned, DivModByZeroThrows) {
    EXPECT_THROW(DynSigned(1, 8) / DynSigned(0, 8), std::domain_error);
    EXPECT_THROW(DynSigned(1, 8) % DynSigned(0, 8), std::domain_error);
}

TEST(TestDynSigned, ShiftRightArithmetic) {
    EXPECT_EQ((DynSigned(-8, 8) >> 1).value(), -4);
    EXPECT_EQ((DynSigned(-1, 8) >> 64).value(), -1);
    EXPECT_THROW(DynSigned(1, 4) << -1, std::invalid_argument);
}

TEST(TestDynSigned, Bitwise) {
    EXPECT_EQ((~DynSigned(0, 4)).value(), -1);
    EXPECT_EQ((DynSigned(0b0110, 4) & DynSigned(0b0011, 4)).value(), 0b0010);
}

TEST(TestDynSigned, CompoundAssign) {
    DynSigned a(7, 4);
    a += DynSigned(1, 4);
    EXPECT_EQ(a.value(), -8);
    EXPECT_EQ(a.width(), 4U);
}

TEST(TestDynSigned, Comparisons) {
    EXPECT_TRUE(DynSigned(-3, 4) == DynSigned(-3, 8));
    EXPECT_TRUE(DynSigned(-3, 4) < DynSigned(2, 4));
}

TEST(TestDynSigned, ToNative) {
    EXPECT_EQ(DynSigned(-100, 8).to<int>(), -100);
    EXPECT_THROW((void)DynSigned(-30000, 16).to<int8_t>(), std::out_of_range);
}

TEST(TestDynSigned, Formatter) { EXPECT_EQ(std::format("{}", DynSigned(-42, 8)), "-42"); }
// LCOV_EXCL_BR_STOP

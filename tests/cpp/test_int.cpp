// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <type_traits>

using namespace coconext::types;

TEST(TestIntStorage, SizeMatchesNativeType) {
    static_assert(sizeof(UInt<8>) == sizeof(uint8_t));
    static_assert(sizeof(UInt<16>) == sizeof(uint16_t));
    static_assert(sizeof(UInt<32>) == sizeof(uint32_t));
    static_assert(sizeof(UInt<64>) == sizeof(uint64_t));

    static_assert(sizeof(SInt<8>) == sizeof(int8_t));
    static_assert(sizeof(SInt<16>) == sizeof(int16_t));
    static_assert(sizeof(SInt<32>) == sizeof(int32_t));
    static_assert(sizeof(SInt<64>) == sizeof(int64_t));
}

TEST(TestIntStorage, ZeroWidthNoUniqueAddress) {
    struct Container {
        [[no_unique_address]] UInt<0> zero;
        uint8_t other;
    };
    static_assert(sizeof(Container) == sizeof(uint8_t));
}

TEST(TestUInt, StaticMetadata) {
    static_assert(UInt<8>::num_bits == 8);
    static_assert(UInt<16>::num_bits == 16);
    static_assert(UInt<32>::num_bits == 32);
    static_assert(UInt<64>::num_bits == 64);
    static_assert(UInt<347>::num_bits == 347);
    static_assert(UInt<8>::is_signed == false);
}

TEST(TestSInt, StaticMetadata) {
    static_assert(SInt<8>::num_bits == 8);
    static_assert(SInt<64>::num_bits == 64);
    static_assert(SInt<347>::num_bits == 347);
    static_assert(SInt<8>::is_signed == true);
}

TEST(TestUInt8, DefaultConstruct) {
    UInt<8> a;
    (void)a;
}

TEST(TestUInt8, ConstructFromValue) {
    EXPECT_EQ(UInt<8>(-1), UInt<8>(255));

    EXPECT_EQ(UInt<8>(static_cast<uint8_t>(256)), UInt<8>(0));
    EXPECT_EQ(UInt<8>(static_cast<uint8_t>(257)), UInt<8>(1));
}

TEST(TestUInt8, Addition) {
    EXPECT_EQ(UInt<8>(10) + UInt<8>(5), UInt<8>(15));
    EXPECT_EQ(UInt<8>(255) + UInt<8>(1), UInt<8>(0));
    EXPECT_EQ(UInt<8>(200) + UInt<8>(100), UInt<8>(44));
}

TEST(TestUInt8, Subtraction) {
    EXPECT_EQ(UInt<8>(10) - UInt<8>(5), UInt<8>(5));
    EXPECT_EQ(UInt<8>(0) - UInt<8>(1), UInt<8>(255));
}

TEST(TestUInt8, Multiplication) { EXPECT_EQ(UInt<8>(10) * UInt<8>(5), UInt<8>(50)); }

TEST(TestUInt8, Division) {
    EXPECT_EQ(UInt<8>(10) / UInt<8>(5), UInt<8>(2));
    EXPECT_EQ(UInt<8>(10) / UInt<8>(3), UInt<8>(3));
}

TEST(TestUInt8, Modulo) {
    EXPECT_EQ(UInt<8>(10) % UInt<8>(3), UInt<8>(1));
    EXPECT_EQ(UInt<8>(10) % UInt<8>(5), UInt<8>(0));
}

TEST(TestUInt8, BitwiseAnd) {
    EXPECT_EQ(UInt<8>(0b10101010) & UInt<8>(0b11001100), UInt<8>(0b10001000));
}

TEST(TestUInt8, BitwiseOr) {
    EXPECT_EQ(UInt<8>(0b10101010) | UInt<8>(0b11001100), UInt<8>(0b11101110));
}

TEST(TestUInt8, BitwiseXor) {
    EXPECT_EQ(UInt<8>(0b10101010) ^ UInt<8>(0b11001100), UInt<8>(0b01100110));
}

TEST(TestUInt8, BitwiseNot) {
    EXPECT_EQ(~UInt<8>(0b00001111), UInt<8>(0b11110000));
    EXPECT_EQ(~UInt<8>(0), UInt<8>(255));
    EXPECT_EQ(~UInt<8>(255), UInt<8>(0));
}

TEST(TestUInt8, ShiftLeft) {
    EXPECT_EQ(UInt<8>(1) << UInt<8>(1), UInt<8>(2));
    EXPECT_EQ(UInt<8>(1) << UInt<8>(3), UInt<8>(8));
    EXPECT_EQ(UInt<8>(1) << UInt<8>(7), UInt<8>(128));
}

TEST(TestUInt8, ShiftRight) {
    EXPECT_EQ(UInt<8>(8) >> UInt<8>(3), UInt<8>(1));
    EXPECT_EQ(UInt<8>(128) >> UInt<8>(7), UInt<8>(1));
}

TEST(TestUInt8, Equality) {
    EXPECT_EQ(UInt<8>(42), UInt<8>(42));
    EXPECT_FALSE(UInt<8>(42) == UInt<8>(43));
}

TEST(TestUInt8, Inequality) {
    EXPECT_NE(UInt<8>(1), UInt<8>(2));
    EXPECT_FALSE(UInt<8>(5) != UInt<8>(5));
}

TEST(TestUInt8, LessThan) {
    EXPECT_LT(UInt<8>(5), UInt<8>(10));
    EXPECT_FALSE(UInt<8>(10) < UInt<8>(5));
    EXPECT_FALSE(UInt<8>(5) < UInt<8>(5));
}

TEST(TestUInt8, GreaterThan) {
    EXPECT_GT(UInt<8>(10), UInt<8>(5));
    EXPECT_FALSE(UInt<8>(5) > UInt<8>(10));
    EXPECT_FALSE(UInt<8>(5) > UInt<8>(5));
}

TEST(TestUInt8, LessEqual) {
    EXPECT_LE(UInt<8>(5), UInt<8>(10));
    EXPECT_LE(UInt<8>(5), UInt<8>(5));
    EXPECT_FALSE(UInt<8>(10) <= UInt<8>(5));
}

TEST(TestUInt8, GreaterEqual) {
    EXPECT_GE(UInt<8>(10), UInt<8>(5));
    EXPECT_GE(UInt<8>(5), UInt<8>(5));
    EXPECT_FALSE(UInt<8>(5) >= UInt<8>(10));
}

TEST(TestUInt32, ArithmeticAndBitwise) {
    EXPECT_EQ(UInt<32>(1'000'000) + UInt<32>(7), UInt<32>(1'000'007));
    EXPECT_EQ(UInt<32>(100) * UInt<32>(200), UInt<32>(20'000));
    EXPECT_EQ(UInt<32>(1'000) / UInt<32>(10), UInt<32>(100));
    EXPECT_EQ(UInt<32>(1'000) % UInt<32>(7), UInt<32>(1000 % 7));
    EXPECT_EQ(UInt<32>(0xDEAD) & UInt<32>(0xFF00), UInt<32>(0xDE00));
    EXPECT_EQ(UInt<32>(0xDEAD) | UInt<32>(0xFF00), UInt<32>(0xFFAD));
    EXPECT_EQ(UInt<32>(0xDEAD) ^ UInt<32>(0xFF00), UInt<32>(0x21AD));
}

TEST(TestUInt64, ShiftFullWidth) {
    EXPECT_EQ(UInt<64>(1) << UInt<64>(63), UInt<64>(0x8000000000000000ULL));
    EXPECT_EQ(UInt<64>(0x8000000000000000ULL) >> UInt<64>(63), UInt<64>(1));
}

TEST(TestUInt64, BitwiseCombine) {
    UInt<64> hi(0xDEADBEEF00000000ULL);
    UInt<64> lo(0x00000000DEADBEEFULL);
    EXPECT_EQ(hi | lo, UInt<64>(0xDEADBEEFDEADBEEFULL));
    EXPECT_EQ(hi & lo, UInt<64>(0));
    EXPECT_EQ(hi ^ lo, UInt<64>(0xDEADBEEFDEADBEEFULL));
}

TEST(TestSInt8, ConstructPositive) {
    SInt<8> a(42);
    EXPECT_EQ(a, SInt<8>(42));
}

TEST(TestSInt8, ConstructNegative) {
    SInt<8> a(-42);
    EXPECT_EQ(a, SInt<8>(-42));
}

TEST(TestSInt8, ConstructMin) {
    SInt<8> a(-128);
    EXPECT_EQ(a, SInt<8>(-128));
}

TEST(TestSInt8, ConstructMax) {
    SInt<8> a(127);
    EXPECT_EQ(a, SInt<8>(127));
}

TEST(TestSInt8, AdditionWithNegative) {
    EXPECT_EQ(SInt<8>(10) + SInt<8>(-5), SInt<8>(5));
    EXPECT_EQ(SInt<8>(-10) + SInt<8>(-5), SInt<8>(-15));
    EXPECT_EQ(SInt<8>(-10) + SInt<8>(10), SInt<8>(0));
}

TEST(TestSInt8, SubtractionWithNegative) {
    EXPECT_EQ(SInt<8>(10) - SInt<8>(-5), SInt<8>(15));
    EXPECT_EQ(SInt<8>(-10) - SInt<8>(5), SInt<8>(-15));
}

TEST(TestSInt8, MultiplicationSigned) {
    EXPECT_EQ(SInt<8>(10) * SInt<8>(-2), SInt<8>(-20));
    EXPECT_EQ(SInt<8>(-10) * SInt<8>(-2), SInt<8>(20));
    EXPECT_EQ(SInt<8>(-10) * SInt<8>(0), SInt<8>(0));
}

TEST(TestSInt8, DivisionSigned) {
    EXPECT_EQ(SInt<8>(-10) / SInt<8>(2), SInt<8>(-5));
    EXPECT_EQ(SInt<8>(10) / SInt<8>(-2), SInt<8>(-5));
    EXPECT_EQ(SInt<8>(-10) / SInt<8>(-2), SInt<8>(5));
}

TEST(TestSInt8, ModuloSigned) {
    EXPECT_EQ(SInt<8>(-7) % SInt<8>(3), SInt<8>(-7 % 3));
    EXPECT_EQ(SInt<8>(7) % SInt<8>(-3), SInt<8>(7 % -3));
}

TEST(TestSInt8, ComparisonsAcrossZero) {
    EXPECT_LT(SInt<8>(-1), SInt<8>(0));
    EXPECT_LT(SInt<8>(-128), SInt<8>(127));
    EXPECT_GT(SInt<8>(0), SInt<8>(-1));
    EXPECT_GT(SInt<8>(1), SInt<8>(-1));
}

TEST(TestSInt8, EqualityNegative) {
    EXPECT_EQ(SInt<8>(-5), SInt<8>(-5));
    EXPECT_NE(SInt<8>(-5), SInt<8>(5));
}

TEST(TestSInt8, LessEqualGreaterEqual) {
    EXPECT_LE(SInt<8>(-5), SInt<8>(-4));
    EXPECT_LE(SInt<8>(-5), SInt<8>(-5));
    EXPECT_GE(SInt<8>(5), SInt<8>(4));
    EXPECT_GE(SInt<8>(5), SInt<8>(5));
}

TEST(TestSInt8, BitwiseAnd) {
    EXPECT_EQ(SInt<8>(0b00001111) & SInt<8>(0b11110000), SInt<8>(0));
}

TEST(TestSInt8, BitwiseOrProducesNegative) {
    // 0x0F | 0xF0 = 0xFF = -1 as int8_t
    EXPECT_EQ(SInt<8>(0b00001111) | SInt<8>(static_cast<int8_t>(0b11110000)), SInt<8>(-1));
}

TEST(TestSInt32, LargeValues) {
    EXPECT_EQ(SInt<32>(2'147'483'647) + SInt<32>(-2'147'483'647), SInt<32>(0));
    EXPECT_EQ(SInt<32>(100) * SInt<32>(-100), SInt<32>(-10'000));
    EXPECT_EQ(SInt<32>(-100) / SInt<32>(10), SInt<32>(-10));
    EXPECT_LT(SInt<32>(-2'147'483'647), SInt<32>(2'147'483'647));
}

TEST(TestSInt64, LargeValues) {
    EXPECT_EQ(
        SInt<64>(1'000'000'000LL) * SInt<64>(1'000'000'000LL),
        SInt<64>(1'000'000'000'000'000'000LL)
    );
    EXPECT_EQ(SInt<64>(0) - SInt<64>(1), SInt<64>(-1));
    EXPECT_LT(
        SInt<64>(-9'223'372'036'854'775'807LL), SInt<64>(9'223'372'036'854'775'807LL)
    );
}

TEST(TestBigInt, DefaultConstructor) {
    UInt<347> a;
    SInt<347> b;
    UInt<200> c;
    SInt<200> d;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}

TEST(TestBigIntStorage, Metadata) {
    static_assert(UInt<347>::num_bits == 347);
    static_assert(SInt<347>::num_bits == 347);
}

#ifdef COCONEXT_USE_APINT

// TODO
// This is the place to test SInt/UInt
// with APInt as backend

#else

#if defined(__SIZEOF_INT128__)
TEST(TestBigInt, JustAbove128) {
    UInt<129> a;
    (void)a;

    static_assert(std::is_same_v<detail::Storage<129, false>::StorageType, detail::BigInt>);

    static_assert(std::is_same_v<detail::Storage<129, true>::StorageType, detail::BigInt>);
}
#else
TEST(TestBigInt, JustAbove64) {
    UInt<65> a;
    (void)a;

    static_assert(std::is_same_v<detail::Storage<65, false>::StorageType, detail::BigInt>);

    static_assert(std::is_same_v<detail::Storage<65, true>::StorageType, detail::BigInt>);
}
#endif  // defined(__SIZEOF_INT128__)

TEST(TestBigInt, Equality) {
    SInt<347> a(0x1234);
    SInt<347> b(0x1234);
    SInt<347> c(0x5678);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
}

TEST(TestBigInt, BitwiseAnd) {
    SInt<462> a(0b10101010);
    SInt<462> b(0b11001100);
    auto s_result = a & b;
    SInt<462> s_exp(0b10001000);

    UInt<462> c(0b10101010);
    UInt<462> d(0b11001100);
    auto u_result = c & d;
    UInt<462> u_exp(0b10001000);

    EXPECT_EQ(s_result, s_exp);
    EXPECT_EQ(u_result, u_exp);
}

TEST(TestBigInt, BitwiseOr) {
    SInt<462> a(0b10101010);
    SInt<462> b(0b11001100);
    auto s_result = a | b;
    SInt<462> s_exp(0b11101110);

    UInt<462> c(0b10101010);
    UInt<462> d(0b11001100);
    auto u_result = c | d;
    UInt<462> u_exp(0b11101110);

    EXPECT_EQ(s_result, s_exp);
    EXPECT_EQ(u_result, u_exp);
}

TEST(TestBigInt, BitwiseXor) {
    SInt<462> a(0b10101010);
    SInt<462> b(0b11001100);
    auto s_result = a ^ b;
    SInt<462> s_exp(0b01100110);

    UInt<462> c(0b10101010);
    UInt<462> d(0b11001100);
    auto u_result = c ^ d;
    UInt<462> u_exp(0b01100110);

    EXPECT_EQ(s_result, s_exp);
    EXPECT_EQ(u_result, u_exp);
}

TEST(TestBigInt, BitwiseNot) {
    SInt<277> a(0);
    auto s_result = ~a;
    auto s_exp = ~SInt<277>(0);
    EXPECT_EQ(s_result, s_exp);

    UInt<277> b(0);
    auto u_result = ~b;
    auto u_exp = ~UInt<277>(0);
    EXPECT_EQ(u_result, u_exp);
}

TEST(TestBigInt, LogicalShiftRight) {
    SInt<344> a(0x80);
    auto s_result_backend = detail::shift_right_logical(a.get_backend().raw(), 7);
    SInt<344> s_result(s_result_backend);
    SInt<344> s_exp(1);
    EXPECT_EQ(s_result, s_exp);

    UInt<344> b(0x80);
    auto u_result_backend = detail::shift_right_logical(b.get_backend().raw(), 7);
    UInt<344> u_result(u_result_backend);
    UInt<344> u_exp(1);
    EXPECT_EQ(u_result, u_exp);
}

TEST(TestBigInt, ShiftLeft) {
    SInt<347> a(1);
    auto s_result_backend = detail::shift_left(a.get_backend().raw(), 8);
    SInt<347> s_result(s_result_backend);
    SInt<347> s_exp(256);
    EXPECT_EQ(s_result, s_exp);

    UInt<347> b(1);
    auto u_result_backend = detail::shift_left(b.get_backend().raw(), 8);
    UInt<347> u_result(u_result_backend);
    UInt<347> u_exp(256);
    EXPECT_EQ(u_result, u_exp);
}

TEST(TestBigInt, ArithmeticShiftRightPositive) {
    SInt<347> a(0x80);

    auto s_result_backend = detail::shift_right_arith(a.get_backend().raw(), 7);
    SInt<347> s_result(s_result_backend);
    SInt<347> s_exp(1);

    EXPECT_EQ(s_result, s_exp);
}

TEST(TestBigInt, ShiftRightBeyondWidth) {
    SInt<347> a(0x1234);
    auto s_result_backend = detail::shift_right_logical(a.get_backend().raw(), 500);
    SInt<347> s_result(s_result_backend);
    SInt<347> s_exp(0);
    EXPECT_EQ(s_result, s_exp);

    UInt<347> b(0x1234);
    auto u_result_backend = detail::shift_right_logical(b.get_backend().raw(), 500);
    UInt<347> u_result(u_result_backend);
    UInt<347> u_exp(0);
    EXPECT_EQ(u_result, u_exp);
}

#endif  // COCONEXT_USE_APINT

// LCOV_EXCL_BR_STOP

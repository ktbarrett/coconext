// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

using namespace coconext::types;
using namespace coconext::literals;

TEST(TestUnsigned, Constructors) {
    static_assert(!std::is_convertible_v<int, Unsigned<6>>);

    Unsigned<4> a(15);
    EXPECT_EQ(static_cast<uint8_t>(a), 15U);
    EXPECT_EQ(a.size(), 4U);

    EXPECT_THROW(Unsigned<4>(16), std::out_of_range);
    EXPECT_THROW(Unsigned<4>(-1), std::out_of_range);

    Unsigned<8> large_val(200);
    Unsigned<8> small_val(10);

    Unsigned<4> narrow_fit(small_val);
    EXPECT_EQ(static_cast<uint8_t>(narrow_fit), 10U);

    EXPECT_THROW(Unsigned<4> narrow_fail(large_val), std::out_of_range);

    BitArray<5> arr_a({'0'_b, '1'_b, '0'_b, '0'_b, '1'_b});
    Unsigned<5> u_arr_a(arr_a);
    EXPECT_EQ(static_cast<uint32_t>(u_arr_a), 9U);

    Unsigned<5> arr_a_exp("01001"_b);
    EXPECT_EQ(u_arr_a, arr_a_exp);

    Signed<20> s(2000);
    Unsigned<20> u(s);

    EXPECT_EQ(static_cast<uint32_t>(u), 2000U);
}

TEST(TestUnsigned, ImplicitBitArrayConversion) {
    Unsigned<6> a(31);
    BitArray<5, 0> b = a;

    EXPECT_EQ(b[0], '1'_b);
    EXPECT_EQ(b[4], '1'_b);
    EXPECT_EQ(b[5], '0'_b);
}

TEST(TestUnsigned, ExplicitNativeCasts) {
    Unsigned<16> a(42000);

    EXPECT_TRUE(static_cast<bool>(a));
    EXPECT_FALSE(static_cast<bool>(Unsigned<16>(0)));

    EXPECT_EQ(static_cast<int>(a), 42000);
    EXPECT_EQ(static_cast<unsigned int>(a), 42000U);
    EXPECT_EQ(static_cast<long long>(a), 42000LL);
    EXPECT_EQ(static_cast<unsigned long long>(a), 42000ULL);

    Unsigned<4> b(10);
    EXPECT_EQ(static_cast<unsigned char>(b), 10);
    EXPECT_EQ(static_cast<signed char>(b), 10);
}

TEST(TestUnsigned, ArithmeticOperators) {
    Unsigned<8> a(150);
    Unsigned<8> b(50);

    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), Unsigned<9>>);
    EXPECT_EQ(static_cast<int>(sum), 200);

    auto prod = a * b;
    static_assert(std::is_same_v<decltype(prod), Unsigned<16>>);
    EXPECT_EQ(static_cast<int>(prod), 7500);

    auto div = a / b;
    EXPECT_EQ(static_cast<int>(div), 3);

    auto mod = a % Unsigned<4>(7);
    EXPECT_EQ(static_cast<int>(mod), 150 % 7);

    EXPECT_THROW(a / Unsigned<4>(0), std::domain_error);
    EXPECT_THROW(a % Unsigned<8>(0), std::domain_error);

    auto sub_pos = a - b;
    auto sub_neg = b - a;
    static_assert(std::is_same_v<decltype(sub_pos), Signed<9>>);
    static_assert(std::is_same_v<decltype(sub_neg), Signed<9>>);
    EXPECT_EQ(static_cast<int16_t>(sub_pos), 100);
    EXPECT_EQ(static_cast<int16_t>(sub_neg), -100);
}

TEST(UnsignedMixedSignednessTest, CompoundAssignmentOperators) {
    auto u1 = u8(15);
    u1 += s8(-5);
    EXPECT_EQ(u1, u8(10));

    auto u2 = u8(250);
    u2 += s8(10);
    EXPECT_EQ(u2, u8(4));

    auto u3 = u8(5);
    u3 -= s8(10);
    EXPECT_EQ(u3, u8(251));

    auto u4 = u8(10);
    u4 *= s8(-3);
    EXPECT_EQ(u4, u8(226));

    auto u5 = u8(20);
    u5 /= s8(-4);
    EXPECT_EQ(u5, u8(251));

    auto u6 = u8(23);
    u6 %= s8(-7);
    EXPECT_EQ(u6, u8(2));

    auto u7 = u8(50);
    EXPECT_THROW(u7 /= s8(0), std::domain_error);
    EXPECT_THROW(u7 %= s8(0), std::domain_error);
}

TEST(TestUnsigned, as_overloads) {
    BitArray<5> arr_a({'0'_b, '1'_b, '0'_b, '0'_b, '1'_b});

    auto a = as<Unsigned<5>>(arr_a);
    BitArray<4, 0> arr_exp = a;
    static_assert(std::is_same_v<decltype(a), Unsigned<5>>);
    EXPECT_EQ(static_cast<uint8_t>(a), 9U);  // explicit conversion to int
    EXPECT_EQ(arr_a, arr_exp);

    Unsigned<5> a1;
    a1 = as(arr_a);
    BitArray<4, 0> arr_exp_a1 = a1;
    EXPECT_EQ(static_cast<uint8_t>(a1), 9U);
    EXPECT_EQ(arr_a, arr_exp_a1);
}

TEST(TestUnsigned, resize_overloads) {
    Unsigned<8> small(200);

    auto wide_spelled = resize<16>(small);
    static_assert(std::is_same_v<decltype(wide_spelled), Unsigned<16>>);
    EXPECT_EQ(static_cast<uint16_t>(wide_spelled), 200U);

    Unsigned<16> wide_deduced;
    wide_deduced = resize(small);
    EXPECT_EQ(static_cast<uint16_t>(wide_deduced), 200U);

    Unsigned<16> wide(1000);

    auto narrow_wrap_spelled = resize<8>(wide);
    static_assert(std::is_same_v<decltype(narrow_wrap_spelled), Unsigned<8>>);
    EXPECT_EQ(static_cast<uint8_t>(narrow_wrap_spelled), 232U);

    Unsigned<8> narrow_wrap_deduced;
    narrow_wrap_deduced = resize(wide);
    EXPECT_EQ(static_cast<uint8_t>(narrow_wrap_deduced), 232U);

    auto narrow_sat_spelled = resize<8>(wide, overflow_mode::saturate);
    EXPECT_EQ(static_cast<uint8_t>(narrow_sat_spelled), 255U);

    Unsigned<8> narrow_sat_deduced;
    narrow_sat_deduced = resize(wide, overflow_mode::saturate);
    EXPECT_EQ(static_cast<uint8_t>(narrow_sat_deduced), 255U);

    Unsigned<8> copy_init_deduced = resize(wide, overflow_mode::saturate);
    EXPECT_EQ(static_cast<uint8_t>(copy_init_deduced), 255U);
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
}

TEST(TestUnsigned, CompoundAssignment) {
    Unsigned<8> a(10);
    Unsigned<4> b(5);

    a += b;
    EXPECT_EQ(static_cast<int>(a), 15);

    a -= Unsigned<8>(3);
    EXPECT_EQ(static_cast<int>(a), 12);

    a *= Unsigned<2>(2);
    EXPECT_EQ(static_cast<int>(a), 24);

    a /= Unsigned<4>(4);
    EXPECT_EQ(static_cast<int>(a), 6);

    a %= Unsigned<4>(4);
    EXPECT_EQ(static_cast<int>(a), 2);

    a += 10;
    EXPECT_EQ(static_cast<int>(a), 12);

    EXPECT_THROW(a /= 0, std::domain_error);
}

TEST(TestUnsigned, IncrementDecrement) {
    Unsigned<8> a(10);

    EXPECT_EQ(static_cast<int>(++a), 11);
    EXPECT_EQ(static_cast<int>(a), 11);

    EXPECT_EQ(static_cast<int>(--a), 10);
    EXPECT_EQ(static_cast<int>(a), 10);

    EXPECT_EQ(static_cast<int>(a++), 10);
    EXPECT_EQ(static_cast<int>(a), 11);

    EXPECT_EQ(static_cast<int>(a--), 11);
    EXPECT_EQ(static_cast<int>(a), 10);

    Unsigned<4> max_val(15);
    max_val++;
    EXPECT_EQ(static_cast<int>(max_val), 0);

    Unsigned<4> min_val(0);
    min_val--;
    EXPECT_EQ(static_cast<int>(min_val), 15);
}

TEST(TestUnsigned, ShiftOperators) {
    Unsigned<8> a(5);

    auto sl = a << 2;
    EXPECT_EQ(static_cast<int>(sl), 20);

    auto sr = a >> 1;
    EXPECT_EQ(static_cast<int>(sr), 2);

    EXPECT_EQ(static_cast<int>(a << 8), 0);
    EXPECT_EQ(static_cast<int>(a >> 10), 0);

    a <<= 3;
    EXPECT_EQ(static_cast<int>(a), 40);
    a >>= 2;
    EXPECT_EQ(static_cast<int>(a), 10);

    EXPECT_THROW(a << -1, std::invalid_argument);
    EXPECT_THROW(a >> -2, std::invalid_argument);

    Unsigned<4> shift_amt(2);
    EXPECT_EQ(static_cast<int>(a << shift_amt), 40);
}

TEST(TestUnsigned, Iterators) {
    Unsigned<4> a(8);  // 1000

    std::vector<int> bit_vals;
    for (auto bit : a) {
        bit_vals.push_back(static_cast<bool>(bit) ? 1 : 0);
    }

    EXPECT_EQ(bit_vals.size(), 4);
    EXPECT_EQ(bit_vals[0], 1);
    EXPECT_EQ(bit_vals[1], 0);
    EXPECT_EQ(bit_vals[2], 0);
    EXPECT_EQ(bit_vals[3], 0);

    std::vector<int> rbit_vals;
    for (auto rit = a.rbegin(); rit != a.rend(); ++rit) {
        rbit_vals.push_back(static_cast<bool>(*rit) ? 1 : 0);
    }

    std::vector<int> expected_rvals = {0, 0, 0, 1};
    EXPECT_EQ(rbit_vals, expected_rvals);

    std::reverse(rbit_vals.begin(), rbit_vals.end());
    EXPECT_EQ(rbit_vals, bit_vals);

    auto it = a.begin();
    EXPECT_EQ(static_cast<bool>(*(it + 1)), false);
    EXPECT_EQ(static_cast<bool>(it[0]), true);
}

TEST(TestUnsigned, index_operator) {
    Unsigned<4> a(2);  // 0010

    EXPECT_FALSE(static_cast<bool>(a[3]));
    EXPECT_FALSE(static_cast<bool>(a[2]));
    EXPECT_TRUE(static_cast<bool>(a[1]));
    EXPECT_FALSE(static_cast<bool>(a[0]));

    EXPECT_THROW(a[4], std::out_of_range);
}

TEST(TestUnsigned, index) {
    Unsigned<4> a(6);  // 0110

    EXPECT_FALSE(static_cast<bool>(a.index<3>()));
    EXPECT_TRUE(static_cast<bool>(a.index<2>()));
    EXPECT_TRUE(static_cast<bool>(a.index<1>()));
    EXPECT_FALSE(static_cast<bool>(a.index<0>()));

    EXPECT_EQ(static_cast<int>(a.index<3>()), 0);
    EXPECT_EQ(static_cast<int>(a.index<1>()), 1);
}

TEST(TestUnsigned, Bitwise_ops) {
    // test if Unsigned support Bitwise operations via
    // LogicArrayType and RangedSequence
    Unsigned<8> a(10);
    Unsigned<8> b(10);

    auto and_result = a & b;
    EXPECT_EQ(and_result, BitArray<8>("00001010"_b));
}

TEST(TestUnsigned, const_udl) {
    static_assert(std::is_same_v<decltype(u8(0)), Unsigned<8>>, "u8 type mismatch");
    static_assert(std::is_same_v<decltype(u16(0)), Unsigned<16>>, "u16 type mismatch");
    static_assert(std::is_same_v<decltype(u32(0)), Unsigned<32>>, "u32 type mismatch");
    static_assert(std::is_same_v<decltype(u64(0)), Unsigned<64>>, "u64 type mismatch");

    static_assert(static_cast<int>(u8(0)) == 0, "u8 min value failed");
    static_assert(static_cast<int>(u8(5)) == 5, "u8 mid value failed");
    static_assert(static_cast<int>(u8(255)) == 255, "u8 max value failed");

    static_assert(static_cast<int>(u16(0)) == 0, "u16 min value failed");
    static_assert(static_cast<int>(u16(65535)) == 65535, "u16 max value failed");

    static_assert(static_cast<long long>(u32(0)) == 0, "u32 min value failed");
    static_assert(
        static_cast<long long>(u32(4294967295ULL)) == 4294967295ULL, "u32 max value failed"
    );

    static_assert(static_cast<unsigned long long>(u64(0)) == 0ULL, "u64 min value failed");
    static_assert(
        static_cast<unsigned long long>(u64(18446744073709551615ULL))
            == 18446744073709551615ULL,
        "u64 max value failed"
    );

    static_assert(u8(42) == u8(42), "u8 equality failed");
    static_assert(u16(1024) == u16(1024), "u16 equality failed");
}

TEST(TestUnsigned, Formatter) {
    Unsigned<10> small(102);
    Unsigned<39> mid(0x0AFFFE9001);
    Unsigned<139> very_large(0x0AFFFE90010AFFFE);
    Unsigned<139> chunk1(0x0AFFFE9001);  // Top 40 bits
    Unsigned<139> chunk2(0x0AFFFE9001);  // Next 40 bits
    Unsigned<139> chunk3(0x0AFFFE9001);  // Next 40 bits
    Unsigned<139> chunk4(0xFFFFF);       // Bottom 20 bits

    very_large = as<Unsigned<139>>(
        as<Unsigned<139>>(as<Unsigned<139>>(chunk1 << 100 | chunk2 << 60) | chunk3 << 20)
        | chunk4
    );

    EXPECT_EQ(std::format("{:b}", small), "Unsigned[9 downto 0]{0001100110}");
    EXPECT_EQ(
        std::format("{:b}", mid),
        "Unsigned[38 downto 0]{000101011111111111111101001000000000001}"
    );
    EXPECT_EQ(
        std::format("{:b}", very_large),
        "Unsigned[138 downto "
        "0]{"
        "0001010111111111111111010010000000000010000101011111111111111101001000000000001000"
        "010101111111111111110100100000000000111111111111111111111}"
    );

    EXPECT_EQ(std::format("{}", small), "Unsigned[9 downto 0]{102}");
    EXPECT_EQ(std::format("{}", mid), "Unsigned[38 downto 0]{47244546049}");
    EXPECT_EQ(
        std::format("{}", very_large),
        "Unsigned[138 downto 0]{59889577156579543121862034195167783682047}"
    );

    EXPECT_EQ(std::format("{:o}", small), "Unsigned[9 downto 0]{0146}");
    EXPECT_EQ(std::format("{:o}", mid), "Unsigned[38 downto 0]{0537777510001}");
    EXPECT_EQ(
        std::format("{:o}", very_large),
        "Unsigned[138 downto 0]{01277777220002053777751000102577776440007777777}"
    );

    EXPECT_EQ(std::format("{:x}", small), "Unsigned[9 downto 0]{066}");
    EXPECT_EQ(std::format("{:x}", mid), "Unsigned[38 downto 0]{0afffe9001}");
    EXPECT_EQ(
        std::format("{:x}", very_large),
        "Unsigned[138 downto 0]{0afffe90010afffe90010afffe9001fffff}"
    );
}

TEST(TestUnsigned, HashDeterminism) {
    Unsigned<10> a(102);
    Unsigned<10> b(102);

    std::hash<Unsigned<10>> hasher;

    EXPECT_EQ(hasher(a), hasher(a));

    EXPECT_EQ(hasher(a), hasher(b));
}

TEST(TestUnsigned, HashCollisionResistance) {
    Unsigned<10> val1(102);
    Unsigned<10> val2(103);

    std::hash<Unsigned<10>> hasher10;

    EXPECT_NE(hasher10(val1), hasher10(val2));

    Unsigned<20> val3(102);
    std::hash<Unsigned<20>> hasher20;

    EXPECT_NE(hasher10(val1), hasher20(val3));
}

TEST(TestUnsigned, HashUnorderedSetIntegration) {
    std::unordered_set<Unsigned<10>> hash_set;

    Unsigned<10> a(10);
    Unsigned<10> b(20);
    Unsigned<10> a_copy(10);

    hash_set.insert(a);
    hash_set.insert(b);

    hash_set.insert(a_copy);

    EXPECT_EQ(hash_set.size(), 2);

    EXPECT_TRUE(hash_set.find(a) != hash_set.end());
    EXPECT_TRUE(hash_set.find(b) != hash_set.end());

    Unsigned<10> c(30);
    EXPECT_TRUE(hash_set.find(c) == hash_set.end());
}

TEST(TestUnsigned, unary_ops) {
    Unsigned<8> a(150);

    auto neg_a = -a;
    static_assert(
        std::is_same_v<decltype(neg_a), Signed<9>>,
        "Unary minus should promote to Signed<W+1>"
    );
    EXPECT_EQ(static_cast<int>(neg_a), -150);

    Unsigned<4> b(5);
    auto neg_b = -b;
    static_assert(
        std::is_same_v<decltype(neg_b), Signed<5>>,
        "Unary minus should promote to Signed<W+1>"
    );
    EXPECT_EQ(static_cast<int>(neg_b), -5);

    auto pos_a = +a;
    static_assert(
        std::is_same_v<decltype(pos_a), Signed<9>>,
        "Unary plus should promote to Signed<W+1>"
    );
    EXPECT_EQ(static_cast<int>(pos_a), 150);
}

TEST(TestUnsigned, zero_width) {
    Unsigned<0> a{};
    Unsigned<0> b{};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    // operator bool: null is falsy
    EXPECT_FALSE(static_cast<bool>(a));

    // ++/--/+= wrap through resize<0> and land back on null
    Unsigned<0> c{};
    ++c;
    EXPECT_TRUE(c == Unsigned<0>{});
    c++;
    EXPECT_TRUE(c == Unsigned<0>{});
    --c;
    EXPECT_TRUE(c == Unsigned<0>{});
    c += 5;
    EXPECT_TRUE(c == Unsigned<0>{});

    // Binary arithmetic widens: Unsigned<0> + Unsigned<0> -> Unsigned<1>(0)
    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), Unsigned<1>>);
    EXPECT_EQ(static_cast<uint8_t>(sum), 0u);

    // Iteration is empty
    EXPECT_EQ(a.begin(), a.end());
    EXPECT_EQ(a.size(), 0u);

    // Formatting: value renders as "" (Bits<0>::to_*_string) inside the
    // wrapper's braces.
    EXPECT_EQ(std::format("{:b}", a), "Unsigned[-1 downto 0]{}");
    EXPECT_EQ(std::format("{:d}", a), "Unsigned[-1 downto 0]{}");
}

// Resize now routes through the Bits width-changing primitives. These pin the
// behaviour across the native/wide tier boundary, which the pre-existing tests
// only covered on the native side.

TEST(TestUnsigned, resize_across_the_tier_boundary) {
    Unsigned<200> wide(12345);

    // Wide -> native, both narrowing modes.
    EXPECT_EQ(static_cast<int>(resize<32>(wide)), 12345);
    EXPECT_EQ(static_cast<int>(resize<16>(wide, overflow_mode::wrap)), 12345);

    // Native -> wide widens by zero-extension.
    Unsigned<8> narrow(200);
    auto grown = resize<200>(narrow);
    EXPECT_EQ(std::format("{:d}", grown), "Unsigned[199 downto 0]{200}");

    // Round trip preserves the value.
    EXPECT_EQ(static_cast<int>(resize<8>(resize<200>(narrow))), 200);
}

TEST(TestUnsigned, saturation_clamps_from_the_wide_tier) {
    Unsigned<200> big{detail::Bits<200>("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFF")};
    EXPECT_EQ(static_cast<int>(resize<8>(big, overflow_mode::saturate)), 255);
    EXPECT_EQ(static_cast<int>(resize<16>(big, overflow_mode::saturate)), 65535);

    // A value that fits is not clamped.
    Unsigned<200> small(42);
    EXPECT_EQ(static_cast<int>(resize<8>(small, overflow_mode::saturate)), 42);

    // Wrapping truncates instead.
    Unsigned<200> v{detail::Bits<200>("0x123456789ABCDEF0123456789")};
    EXPECT_EQ(static_cast<int>(resize<16>(v, overflow_mode::wrap)), 26505);
}

TEST(TestUnsigned, wide_arithmetic_still_grows) {
    Unsigned<104> a{detail::Bits<104>("0xFEDCBA98765432100123456789")};
    Unsigned<104> b(1000);

    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), Unsigned<105>>);
    EXPECT_EQ(
        std::format("{:d}", sum), "Unsigned[104 downto 0]{20192265560968774111035004382065}"
    );
}

// LCOV_EXCL_BR_STOP

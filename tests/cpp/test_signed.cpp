// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <compare>
#include <concepts>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

using namespace coconext::types;
using namespace coconext::literals;

TEST(TestSigned, Constructors) {
    static_assert(!std::is_convertible_v<int, Signed<6>>);
    static_assert(!std::is_constructible_v<Signed<5>, BitArray<5>>);

    Signed<4> a(-5);
    EXPECT_EQ(static_cast<int8_t>(a), -5);
    EXPECT_EQ(a.size(), 4U);

    EXPECT_THROW(Signed<4>(8), std::out_of_range);
    EXPECT_THROW(Signed<4>(-9), std::out_of_range);
    EXPECT_EQ(static_cast<int16_t>(Signed<8>(uint8_t{127})), 127);
    EXPECT_THROW(Signed<8>(uint8_t{128}), std::out_of_range);
    EXPECT_THROW(Signed<1>(uint8_t{1}), std::out_of_range);

    Signed<8> large_val(100);
    Signed<8> small_val(-5);

    Signed<4> narrow_fit(small_val);
    EXPECT_EQ(static_cast<int8_t>(narrow_fit), -5);

    EXPECT_THROW(Signed<4> narrow_fail(large_val), std::out_of_range);

    BitArray<5> arr_a({'1'_b, '1'_b, '0'_b, '1'_b, '1'_b});
    auto s_arr_a = as<Signed<5>>(arr_a);
    EXPECT_EQ(static_cast<int32_t>(s_arr_a), -5);

    auto arr_a_exp = as<Signed<5>>("11011"_b);
    EXPECT_EQ(s_arr_a, arr_a_exp);

    Unsigned<8> u(100);
    Signed<10> s(u);
    EXPECT_EQ(static_cast<int32_t>(s), 100);

    Unsigned<8> u_large(200);
    EXPECT_THROW(Signed<8> s_narrow(u_large), std::out_of_range);
}

TEST(TestSigned, ImplicitBitArrayConversion) {
    Signed<6> a(-1);
    BitArray<5, 0> b = a;

    EXPECT_EQ(b[0], '1'_b);
    EXPECT_EQ(b[4], '1'_b);
    EXPECT_EQ(b[5], '1'_b);
}

TEST(TestSigned, ExplicitNativeCasts) {
    Signed<16> a(-30000);

    EXPECT_TRUE(static_cast<bool>(a));
    EXPECT_FALSE(static_cast<bool>(Signed<16>(0)));

    EXPECT_EQ(static_cast<int>(a), -30000);
    EXPECT_EQ(static_cast<long long>(a), -30000LL);

    Signed<4> b(-2);
    EXPECT_EQ(static_cast<signed char>(b), -2);

    EXPECT_EQ(static_cast<signed char>(Signed<200>(-128)), -128);
    EXPECT_THROW((void)static_cast<signed char>(Signed<200>(128)), std::out_of_range);
    EXPECT_THROW((void)static_cast<unsigned char>(Signed<200>(-1)), std::out_of_range);
}

TEST(TestSigned, FixedPointConstruction) {
    static_assert(std::is_constructible_v<Signed<8>, Ufixed<8, -4>>);
    static_assert(std::is_constructible_v<Signed<8>, Sfixed<8, -4>>);
    static_assert(!std::is_convertible_v<Ufixed<8, -4>, Signed<8>>);
    static_assert(!std::is_convertible_v<Sfixed<8, -4>, Signed<8>>);

    Signed<8> from_unsigned_fixed(Ufixed<7, -4>(127.9375));
    EXPECT_EQ(static_cast<int>(from_unsigned_fixed), 127);

    Signed<8> from_signed_fixed(Sfixed<8, -4>(-128.9375));
    EXPECT_EQ(static_cast<int>(from_signed_fixed), -128);

    EXPECT_THROW((Signed<8>(Ufixed<8, 0>(128))), std::out_of_range);
    EXPECT_THROW((Signed<8>(Sfixed<8, 0>(-129))), std::out_of_range);

    Sfixed<200, 0> wide_fixed(-1);
    wide_fixed <<= 150;
    EXPECT_EQ(Signed<202>(wide_fixed), Signed<202>(-1) << 150);
}

TEST(TestSigned, ArithmeticOperators) {
    Signed<8> a(-50);
    Signed<8> b(20);

    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), Signed<9>>);
    EXPECT_EQ(static_cast<int>(sum), -30);

    auto prod = a * b;
    static_assert(std::is_same_v<decltype(prod), Signed<16>>);
    EXPECT_EQ(static_cast<int>(prod), -1000);

    auto div = a / b;
    static_assert(std::is_same_v<decltype(div), Signed<9>>);
    EXPECT_EQ(static_cast<int>(div), -2);

    auto mod = a % Signed<4>(7);
    static_assert(std::is_same_v<decltype(mod), Signed<4>>);
    EXPECT_EQ(static_cast<int>(mod), -50 % 7);

    EXPECT_THROW(a / Signed<4>(0), std::domain_error);
    EXPECT_THROW(a % Signed<8>(0), std::domain_error);

    auto sub_pos = a - b;
    auto sub_neg = b - a;
    static_assert(std::is_same_v<decltype(sub_pos), Signed<9>>);
    static_assert(std::is_same_v<decltype(sub_neg), Signed<9>>);
    EXPECT_EQ(static_cast<int16_t>(sub_pos), -70);
    EXPECT_EQ(static_cast<int16_t>(sub_neg), 70);

    Unsigned<8> u(10);
    Signed<5> s2(-5);
    auto mixed_sum = u + s2;
    static_assert(std::is_same_v<decltype(mixed_sum), Signed<10>>);
    EXPECT_EQ(static_cast<int>(mixed_sum), 5);
}

TEST(SignedMixedSignednessTest, CompoundAssignmentOperators) {
    auto s1 = s8(-5);
    s1 += u8(15);
    EXPECT_EQ(s1, s8(10));

    auto s2 = s8(120);
    s2 += u8(20);
    EXPECT_EQ(s2, s8(-116));

    auto s3 = s8(5);
    s3 -= u8(10);
    EXPECT_EQ(s3, s8(-5));

    auto s4 = s8(-3);
    s4 *= u8(10);
    EXPECT_EQ(s4, s8(-30));

    auto s5 = s8(-20);
    s5 /= u8(4);
    EXPECT_EQ(s5, s8(-5));

    auto s6 = s8(-23);
    s6 %= u8(7);
    EXPECT_EQ(s6, s8(-2));

    auto s7 = s8(50);
    EXPECT_THROW(s7 /= u8(0), std::domain_error);
    EXPECT_THROW(s7 %= u8(0), std::domain_error);
}

TEST(TestSigned, as_overloads) {
    BitArray<5> arr_a({'1'_b, '1'_b, '0'_b, '1'_b, '1'_b});

    auto a = as<Signed<5>>(arr_a);
    BitArray<4, 0> arr_exp = a;
    static_assert(std::is_same_v<decltype(a), Signed<5>>);
    EXPECT_EQ(static_cast<int8_t>(a), -5);
    EXPECT_EQ(arr_a, arr_exp);

    Signed<5> a1;
    a1 = as(arr_a);
    BitArray<4, 0> arr_exp_a1 = a1;
    EXPECT_EQ(static_cast<int8_t>(a1), -5);
    EXPECT_EQ(arr_a, arr_exp_a1);
}

TEST(TestSigned, resize_overloads) {
    Signed<8> small(-10);

    auto wide_spelled = resize<16>(small);
    static_assert(std::is_same_v<decltype(wide_spelled), Signed<16>>);
    EXPECT_EQ(static_cast<int16_t>(wide_spelled), -10);

    Signed<16> wide_deduced;
    wide_deduced = resize(small);
    EXPECT_EQ(static_cast<int16_t>(wide_deduced), -10);

    Signed<16> wide(1000);

    auto narrow_wrap_spelled = resize<8>(wide);
    static_assert(std::is_same_v<decltype(narrow_wrap_spelled), Signed<8>>);
    EXPECT_EQ(static_cast<int8_t>(narrow_wrap_spelled), static_cast<int8_t>(1000));

    Signed<8> narrow_wrap_deduced;
    narrow_wrap_deduced = resize(wide);
    EXPECT_EQ(static_cast<int8_t>(narrow_wrap_deduced), static_cast<int8_t>(1000));

    auto narrow_sat_spelled = resize<8>(wide, overflow_mode::saturate);
    EXPECT_EQ(static_cast<int8_t>(narrow_sat_spelled), 127);

    Signed<16> wide_neg(-1000);
    Signed<8> narrow_sat_deduced;
    narrow_sat_deduced = resize(wide_neg, overflow_mode::saturate);
    EXPECT_EQ(static_cast<int8_t>(narrow_sat_deduced), -128);

    Signed<8> copy_init_deduced = resize(wide_neg, overflow_mode::saturate);
    EXPECT_EQ(static_cast<int8_t>(copy_init_deduced), -128);
}

TEST(TestSigned, CrossRangeConversion) {
    Signed<7, 0> source(-42);
    Signed<0, Direction::TO, 7> converted = source;
    EXPECT_EQ(static_cast<signed char>(converted), -42);
}

TEST(TestSigned, Comparisons) {
    static_assert(!std::equality_comparable_with<Signed<8>, Signed<16>>);
    static_assert(!std::three_way_comparable_with<Signed<8>, Signed<16>>);
    static_assert(!std::equality_comparable_with<Signed<8>, Unsigned<8>>);

    Signed<8> a(-10);
    Signed<8> b(-10);
    Signed<8> c(20);
    Signed<8> d(-15);

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
}

TEST(TestSigned, CompoundAssignment) {
    Signed<8> a(-10);
    Signed<4> b(5);

    a += b;
    EXPECT_EQ(static_cast<int>(a), -5);

    a -= Signed<8>(3);
    EXPECT_EQ(static_cast<int>(a), -8);

    a *= Signed<2>(-2);
    EXPECT_EQ(static_cast<int>(a), 16);

    a /= Signed<4>(-4);
    EXPECT_EQ(static_cast<int>(a), -4);

    a %= Signed<4>(3);
    EXPECT_EQ(static_cast<int>(a), -1);

    a += 10;
    EXPECT_EQ(static_cast<int>(a), 9);

    EXPECT_THROW(a /= 0, std::domain_error);
}

TEST(TestSigned, IncrementDecrement) {
    Signed<8> a(-1);

    EXPECT_EQ(static_cast<int>(++a), 0);
    EXPECT_EQ(static_cast<int>(a), 0);

    EXPECT_EQ(static_cast<int>(--a), -1);
    EXPECT_EQ(static_cast<int>(a), -1);

    EXPECT_EQ(static_cast<int>(a++), -1);
    EXPECT_EQ(static_cast<int>(a), 0);

    EXPECT_EQ(static_cast<int>(a--), 0);
    EXPECT_EQ(static_cast<int>(a), -1);

    Signed<4> max_val(7);
    max_val++;
    EXPECT_EQ(static_cast<int>(max_val), -8);

    Signed<4> min_val(-8);
    min_val--;
    EXPECT_EQ(static_cast<int>(min_val), 7);
}

TEST(TestSigned, ShiftOperators) {
    Signed<8> a(-4);

    auto sl = a << 2;
    EXPECT_EQ(static_cast<int>(sl), -16);

    auto sr = a >> 1;
    EXPECT_EQ(static_cast<int>(sr), -2);

    EXPECT_EQ(static_cast<int>(a << 8), 0);
    EXPECT_EQ(static_cast<int>(a >> 10), -1);

    Signed<8> b(4);
    EXPECT_EQ(static_cast<int>(b >> 10), 0);

    a <<= 3;
    EXPECT_EQ(static_cast<int>(a), -32);
    a >>= 2;
    EXPECT_EQ(static_cast<int>(a), -8);

    EXPECT_THROW(a << -1, std::invalid_argument);
    EXPECT_THROW(a >> -2, std::invalid_argument);

    Signed<4> shift_amt(2);
    EXPECT_EQ(static_cast<int>(a << shift_amt), -32);

    constexpr Signed<1000> wide_shift_amt(2);
    static_assert(static_cast<int>(Signed<8>(5) << wide_shift_amt) == 20);

    Signed<1000> huge_shift_amt;
    huge_shift_amt[998] = Bit::_1;
    EXPECT_EQ(static_cast<int>(a << huge_shift_amt), 0);
    EXPECT_EQ(static_cast<int>(a >> huge_shift_amt), -1);

    Signed<1000> negative_shift_amt(-1);
    EXPECT_THROW(a << negative_shift_amt, std::invalid_argument);
    EXPECT_THROW(a >> negative_shift_amt, std::invalid_argument);
}

TEST(TestSigned, Iterators) {
    Signed<4> a(-2);  // 1110

    std::vector<int> bit_vals;
    for (auto bit : a) {
        bit_vals.push_back(static_cast<bool>(bit) ? 1 : 0);
    }

    EXPECT_EQ(bit_vals.size(), 4);
    EXPECT_EQ(bit_vals[0], 1);
    EXPECT_EQ(bit_vals[1], 1);
    EXPECT_EQ(bit_vals[2], 1);
    EXPECT_EQ(bit_vals[3], 0);

    std::vector<int> rbit_vals;
    for (auto rit = a.rbegin(); rit != a.rend(); ++rit) {
        rbit_vals.push_back(static_cast<bool>(*rit) ? 1 : 0);
    }

    std::vector<int> expected_rvals = {0, 1, 1, 1};
    EXPECT_EQ(rbit_vals, expected_rvals);

    std::reverse(rbit_vals.begin(), rbit_vals.end());
    EXPECT_EQ(rbit_vals, bit_vals);

    auto it = a.begin();
    EXPECT_EQ(static_cast<bool>(*(it + 1)), true);
    EXPECT_EQ(static_cast<bool>(it[2]), true);

    for (auto bit : a) {
        bit = Bit::_0;
    }
    EXPECT_EQ(static_cast<int8_t>(a), 0);

    *a.rbegin() = Bit::_1;
    EXPECT_EQ(static_cast<int8_t>(a), 1);

    auto const& const_a = a;
    EXPECT_EQ(std::distance(const_a.begin(), const_a.end()), 4);
}

TEST(TestSigned, index_operator) {
    Signed<4> a(-2);  // 1110

    EXPECT_TRUE(static_cast<bool>(a[3]));
    EXPECT_TRUE(static_cast<bool>(a[2]));
    EXPECT_TRUE(static_cast<bool>(a[1]));
    EXPECT_FALSE(static_cast<bool>(a[0]));

    EXPECT_THROW(a[4], std::out_of_range);

    a[3] = Bit::_0;
    a[0] = Bit::_1;
    EXPECT_EQ(static_cast<int8_t>(a), 7);

    auto const& const_a = a;
    EXPECT_FALSE(static_cast<bool>(const_a[3]));
}

TEST(TestSigned, index) {
    Signed<4> a(-6);  // 1010

    EXPECT_TRUE(static_cast<bool>(a.index<3>()));
    EXPECT_FALSE(static_cast<bool>(a.index<2>()));
    EXPECT_TRUE(static_cast<bool>(a.index<1>()));
    EXPECT_FALSE(static_cast<bool>(a.index<0>()));

    EXPECT_EQ(static_cast<int>(a.index<3>()), 1);
    EXPECT_EQ(static_cast<int>(a.index<0>()), 0);
}

TEST(TestSigned, Bitwise_ops) {
    Signed<8> a(-10);
    Signed<8> b(-10);

    auto and_result = a & b;
    EXPECT_EQ(and_result, BitArray<8>("11110110"_b));
}

TEST(TestSigned, const_udl) {
    static_assert(std::is_same_v<decltype(s8(0)), Signed<8>>);
    static_assert(std::is_same_v<decltype(s16(0)), Signed<16>>);
    static_assert(std::is_same_v<decltype(s32(0)), Signed<32>>);
    static_assert(std::is_same_v<decltype(s64(0)), Signed<64>>);

    static_assert(static_cast<int>(s8(-128)) == -128);
    static_assert(static_cast<int>(s8(127)) == 127);
    static_assert(static_cast<int>(s8(-5)) == -5);

    static_assert(static_cast<int>(s16(-32768)) == -32768);
    static_assert(static_cast<int>(s16(32767)) == 32767);

    static_assert(static_cast<long long>(s32(-2147483648LL)) == -2147483648LL);
    static_assert(static_cast<long long>(s32(2147483647LL)) == 2147483647LL);

    static_assert(
        static_cast<long long>(s64(-9223372036854775807LL - 1))
        == -9223372036854775807LL - 1
    );
    static_assert(
        static_cast<long long>(s64(9223372036854775807LL)) == 9223372036854775807LL
    );

    static_assert(s8(-42) == s8(-42));
    static_assert(s16(1024) == s16(1024));
}

TEST(TestSigned, Formatter) {
    Signed<10> small(-102);

    EXPECT_EQ(std::format("{:b}", small), "Signed[9 downto 0]{1110011010}");

    EXPECT_EQ(std::format("{}", small), "Signed[9 downto 0]{-102}");

    EXPECT_EQ(std::format("{:o}", small), "Signed[9 downto 0]{1632}");

    EXPECT_EQ(std::format("{:x}", small), "Signed[9 downto 0]{39a}");
}

TEST(TestSigned, HashDeterminism) {
    Signed<10> a(-102);
    Signed<10> b(-102);

    std::hash<Signed<10>> hasher;

    EXPECT_EQ(hasher(a), hasher(a));
    EXPECT_EQ(hasher(a), hasher(b));
}

TEST(TestSigned, HashCollisionResistance) {
    Signed<10> val1(-102);
    Signed<10> val2(-103);

    std::hash<Signed<10>> hasher10;

    EXPECT_NE(hasher10(val1), hasher10(val2));

    Signed<20> val3(-102);
    std::hash<Signed<20>> hasher20;

    EXPECT_NE(hasher10(val1), hasher20(val3));
}

TEST(TestSigned, HashUnorderedSetIntegration) {
    std::unordered_set<Signed<10>> hash_set;

    Signed<10> a(-10);
    Signed<10> b(20);
    Signed<10> a_copy(-10);

    hash_set.insert(a);
    hash_set.insert(b);
    hash_set.insert(a_copy);

    EXPECT_EQ(hash_set.size(), 2);

    EXPECT_TRUE(hash_set.find(a) != hash_set.end());
    EXPECT_TRUE(hash_set.find(b) != hash_set.end());

    Signed<10> c(-30);
    EXPECT_TRUE(hash_set.find(c) == hash_set.end());
}

TEST(TestSigned, unary_ops) {
    Signed<8> a(-100);

    auto neg_a = -a;
    static_assert(std::is_same_v<decltype(neg_a), Signed<9>>);
    EXPECT_EQ(static_cast<int>(neg_a), 100);

    Signed<4> b(5);
    auto neg_b = -b;
    static_assert(std::is_same_v<decltype(neg_b), Signed<5>>);
    EXPECT_EQ(static_cast<int>(neg_b), -5);

    auto pos_a = +a;
    static_assert(std::is_same_v<decltype(pos_a), Signed<8>>);
    EXPECT_EQ(static_cast<int>(pos_a), -100);

    auto abs_a = abs(a);
    static_assert(std::is_same_v<decltype(abs_a), Signed<9>>);
    EXPECT_EQ(static_cast<int>(abs_a), 100);
}

TEST(TestSigned, zero_width) {
    Signed<0> a{};
    Signed<0> b{};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_FALSE(static_cast<bool>(a));

    Signed<0> c{};
    ++c;
    EXPECT_TRUE(c == Signed<0>{});
    c--;
    EXPECT_TRUE(c == Signed<0>{});
    c += 5;
    EXPECT_TRUE(c == Signed<0>{});

    EXPECT_EQ(a.begin(), a.end());
    EXPECT_EQ(a.size(), 0u);
}

// Resize now routes through the signed representation. These pin the
// behaviour across the native/wide tier boundary, and in particular that
// sign-extension still replicates the sign bit into wide storage.

TEST(TestSigned, sign_extension_across_the_tier_boundary) {
    Signed<8> neg(-1);
    auto wide = resize<200>(neg);
    EXPECT_EQ(std::format("{:d}", wide), "Signed[199 downto 0]{-1}");

    Signed<8> pos(127);
    EXPECT_EQ(std::format("{:d}", resize<200>(pos)), "Signed[199 downto 0]{127}");

    // Round trip preserves the value and the sign.
    EXPECT_EQ(static_cast<int>(resize<8>(resize<200>(neg))), -1);
    EXPECT_EQ(static_cast<int>(resize<8>(resize<200>(Signed<8>(-100)))), -100);
}

TEST(TestSigned, saturation_clamps_at_both_ends_from_wide) {
    Signed<200> big(5000);
    EXPECT_EQ(static_cast<int>(resize<8>(big, overflow_mode::saturate)), 127);

    Signed<200> very_neg(-5000);
    EXPECT_EQ(static_cast<int>(resize<8>(very_neg, overflow_mode::saturate)), -128);

    // Values that fit are untouched, including negative ones.
    EXPECT_EQ(
        static_cast<int>(resize<8>(Signed<200>(-100), overflow_mode::saturate)), -100
    );
    EXPECT_EQ(static_cast<int>(resize<8>(Signed<200>(100), overflow_mode::saturate)), 100);
}

TEST(TestSigned, wide_arithmetic_still_grows) {
    Signed<110> a{detail::SInt<110>("-20192265560968774111035004381065")};
    Signed<110> b(1000);

    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), Signed<111>>);
    EXPECT_EQ(
        std::format("{:d}", sum), "Signed[110 downto 0]{-20192265560968774111035004380065}"
    );
}

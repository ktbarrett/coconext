// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types.hpp>

using namespace coconext::types;
using namespace coconext::literals;

TEST(TestSfixed, shape_and_typelevel) {
    Sfixed<6, -2> a(-37.25);
    Sfixed<-2, Direction::TO, 6> b;
    BitArray<9> ba(-37);

    static_assert(a.size() == 9);
    static_assert(b.size() == 9);

    static_assert(a.static_range == Range(6, Direction::DOWNTO, -2));
    static_assert(b.range() == Range(-2, Direction::TO, 6));

    static_assert(is_fixed<decltype(a)>);
    static_assert(is_fixed<decltype(b)>);

    static_assert(a.resolution() == 0.25);
    static_assert(b.resolution() == 0.25);

    static_assert(a.int_bits() == 7);
    static_assert(b.int_bits() == 7);

    static_assert(a.frac_bits() == 2);
    static_assert(b.frac_bits() == 2);

    static_assert(is_fixed<decltype(a)>);
}

TEST(TestSfixed, BooleanConversion) {
    Sfixed<3, -4> zero_val(0.0);
    Sfixed<3, -4> nonzero_val(-0.0625);

    Sfixed<3, -4> zero_int(0);
    Sfixed<3, -4> nonzero_int(-5);

    EXPECT_FALSE(static_cast<bool>(zero_val));
    EXPECT_TRUE(static_cast<bool>(nonzero_val));

    EXPECT_FALSE(static_cast<bool>(zero_int));
    EXPECT_TRUE(static_cast<bool>(nonzero_int));

    static_assert(
        noexcept(static_cast<bool>(zero_val)), "Boolean egress must be noexcept."
    );
}

TEST(TestSfixed, NumericEgress) {
    Sfixed<4, -4> val(-5.9375);
    EXPECT_EQ(static_cast<int>(val), -5);
    EXPECT_EQ(static_cast<short>(val), -5);
    EXPECT_EQ(static_cast<long long>(val), -5LL);

    Sfixed<7, 0> val_int(-100);
    EXPECT_EQ(static_cast<signed char>(val_int), -100);
    EXPECT_EQ(static_cast<int>(val_int), -100);

    Sfixed<11, 0> large_val(-300);
    EXPECT_NO_THROW(static_cast<void>(static_cast<short>(large_val)));
    EXPECT_THROW(static_cast<unsigned char>(large_val), std::out_of_range);
    EXPECT_THROW(static_cast<signed char>(large_val), std::out_of_range);
}

TEST(TestSfixed, FloatEgress) {
    Sfixed<3, -4> val(-5.0625);
    EXPECT_DOUBLE_EQ(static_cast<double>(val), -5.0625);
    EXPECT_FLOAT_EQ(static_cast<float>(val), -5.0625f);

    Sfixed<7, -8> val_m(-12.5);
    static_assert(
        noexcept(static_cast<double>(val_m)), "double conversion must be noexcept"
    );
    static_assert(noexcept(static_cast<float>(val_m)), "float conversion must be noexcept");

    Sfixed<3, -4> val_s(-2.5);
    EXPECT_EQ(int(val_s), -2);
    EXPECT_EQ((int)val_s, -2);
    EXPECT_DOUBLE_EQ(double(val_s), -2.5);
}

TEST(TestSfixed, Constructors) {
    Sfixed<3, -4> val;
    EXPECT_DOUBLE_EQ(static_cast<double>(val), 0.0);

    int8_t native_val = -100;
    Sfixed<7, 0> val_n = native_val;
    EXPECT_EQ(static_cast<int>(val_n), -100);

    Signed<8> s_val(-100);
    Sfixed<7, 0> sf_val = s_val;
    EXPECT_EQ(static_cast<int>(sf_val), -100);

    Unsigned<7> u_val(100);
    Sfixed<7, 0> suf_val = u_val;  // Unsigned to Sfixed implicitly gains a sign bit
    EXPECT_EQ(static_cast<int>(suf_val), 100);

    Sfixed<3, 0> original(-5);
    Sfixed<7, -4> widened = original;
    EXPECT_DOUBLE_EQ(static_cast<double>(widened), -5.0);

    EXPECT_THROW((Sfixed<3, 0>(-10)), std::out_of_range);  // Sfixed<3,0> min is -8

    Sfixed<3, -4> val_float(-5.0625);
    EXPECT_DOUBLE_EQ(static_cast<double>(val_float), -5.0625);

    Sfixed<3, -4> inf_val(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(static_cast<double>(inf_val), 7.9375);  // Saturates to max

    Sfixed<3, -4> neg_inf_val(-std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(static_cast<double>(neg_inf_val), -8.0);  // Saturates to min

    EXPECT_THROW(
        (Sfixed<3, -4>(std::numeric_limits<double>::quiet_NaN())), std::domain_error
    );

    Sfixed<7, 0> wide(-100);
    Sfixed<3, 0> narrow_saturated(wide);
    Sfixed<3, 0> narrow_wrapped(wide, overflow_mode::wrap, round_mode::round_to_even);

    EXPECT_EQ(static_cast<int>(narrow_saturated), -8);
    EXPECT_EQ(static_cast<int>(narrow_wrapped), -4);
}

TEST(TestSfixed, as_overloads) {
    Sfixed<3, 0> signed_val(-1);

    auto unsigned_val = as<Ufixed<3, 0>>(signed_val);
    EXPECT_EQ(static_cast<int>(unsigned_val), 15);

    Sfixed<3, 0> s_val(-4);
    auto frac_val = as<Sfixed<-1, -4>>(s_val);
    EXPECT_DOUBLE_EQ(static_cast<double>(frac_val), -0.25);

    Sfixed<3, 0> original_downto(-1);
    auto to_val = as<Sfixed<Range{0, Direction::TO, 3}>>(original_downto);
    auto new_downto = as<Sfixed<1, -2>>(to_val);
    EXPECT_DOUBLE_EQ(static_cast<double>(new_downto), -0.25);
}

TEST(TestSfixed, resize) {
    Sfixed<7, 0> large_neg_val(-100);
    auto sat_res = resize<3, 0>(large_neg_val);
    EXPECT_EQ(static_cast<int>(sat_res), -8);

    Sfixed<3, -4> even_tie1(-2.5);
    auto rnd_res1 = resize<3, 0>(even_tie1);
    EXPECT_DOUBLE_EQ(static_cast<double>(rnd_res1), -2.0);  // Ties to even

    Sfixed<3, -4> even_tie2(-3.5);
    auto rnd_res2 = resize<3, 0>(even_tie2);
    EXPECT_DOUBLE_EQ(static_cast<double>(rnd_res2), -4.0);  // Ties to even

    Sfixed<3, 0> s_orig(-5);
    auto s_wide = resize<7, -4>(s_orig);
    EXPECT_DOUBLE_EQ(static_cast<double>(s_wide), -5.0);

    Sfixed<7, 0> src(-100);
    Sfixed<3, 0> dst = resize(src, overflow_mode::wrap);
    EXPECT_EQ(static_cast<int>(dst), -4);
}

TEST(TestSfixed, ShiftOperators) {
    Sfixed<3, -4> val(-1.0);  // raw binary 11110000
    auto shifted_left = val << 1;
    EXPECT_TRUE((std::is_same_v<decltype(shifted_left), Sfixed<3, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(shifted_left), -2.0);

    // Sfixed >> is arithmetic (sign-extending)
    auto shifted_right = val >> 1;
    EXPECT_TRUE((std::is_same_v<decltype(shifted_right), Sfixed<3, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(shifted_right), -0.5);

    Sfixed<3, -4> compound_val(-1.5);
    compound_val <<= 1;
    EXPECT_DOUBLE_EQ(static_cast<double>(compound_val), -3.0);

    compound_val >>= 2;
    EXPECT_DOUBLE_EQ(static_cast<double>(compound_val), -0.75);

    int neg_shift = -1;
    EXPECT_THROW(val << neg_shift, std::invalid_argument);
    EXPECT_THROW(val >> neg_shift, std::invalid_argument);
}

TEST(TestSfixed, ComparisonOperators) {
    Sfixed<5, -2> a(-3.25);
    Sfixed<5, -2> b(-3.25);
    Sfixed<5, -2> c(-4.5);
    Sfixed<5, -2> d(1.0);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);

    EXPECT_TRUE(c < a);
    EXPECT_FALSE(d < a);

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(c <= a);

    EXPECT_TRUE(a > c);
    EXPECT_TRUE(d > a);
}

TEST(TestSfixed, AtOrdinalIndexing) {
    auto ba = "100110"_b;
    Sfixed<4, -1> uf(ba);

    EXPECT_TRUE(uf.at_ordinal(0) && uf.at_ordinal(3) && uf.at_ordinal(4));
    EXPECT_FALSE(uf.at_ordinal(1) || uf.at_ordinal(2) || uf.at_ordinal(5));

    EXPECT_THROW(uf.at_ordinal(6), std::out_of_range);
    EXPECT_THROW(uf.at_ordinal(7), std::out_of_range);
}

TEST(TestSfixed, Indexing) {
    auto ba = "100110"_b;
    Sfixed<4, -1> uf(ba);

    EXPECT_TRUE(uf[4] && uf[1] && uf[0]);
    EXPECT_FALSE(uf[-1] || uf[3] || uf[2]);

    EXPECT_THROW(uf[5], std::out_of_range);
    EXPECT_THROW(uf[-2], std::out_of_range);
}

TEST(TestSfixed, IterationSurface) {
    Sfixed<3, -4> val(6.5);

    size_t forward_count = 0;
    for (auto it = val.begin(); it != val.end(); ++it) {
        forward_count++;
    }
    EXPECT_EQ(forward_count, 8);

    size_t reverse_count = 0;
    for (auto it = val.rbegin(); it != val.rend(); ++it) {
        reverse_count++;
    }
    EXPECT_EQ(reverse_count, 8);

    EXPECT_EQ(std::distance(val.begin(), val.end()), 8);
    EXPECT_EQ(std::distance(val.rbegin(), val.rend()), 8);

    auto true_bits = std::count_if(val.begin(), val.end(), [](auto bit) {
        return static_cast<bool>(bit);
    });
    EXPECT_EQ(true_bits, 3);

    size_t range_based_count = 0;
    for (auto bit : val) {
        (void)bit;
        range_based_count++;
    }
    EXPECT_EQ(range_based_count, 8);
}

TEST(TestSfixed, RoundFreeFunctions) {
    EXPECT_EQ((floor(Sfixed<10, -10>(-9.6))), (Sfixed<10, 0>(-10)));
    EXPECT_EQ((floor(Sfixed<10, -10>(-9.025))), (Sfixed<10, 0>(-10)));
    EXPECT_EQ((floor(Sfixed<10, -10>(9.6))), (Sfixed<10, 0>(9)));
    EXPECT_EQ((floor(Sfixed<10, -10>(9.025))), (Sfixed<10, 0>(9)));

    EXPECT_EQ((ceil(Sfixed<10, -10>(-9.6))), (Sfixed<10, 0>(-9)));
    EXPECT_EQ((ceil(Sfixed<10, -10>(-9.025))), (Sfixed<10, 0>(-9)));
    EXPECT_EQ((ceil(Sfixed<10, -10>(9.6))), (Sfixed<10, 0>(10)));
    EXPECT_EQ((ceil(Sfixed<10, -10>(9.025))), (Sfixed<10, 0>(10)));

    EXPECT_EQ((trunc(Sfixed<10, -10>(-9.6))), (Sfixed<10, 0>(-9)));
    EXPECT_EQ((trunc(Sfixed<10, -10>(-9.025))), (Sfixed<10, 0>(-9)));
    EXPECT_EQ((trunc(Sfixed<10, -10>(9.6))), (Sfixed<10, 0>(9)));
    EXPECT_EQ((trunc(Sfixed<10, -10>(9.025))), (Sfixed<10, 0>(9)));

    EXPECT_EQ((round(Sfixed<10, -10>(-9.6))), (Sfixed<10, 0>(-10)));
    EXPECT_EQ((round(Sfixed<10, -10>(-9.5))), (Sfixed<10, 0>(-10)));
    EXPECT_EQ((round(Sfixed<10, -10>(-9.052))), (Sfixed<10, 0>(-9)));
    EXPECT_EQ((round(Sfixed<10, -10>(9.6))), (Sfixed<10, 0>(10)));
    EXPECT_EQ((round(Sfixed<10, -10>(9.5))), (Sfixed<10, 0>(10)));
    EXPECT_EQ((round(Sfixed<10, -10>(9.052))), (Sfixed<10, 0>(9)));
}

TEST(TestSfixed, Reverse) {
    auto ba = "11110110"_b;
    auto ba_r = "01101111"_b;

    Sfixed<3, -4> sf_down(ba);
    Sfixed<-4, Direction::TO, 3> sf_to(ba);

    auto r_to = reverse(sf_down);
    auto r_down = reverse(sf_to);

    EXPECT_EQ(r_to, sf_to);
    EXPECT_EQ(r_down, (Sfixed<3, -4>(ba_r)));
}

TEST(TestSfixed, Formatter) {
    Sfixed<3, -4> val(-5.0625);
    EXPECT_EQ(std::format("{}", val), std::format("{:d}", val));
    EXPECT_EQ(std::format("{:d}", val), "Sfixed[3 downto -4]{-5.0625}");

    EXPECT_EQ(std::format("{:b}", val), "Sfixed[3 downto -4]{1010.1111}");

    Sfixed<8, -3> val_downto(-120);
    auto val_to = reverse(val_downto);
    EXPECT_THROW(std::vformat("{:d}", std::make_format_args(val_to)), std::format_error);
}

TEST(TestSfixed, Hash) {
    Sfixed<3, 0> a(5);
    Sfixed<3, 0> b(5);
    Sfixed<3, 0> c(2);

    auto hash_a = std::hash<Sfixed<3, 0>>{}(a);
    auto hash_b = std::hash<Sfixed<3, 0>>{}(b);
    auto hash_c = std::hash<Sfixed<3, 0>>{}(c);

    EXPECT_EQ(hash_a, hash_b);
    EXPECT_NE(hash_a, hash_c);

    detail::Array<Bit, Range{3, Direction::DOWNTO, 0}> raw_bits;
    raw_bits[3] = Bit::_1;
    raw_bits[2] = Bit::_1;
    raw_bits[1] = Bit::_0;
    raw_bits[0] = Bit::_1;

    Ufixed<3, 0> u_downto(raw_bits);
    Ufixed<2, -1> u_shifted(raw_bits);
    Ufixed<Range{0, Direction::TO, 3}> u_to(raw_bits);
    Sfixed<3, 0> s_downto(raw_bits);

    auto hash_u_downto = std::hash<decltype(u_downto)>{}(u_downto);
    auto hash_u_shifted = std::hash<decltype(u_shifted)>{}(u_shifted);
    auto hash_u_to = std::hash<decltype(u_to)>{}(u_to);
    auto hash_s_downto = std::hash<decltype(s_downto)>{}(s_downto);
    auto hash_raw_bits = std::hash<decltype(raw_bits)>{}(raw_bits);

    EXPECT_NE(hash_u_downto, hash_u_shifted);
    EXPECT_NE(hash_u_downto, hash_u_to);
    EXPECT_NE(hash_u_downto, hash_s_downto);
    EXPECT_NE(hash_u_downto, hash_raw_bits);

    Sfixed<100, 0> wide_a(1);
    wide_a <<= 80;

    Sfixed<100, 0> wide_b(1);
    wide_b <<= 80;

    Sfixed<100, 0> wide_c(1);
    wide_c <<= 79;  // Different bit pattern

    auto hash_wide_a = std::hash<Sfixed<100, 0>>{}(wide_a);
    auto hash_wide_b = std::hash<Sfixed<100, 0>>{}(wide_b);
    auto hash_wide_c = std::hash<Sfixed<100, 0>>{}(wide_c);

    EXPECT_EQ(hash_wide_a, hash_wide_b);
    EXPECT_NE(hash_wide_a, hash_wide_c);
}

TEST(TestSfixed, BitwiseAndReduction) {
    Sfixed<3, 0> a(-3);  // -3 in 4-bit two's complement is 1101
    Sfixed<3, 0> b(2);   //  2 in 4-bit two's complement is 0010

    auto and_res = a & b;  // 1101 & 0010 = 0000 (0)
    EXPECT_TRUE((
        std::is_same_v<decltype(and_res), BitArray<Range{3, Direction::DOWNTO, 0}>>
    ));
    EXPECT_EQ((as<Sfixed<3, 0>>(and_res)), (Sfixed<3, 0>(0)));

    auto or_res = a | b;  // 1101 | 0010 = 1111 (-1)
    EXPECT_EQ((as<Sfixed<3, 0>>(or_res)), (Sfixed<3, 0>(-1)));

    auto xor_res = a ^ b;  // 1101 ^ 0010 = 1111 (-1)
    EXPECT_EQ((as<Sfixed<3, 0>>(xor_res)), (Sfixed<3, 0>(-1)));

    auto not_res = ~a;  // ~1101 = 0010 (2)
    EXPECT_EQ((as<Sfixed<3, 0>>(not_res)), (Sfixed<3, 0>(2)));

    // Reduction on a (-3 -> 1101)
    EXPECT_FALSE(and_reduce(a));  // Contains a zero
    EXPECT_TRUE(or_reduce(a));    // Contains a one
    EXPECT_TRUE(xor_reduce(a));   // Contains an odd number of ones (three 1s)
}

TEST(TestSfixed, Concatenation) {
    Sfixed<3, 0> a(5);
    Sfixed<1, -2> b(-0.25);

    auto cat_res = concat(a, b);
    EXPECT_TRUE((std::is_same_v<decltype(cat_res), BitArray<8>>));

    EXPECT_EQ(static_cast<int>(as<Sfixed<7, 0>>(cat_res)), 95);
}

TEST(TestSfixed, SubtypeRoundTrip) {
    Sfixed<3, -4> s(5.0625);

    BitArray<Range{3, Direction::DOWNTO, -4}> ba = s;
    Sfixed<3, -4> restored(ba);

    EXPECT_EQ(s, restored);
    EXPECT_TRUE(s == (Sfixed<3, -4>(BitArray<Range{3, Direction::DOWNTO, -4}>(s))));
}

TEST(TestSfixed, InfinityWrapThrows) {
    EXPECT_THROW(
        (Sfixed<3, -4>(std::numeric_limits<double>::infinity(), overflow_mode::wrap)),
        std::domain_error
    );
    EXPECT_THROW(
        (Sfixed<3, -4>(-std::numeric_limits<double>::infinity(), overflow_mode::wrap)),
        std::domain_error
    );
}

TEST(TestSfixed, MinMax) {
    Sfixed<5, -2> a(3.25);
    Sfixed<5, -2> b(4.5);
    Sfixed<5, -2> c(-4.5);
    Sfixed<5, -2> d(-3.25);

    EXPECT_EQ(std::min(a, b), a);
    EXPECT_EQ(std::max(a, b), b);

    EXPECT_EQ(std::min(c, d), c);
    EXPECT_EQ(std::max(c, d), d);

    EXPECT_EQ(std::min(a, c), c);
    EXPECT_EQ(std::max(b, d), b);
}

TEST(TestSfixed, StrictTODirectionRejection) {
    using ToType = Sfixed<-2, Direction::TO, 6>;
    using DowntoType = Sfixed<6, -2>;

    static_assert(requires(DowntoType a, DowntoType b) { a == b; });
    static_assert(requires(DowntoType a) { static_cast<double>(a); });
    static_assert(requires(ToType a) { std::format("{:b}", a); });
    static_assert(requires(ToType a) {
        static_cast<BitArray<Range{-2, Direction::TO, 6}>>(a);
    });
}

TEST(TestSfixed, ImplicitCrossKindWidening) {
    Ufixed<3, -4> u(5.0625);

    Sfixed<4, -4> s = u;
    EXPECT_DOUBLE_EQ(static_cast<double>(s), 5.0625);

    static_assert(!std::is_convertible_v<Sfixed<3, -4>, Ufixed<3, -4>>);
}

TEST(TestSfixed, AbsFunction) {
    Sfixed<10, -10> neg_val(-8.25);
    Sfixed<10, -10> pos_val(7.25);

    auto abs_val_n = abs(neg_val);
    auto abs_val_p = abs(pos_val);

    EXPECT_TRUE((std::is_same_v<decltype(abs_val_n), Sfixed<11, -10>>));
    EXPECT_TRUE((std::is_same_v<decltype(abs_val_p), Sfixed<11, -10>>));

    EXPECT_EQ(static_cast<long double>(abs_val_p), 7.25);
    EXPECT_EQ(static_cast<long double>(abs_val_n), 8.25);
}

TEST(TestSfixed, UnaryOperators) {
    Sfixed<3, -4> val_neg(-5.25);
    auto neg_res = -val_neg;
    EXPECT_TRUE((std::is_same_v<decltype(neg_res), Sfixed<4, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(neg_res), 5.25);

    Sfixed<3, 0> min_val(-8);
    auto neg_min = -min_val;
    EXPECT_TRUE((std::is_same_v<decltype(neg_min), Sfixed<4, 0>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(neg_min), 8.0);

    Sfixed<3, -4> pos_val(7.25);
    auto neg_pos_v = -pos_val;
    EXPECT_TRUE((std::is_same_v<decltype(neg_pos_v), Sfixed<4, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(neg_pos_v), -7.25);

    Sfixed<3, -4> val_pos(3.75);
    auto pos_res = +val_pos;
    EXPECT_TRUE((std::is_same_v<decltype(pos_res), Sfixed<3, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(pos_res), 3.75);
}

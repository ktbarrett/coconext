// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>

using namespace coconext::types;
using namespace coconext::literals;

TEST(TestUfixed, shape_and_typelevel) {
    Ufixed<6, -2> a(37);
    Ufixed<-2, Direction::TO, 6> b;
    BitArray<9> ba(37);

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

    static_assert(static_cast<BitArray<9>>(Ufixed<6, -2>(37)) == BitArray<9>(37 << 2));

    static_assert(is_fixed<decltype(a)>);
}

TEST(TestUfixed, BooleanConversion) {
    Ufixed<3, -4> zero_val(0.0);
    Ufixed<3, -4> nonzero_val(0.0625);

    Ufixed<3, -4> zero_int(0);
    Ufixed<3, -4> nonzero_int(5);

    EXPECT_FALSE(static_cast<bool>(zero_val));
    EXPECT_TRUE(static_cast<bool>(nonzero_val));

    EXPECT_FALSE(static_cast<bool>(zero_int));
    EXPECT_TRUE(static_cast<bool>(nonzero_int));

    static_assert(
        noexcept(static_cast<bool>(zero_val)), "Boolean egress must be noexcept."
    );
}

TEST(TestUfixed, NumericEgress) {
    Ufixed<3, -4> val(5.9375);
    EXPECT_EQ(static_cast<int>(val), 5);
    EXPECT_EQ(static_cast<unsigned int>(val), 5u);
    EXPECT_EQ(static_cast<short>(val), 5);
    EXPECT_EQ(static_cast<long long>(val), 5LL);

    Ufixed<7, 0> val_int(200);
    EXPECT_EQ(static_cast<unsigned char>(val_int), 200);
    EXPECT_EQ(static_cast<int>(val_int), 200);

    Ufixed<11, 0> large_val(300);
    EXPECT_NO_THROW(static_cast<short>(large_val));
    EXPECT_THROW(static_cast<unsigned char>(large_val), std::out_of_range);
    EXPECT_THROW(static_cast<signed char>(large_val), std::out_of_range);
}

TEST(TestUfixed, FloatEgress) {
    Ufixed<3, -4> val(5.0625);
    EXPECT_DOUBLE_EQ(static_cast<double>(val), 5.0625);
    EXPECT_FLOAT_EQ(static_cast<float>(val), 5.0625f);

    Ufixed<7, -8> val_m(12.5);
    static_assert(
        noexcept(static_cast<double>(val_m)), "double conversion must be noexcept"
    );
    static_assert(noexcept(static_cast<float>(val_m)), "float conversion must be noexcept");
    static_assert(
        noexcept(static_cast<long double>(val_m)), "long double conversion must be noexcept"
    );

    Ufixed<39, -10> large_exact_val((1ULL << 39) + 0.5);
    EXPECT_DOUBLE_EQ(
        static_cast<double>(large_exact_val), static_cast<double>((1ULL << 39) + 0.5)
    );

    Ufixed<3, -4> val_s(2.5);
    EXPECT_EQ(int(val_s), 2);
    EXPECT_EQ((int)val_s, 2);
    EXPECT_EQ(static_cast<int>(val_s), 2);
    EXPECT_DOUBLE_EQ(double(val_s), 2.5);
}

TEST(TestUfixed, Constructors) {
    Ufixed<3, -4> val;
    EXPECT_DOUBLE_EQ(static_cast<double>(val), 0.0);

    uint8_t native_val = 200;
    Ufixed<7, 0> val_n = native_val;
    EXPECT_EQ(static_cast<int>(val_n), 200);

    Unsigned<8> u_val(150);
    Ufixed<7, 0> uf_val = u_val;
    EXPECT_EQ(static_cast<int>(uf_val), 150);

    Ufixed<3, 0> original(10);
    Ufixed<7, -4> widened = original;
    EXPECT_DOUBLE_EQ(static_cast<double>(widened), 10.0);

    Ufixed<3, 0> val_int(15);
    EXPECT_EQ(static_cast<int>(val_int), 15);

    EXPECT_THROW((Ufixed<3, 0>(16)), std::out_of_range);

    Ufixed<3, -4> val_float(5.0625);
    EXPECT_DOUBLE_EQ(static_cast<double>(val_float), 5.0625);

    Ufixed<3, -4> val_f(0.09375);
    EXPECT_DOUBLE_EQ(static_cast<double>(val_f), 0.125);

    Ufixed<3, -4> val_fi(100.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(val_fi), 15.9375);

    Ufixed<3, -4> inf_val(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(static_cast<double>(inf_val), 15.9375);

    Ufixed<3, -4> neg_inf_val(-std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(static_cast<double>(neg_inf_val), 0.0);

    EXPECT_THROW(
        (Ufixed<3, -4>(std::numeric_limits<double>::quiet_NaN())), std::domain_error
    );

    BitArray<7, 0> bits;
    bits[7] = Bit::_1;
    Ufixed<3, -4> val_ba(bits);
    EXPECT_DOUBLE_EQ(static_cast<double>(val_ba), 8.0);

    Sfixed<3, -4> negative_val(-5.0);
    Sfixed<3, -4> positive_val(5.0);
    EXPECT_THROW((Ufixed<3, -4>(negative_val)), std::out_of_range);

    Ufixed<3, -4> pos_uf(positive_val);
    EXPECT_DOUBLE_EQ(static_cast<double>(pos_uf), 5.0);

    Ufixed<7, 0> wide(200);
    Ufixed<3, 0> narrow_saturated(wide);
    Ufixed<3, 0> narrow_wrapped(wide, overflow_mode::wrap, round_mode::round_to_even);

    EXPECT_EQ(static_cast<int>(narrow_saturated), 15);
    EXPECT_EQ(static_cast<int>(narrow_wrapped), 8);
}

TEST(TestUfixed, as_overloads) {
    Ufixed<3, 0> unsigned_val(15);

    auto signed_val = as<Sfixed<3, 0>>(unsigned_val);
    EXPECT_EQ(static_cast<int>(signed_val), -1);

    auto restored_u_val = as<Ufixed<3, 0>>(signed_val);
    EXPECT_EQ(static_cast<int>(restored_u_val), 15);

    Ufixed<3, 0> u_val(4);
    auto frac_val = as<Ufixed<-1, -4>>(u_val);
    EXPECT_DOUBLE_EQ(static_cast<double>(frac_val), 0.25);

    Ufixed<3, 0> original_downto(1);
    auto to_val = as<Ufixed<Range{0, Direction::TO, 3}>>(original_downto);

    auto new_downto = as<Ufixed<1, -2>>(to_val);

    EXPECT_DOUBLE_EQ(static_cast<double>(new_downto), 0.25);

    Ufixed<3, 0> u_val_i(5);

    auto bits = as<BitArray<Range{3, Direction::DOWNTO, 0}>>(u_val_i);
    EXPECT_TRUE(bits[0] == Bit('1'));
    EXPECT_TRUE(bits[1] == Bit('0'));
    EXPECT_TRUE(bits[2] == Bit('1'));
    EXPECT_TRUE(bits[3] == Bit('0'));
    auto s_val = as<Ufixed<3, 0>>(bits);
    EXPECT_EQ(static_cast<int>(s_val), 5);
}

TEST(TestUfixed, resize) {
    Ufixed<7, 0> large_val(100);
    auto sat_res = resize<3, 0>(large_val);
    EXPECT_EQ(static_cast<int>(sat_res), 15);

    Ufixed<3, -4> even_tie1(2.5);
    auto rnd_res1 = resize<3, 0>(even_tie1);
    EXPECT_DOUBLE_EQ(static_cast<double>(rnd_res1), 2.0);

    Ufixed<3, -4> even_tie2(3.5);
    auto rnd_res2 = resize<3, 0>(even_tie2);
    EXPECT_DOUBLE_EQ(static_cast<double>(rnd_res2), 4.0);

    Ufixed<3, 0> u_orig(10);
    auto u_wide = resize<7, -4>(u_orig);
    EXPECT_DOUBLE_EQ(static_cast<double>(u_wide), 10.0);

    Ufixed<7, 0> big_unsigned(200);
    auto wrap_res = resize<3, 0>(big_unsigned, overflow_mode::wrap);
    EXPECT_EQ(static_cast<int>(wrap_res), 8);

    Ufixed<7, 0> src(100);
    Ufixed<3, 0> dst = resize(src, overflow_mode::wrap);
    EXPECT_EQ(static_cast<int>(dst), 4);
}

TEST(TestUfixed, ShiftOperators) {
    Ufixed<3, -4> val(1.0);
    auto shifted_left = val << 1;
    EXPECT_TRUE((std::is_same_v<decltype(shifted_left), Ufixed<3, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(shifted_left), 2.0);

    EXPECT_DOUBLE_EQ(static_cast<double>(val << 2), 4.0);

    auto shifted_right = val >> 1;
    EXPECT_TRUE((std::is_same_v<decltype(shifted_right), Ufixed<3, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(shifted_right), 0.5);

    EXPECT_DOUBLE_EQ(static_cast<double>(val >> 2), 0.25);

    Ufixed<3, -4> compound_val(1.5);  // raw 24
    compound_val <<= 1;
    EXPECT_DOUBLE_EQ(static_cast<double>(compound_val), 3.0);  // raw 48

    compound_val >>= 2;
    EXPECT_DOUBLE_EQ(static_cast<double>(compound_val), 0.75);  // raw 12

    int neg_shift = -1;
    EXPECT_THROW(val << neg_shift, std::invalid_argument);
    EXPECT_THROW(val >> neg_shift, std::invalid_argument);
    EXPECT_THROW(compound_val <<= neg_shift, std::invalid_argument);
    EXPECT_THROW(compound_val >>= neg_shift, std::invalid_argument);
}

TEST(TestUfixed, ComparisonOperators) {
    Ufixed<5, -2> a(3.25);
    Ufixed<5, -2> b(3.25);
    Ufixed<5, -2> c(4.5);
    Ufixed<5, -2> d(1.0);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);

    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a != b);

    EXPECT_TRUE(d < a);
    EXPECT_FALSE(c < a);

    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a <= c);
    EXPECT_FALSE(c <= a);

    EXPECT_TRUE(c > a);
    EXPECT_FALSE(d > a);

    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(c >= a);
    EXPECT_FALSE(d >= a);
}

TEST(TestUfixed, AtOrdinalIndexing) {
    auto ba = "100110"_b;
    Ufixed<4, -1> uf(ba);

    EXPECT_TRUE(uf.at_ordinal(0) && uf.at_ordinal(3) && uf.at_ordinal(4));
    EXPECT_FALSE(uf.at_ordinal(1) || uf.at_ordinal(5) || uf.at_ordinal(2));

    EXPECT_THROW(uf.at_ordinal(6), std::out_of_range);
    EXPECT_THROW(uf.at_ordinal(7), std::out_of_range);
}

TEST(TestUfixed, Indexing) {
    auto ba = "100110"_b;
    Ufixed<4, -1> uf(ba);

    EXPECT_TRUE(uf[4] && uf[1] && uf[0]);
    EXPECT_FALSE(uf[-1] || uf[3] || uf[2]);

    EXPECT_THROW(uf[6], std::out_of_range);
    EXPECT_THROW(uf[7], std::out_of_range);
}

TEST(TestUfixed, IterationSurface) {
    Ufixed<3, -4> val(6.5);

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

TEST(TestUfixed, RoundFreeFunctions) {
    EXPECT_EQ((floor(Ufixed<10, -10>(9.6))), (Ufixed<10, 0>(9)));

    EXPECT_EQ((floor(Ufixed<10, -10>(9.025))), (Ufixed<10, 0>(9)));

    EXPECT_EQ((ceil(Ufixed<10, -10>(9.6))), (Ufixed<10, 0>(10)));

    EXPECT_EQ((ceil(Ufixed<10, -10>(9.025))), (Ufixed<10, 0>(10)));

    EXPECT_EQ((trunc(Ufixed<10, -10>(9.6))), (Ufixed<10, 0>(9)));

    EXPECT_EQ((trunc(Ufixed<10, -10>(9.025))), (Ufixed<10, 0>(9)));

    EXPECT_EQ((round(Ufixed<10, -10>(9.6))), (Ufixed<10, 0>(10)));

    EXPECT_EQ((round(Ufixed<10, -10>(9.5))), (Ufixed<10, 0>(10)));

    EXPECT_EQ((round(Ufixed<10, -10>(9.052))), (Ufixed<10, 0>(9)));
}

TEST(TestUfixed, Reverse) {
    auto ba = "10010110"_b;
    auto ba_r = "01101001"_b;

    Ufixed<3, -4> uf_down(ba);
    Ufixed<-4, Direction::TO, 3> uf_to(ba);

    auto r_to = reverse(uf_down);  // only direction changed
    auto r_down = reverse(uf_to);  // bits also reversed

    EXPECT_EQ(r_to, uf_to);
    EXPECT_EQ(r_down, (Ufixed<3, -4>(ba_r)));
}

TEST(TestUfixed, Formatter) {
    Ufixed<3, -4> val(5.0625);
    EXPECT_EQ(std::format("{}", val), std::format("{:d}", val));
    EXPECT_EQ(std::format("{:d}", val), "Ufixed[3 downto -4]{5.0625}");
    EXPECT_EQ(std::format("{:b}", val), "Ufixed[3 downto -4]{0101.0001}");

    Ufixed<7, 0> int_val(150);
    EXPECT_EQ(std::format("{:b}", int_val), "Ufixed[7 downto 0]{10010110}");

    Ufixed<-1, -4> frac_val(0.3125);
    EXPECT_EQ(std::format("{:b}", frac_val), "Ufixed[-1 downto -4]{0101}");
    EXPECT_EQ(std::format("{:d}", frac_val), "Ufixed[-1 downto -4]{0.3125}");

    EXPECT_THROW(
        auto s = std::vformat("{:x}", std::make_format_args(val)), std::format_error
    );
    EXPECT_THROW(
        auto s = std::vformat("{:o}", std::make_format_args(val)), std::format_error
    );
    EXPECT_THROW(
        auto s = std::vformat("{:e}", std::make_format_args(val)), std::format_error
    );
}

TEST(TestUfixed, Hash) {
    Ufixed<3, 0> a(10);
    Ufixed<3, 0> b(10);
    Ufixed<3, 0> c(11);

    auto hash_a = std::hash<Ufixed<3, 0>>{}(a);
    auto hash_b = std::hash<Ufixed<3, 0>>{}(b);
    auto hash_c = std::hash<Ufixed<3, 0>>{}(c);

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

    Ufixed<100, 0> wide_a(1);
    wide_a <<= 80;

    Ufixed<100, 0> wide_b(1);
    wide_b <<= 80;

    Ufixed<100, 0> wide_c(1);
    wide_c <<= 79;  // Different bit pattern

    auto hash_wide_a = std::hash<Ufixed<100, 0>>{}(wide_a);
    auto hash_wide_b = std::hash<Ufixed<100, 0>>{}(wide_b);
    auto hash_wide_c = std::hash<Ufixed<100, 0>>{}(wide_c);

    EXPECT_EQ(hash_wide_a, hash_wide_b);
    EXPECT_NE(hash_wide_a, hash_wide_c);
}

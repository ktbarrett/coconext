// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <cfenv>
#include <coconext/types.hpp>
#include <type_traits>
#include <utility>

using namespace coconext::types;
using namespace coconext::literals;

template <typename T>
concept HasSignedFixedOrdering = requires(T a, T b) { a <=> b; };

TEST(TestSfixed, ReviewRegressions) {
    Sfixed<3, 0> deduced_resize = resize(Sfixed<7, 0>(100));
    EXPECT_EQ(static_cast<int>(deduced_resize), 7);

    Sfixed<3, 0> converted(Ufixed<3, 0>(1));
    EXPECT_EQ(static_cast<int>(converted), 1);

    Sfixed<3, 0> negative(-1);
    static_assert(!noexcept(static_cast<unsigned char>(negative)));
    EXPECT_THROW((void)static_cast<unsigned char>(negative), std::out_of_range);

    Sfixed<100, 0> native_wide(1);
    native_wide <<= 80;
    EXPECT_DOUBLE_EQ(static_cast<double>(native_wide), std::ldexp(1.0, 80));
#if defined(__SIZEOF_INT128__)
    EXPECT_TRUE(static_cast<__int128_t>(native_wide) == (__int128_t{1} << 80));
#endif

    double const two_to_80 = std::ldexp(1.0, 80);
    Sfixed<100, 0> float_wide(two_to_80);
    EXPECT_DOUBLE_EQ(static_cast<double>(float_wide), two_to_80);
    EXPECT_EQ(static_cast<unsigned>(Sfixed<100, -50>(-0.5)), 0U);

    Sfixed<-1, -4> pure_fraction(0);
    EXPECT_DOUBLE_EQ(static_cast<double>(pure_fraction), 0.0);

    Sfixed<0, -63> unit_fraction(-1);
    EXPECT_EQ(static_cast<int>(unit_fraction), -1);

    Sfixed<10, 3> positive_lsb(-8);
    static_assert(std::is_signed_v<decltype(Sfixed<10, 3>::frac_bits())>);
    static_assert(std::is_signed_v<decltype(Sfixed<10, 3>::int_bits())>);
    static_assert(Sfixed<10, 3>::frac_bits() == -3);
    static_assert(Sfixed<10, 3>::int_bits() == 11);
    EXPECT_DOUBLE_EQ(static_cast<double>(positive_lsb), -8.0);
    EXPECT_EQ(static_cast<int>(positive_lsb), -8);
    EXPECT_DOUBLE_EQ(static_cast<double>(Sfixed<10, 3>(1016)), 1016.0);
    EXPECT_EQ(std::format("{:d}", Sfixed<10, 3>(-96)), "Sfixed[10 downto 3]{-96}");

    using Subnormal = Sfixed<-5, -10>;
    static_assert(Subnormal::frac_bits() == 10);
    static_assert(Subnormal::int_bits() == -4);
    static_assert(Subnormal::int_bits() + Subnormal::frac_bits() == Subnormal::size());
    Subnormal subnormal(-1.0 / 32.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(subnormal), -1.0 / 32.0);
    EXPECT_EQ(std::format("{:d}", subnormal), "Sfixed[-5 downto -10]{-0.0312500000}");

    EXPECT_THROW((Sfixed<3, 0>(Unsigned<4>(15))), std::out_of_range);

    auto remainder = Sfixed<2, 0>(2) % Sfixed<5, 0>(3);
    static_assert(std::is_same_v<decltype(remainder), Sfixed<2, 0>>);
    EXPECT_EQ(static_cast<int>(remainder), 2);

    Sfixed<3, 0> shifted(1);
    static_assert(std::is_same_v<decltype(shifted <<= 1), Sfixed<3, 0>&>);
    (shifted <<= 1) <<= 1;
    EXPECT_EQ(static_cast<int>(shifted), 4);

    static_assert(!HasSignedFixedOrdering<Sfixed<0, Direction::TO, 3>>);
}

TEST(TestSfixed, ZeroWidth) {
    constexpr Range NullRange{-1, Direction::DOWNTO, 0};
    using NullSfixed = Sfixed<NullRange>;

    NullSfixed a{};
    NullSfixed b{};
    static_assert(NullSfixed::size() == 0);
    EXPECT_EQ(a, b);
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_EQ(a.begin(), a.end());
    EXPECT_EQ(std::format("{:b}", a), "Sfixed[-1 downto 0]{}");
    EXPECT_EQ(std::format("{:d}", a), "Sfixed[-1 downto 0]{}");

    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), Sfixed<0, 0>>);
    EXPECT_EQ(static_cast<int>(sum), 0);
    EXPECT_FALSE(static_cast<bool>(a * b));

    a += Sfixed<3, 0>(1);
    a -= Sfixed<3, 0>(1);
    a *= Sfixed<3, 0>(2);
    a /= Sfixed<3, 0>(1);
    a %= Sfixed<3, 0>(1);
    EXPECT_EQ(a, NullSfixed{});

    NullSfixed from_signed(Signed<0>{});
    Sfixed<3, 0> widened = from_signed;
    EXPECT_EQ(static_cast<int>(widened), 0);
    EXPECT_FALSE(static_cast<bool>(resize<NullRange>(Sfixed<3, 0>(7))));

    EXPECT_THROW((a /= Sfixed<3, 0>(0)), std::domain_error);
    EXPECT_THROW((a %= Sfixed<3, 0>(0)), std::domain_error);

    EXPECT_EQ(static_cast<int>(Ufixed<3, 0>(1) + Signed<0>{}), 1);
    EXPECT_EQ(static_cast<int>(Signed<0>{} + Sfixed<3, 0>(1)), 1);
    EXPECT_THROW((void)(Sfixed<3, 0>(1) / Signed<0>{}), std::domain_error);

    Unsigned<0> unsigned_lhs{};
    unsigned_lhs += Sfixed<3, 0>(1);
    unsigned_lhs -= Sfixed<3, 0>(1);
    unsigned_lhs *= Sfixed<3, 0>(1);
    EXPECT_THROW((unsigned_lhs /= Sfixed<3, 0>(0)), std::domain_error);
    EXPECT_THROW((unsigned_lhs %= Sfixed<3, 0>(0)), std::domain_error);
    EXPECT_EQ(unsigned_lhs, Unsigned<0>{});

    Signed<0> signed_lhs{};
    signed_lhs += Ufixed<3, 0>(1);
    EXPECT_EQ(signed_lhs, Signed<0>{});
}

TEST(TestSfixed, SubnormalSupernormalInterfaces) {
    using Supernormal = Sfixed<10, 3>;
    using WideSupernormal = Sfixed<15, 3>;
    static_assert(!noexcept(static_cast<signed char>(std::declval<Supernormal const&>())));
    static_assert(!std::is_convertible_v<int8_t, WideSupernormal>);
    EXPECT_THROW((void)static_cast<signed char>(Supernormal(1016)), std::out_of_range);
    EXPECT_EQ(WideSupernormal(int8_t{120}), WideSupernormal(120));
    EXPECT_THROW((WideSupernormal(int8_t{127})), std::out_of_range);

    EXPECT_THROW((Supernormal(Ufixed<10, 3>(2040))), std::out_of_range);
    EXPECT_EQ(Supernormal(Ufixed<9, 3>(1016)), Supernormal(1016));

    using Subnormal = Sfixed<-5, -10>;
    auto unsigned_subnormal = as<Ufixed<-5, -10>>(BitArray<6>("111111"));
    EXPECT_THROW((Subnormal(unsigned_subnormal)), std::out_of_range);
    EXPECT_EQ(
        Subnormal(as<Ufixed<-6, -10>>(BitArray<5>("11111"))), Subnormal(31.0 / 1024.0)
    );

    static_assert(!std::is_convertible_v<Signed<11>, Supernormal>);
    static_assert(!std::is_convertible_v<Signed<12>, Supernormal>);
    static_assert(!std::is_convertible_v<Unsigned<10>, Supernormal>);
    static_assert(!std::is_convertible_v<Unsigned<11>, Supernormal>);

    Supernormal from_signed(Signed<11>(1016));
    Supernormal from_unsigned(Unsigned<10>(1016));
    EXPECT_EQ(from_signed, Supernormal(1016));
    EXPECT_EQ(from_unsigned, Supernormal(1016));
    EXPECT_THROW((Supernormal(Signed<11>(1023))), std::out_of_range);
    EXPECT_THROW((Supernormal(Unsigned<10>(1023))), std::out_of_range);
    EXPECT_EQ(Supernormal(Signed<12>(-1024)), Supernormal(-1024));
    EXPECT_THROW((Supernormal(Signed<12>(1024))), std::out_of_range);
    EXPECT_THROW((Supernormal(Unsigned<12>(1024))), std::out_of_range);
    EXPECT_NO_THROW((Subnormal(Signed<8>(0))));
    EXPECT_THROW((Subnormal(Signed<8>(1))), std::out_of_range);
    EXPECT_THROW((Subnormal(Unsigned<8>(1))), std::out_of_range);

    Sfixed<3, -4> fractional_from_signed = Signed<4>(-5);
    EXPECT_DOUBLE_EQ(static_cast<double>(fractional_from_signed), -5.0);

    auto positive_tiny = as<Sfixed<-95, -100>>(BitArray<6>("000001"));
    auto negative_tiny = as<Sfixed<-95, -100>>(BitArray<6>("111111"));
    EXPECT_EQ(
        (resize<10, 3>(positive_tiny, overflow_mode::saturate, round_mode::round_to_pos)),
        Supernormal(8)
    );
    EXPECT_EQ(
        (resize<10, 3>(negative_tiny, overflow_mode::saturate, round_mode::truncate)),
        Supernormal(-8)
    );
    EXPECT_EQ(
        (resize<10, 3>(negative_tiny, overflow_mode::saturate, round_mode::round_to_zero)),
        Supernormal(0)
    );

    auto min_double = Sfixed<-1073, -1075>(std::numeric_limits<double>::denorm_min());
    EXPECT_EQ(as<BitArray<3>>(min_double), BitArray<3>("010"));

    auto rounded_down =
        Sfixed<1101, 1100>(-1.0, overflow_mode::saturate, round_mode::truncate);
    EXPECT_EQ(as<BitArray<2>>(rounded_down), BitArray<2>("11"));

    Subnormal compound(1.0 / 1024.0);
    auto const original = compound;
    EXPECT_NO_THROW(compound += 1);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(++compound);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(--compound);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(compound *= 1);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(compound /= 1);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(compound %= 1);
    EXPECT_EQ(compound, original);
}

TEST(TestSfixed, RoundingModeIndependence) {
    int const original_rounding_mode = std::fegetround();
    ASSERT_EQ(std::fesetround(FE_DOWNWARD), 0);
    Sfixed<3, -1> rounded(1.75, overflow_mode::saturate, round_mode::round_to_even);
    ASSERT_EQ(std::fesetround(original_rounding_mode), 0);
    EXPECT_DOUBLE_EQ(static_cast<double>(rounded), 2.0);
}

TEST(TestSfixed, shape_and_typelevel) {
    Sfixed<6, -2> a(-37.25);
    Sfixed<-2, Direction::TO, 6> b;

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

    Sfixed<100, -50> wide_a(-37.25);
    static_assert(wide_a.size() == 151);
    static_assert(wide_a.int_bits() == 101);
    static_assert(wide_a.frac_bits() == 50);
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

    Sfixed<100, -50> wide_zero(0.0);
    Sfixed<100, -50> wide_nonzero(-0.0625);
    EXPECT_FALSE(static_cast<bool>(wide_zero));
    EXPECT_TRUE(static_cast<bool>(wide_nonzero));
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
    EXPECT_THROW((void)static_cast<unsigned char>(large_val), std::out_of_range);
    EXPECT_THROW((void)static_cast<signed char>(large_val), std::out_of_range);

    Sfixed<100, -50> wide_val(-5.9375);
    EXPECT_EQ(static_cast<int>(wide_val), -5);
    Sfixed<100, -50> wide_huge(1);
    wide_huge <<= 65;
    EXPECT_EQ(
        std::format("{}", wide_huge),
        "Sfixed[100 downto "
        "-50]{36893488147419103232.00000000000000000000000000000000000000000000000000}"
    );
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

    Sfixed<100, -50> wide_float(-5.0625);
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_float), -5.0625);
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
    EXPECT_THROW((Sfixed<3, 0>(wide)), std::out_of_range);
    Sfixed<3, 0> narrow_saturated = resize<3, 0>(wide);
    Sfixed<3, 0> narrow_wrapped =
        resize<3, 0>(wide, overflow_mode::wrap, round_mode::round_to_even);

    EXPECT_EQ(static_cast<int>(narrow_saturated), -8);
    EXPECT_EQ(static_cast<int>(narrow_wrapped), -4);

    Sfixed<7, -4> fractional(5.5);
    EXPECT_THROW((Sfixed<3, 0>(fractional)), std::out_of_range);
    EXPECT_EQ(static_cast<int>(Sfixed<3, 0>(Sfixed<7, -4>(5.0))), 5);

    Sfixed<100, -50> wide_default;
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_default), 0.0);
    Sfixed<100, -50> wide_from_double(-100.5);
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_from_double), -100.5);
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

    Sfixed<100, -50> wide_s(-1);
    auto wide_u = as<Ufixed<100, -50>>(wide_s);
    EXPECT_TRUE(static_cast<bool>(wide_u[100]));
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

    Sfixed<100, -50> wide_orig_res(-5.5);
    auto narrow_from_wide = resize<3, 0>(wide_orig_res);
    EXPECT_EQ(static_cast<int>(narrow_from_wide), -6);
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

    Sfixed<100, -50> wide_shift(-1.0);
    wide_shift <<= 2;
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_shift), -4.0);
    wide_shift >>= 1;
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_shift), -2.0);
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

    Sfixed<100, -50> w_cmp_a(-3.25), w_cmp_b(-3.25), w_cmp_c(4.5);
    EXPECT_TRUE(w_cmp_a == w_cmp_b);
    EXPECT_TRUE(w_cmp_a < w_cmp_c);
}

TEST(TestSfixed, Indexing) {
    auto ba = "100110"_b;
    auto uf = as<Sfixed<4, -1>>(ba);

    EXPECT_TRUE(uf[4] && uf[1] && uf[0]);
    EXPECT_FALSE(uf[-1] || uf[3] || uf[2]);

    EXPECT_THROW(uf[5], std::out_of_range);
    EXPECT_THROW(uf[-2], std::out_of_range);

    Sfixed<100, -50> w_idx(-1);
    EXPECT_TRUE(w_idx[100]);   // Integer part is 1s
    EXPECT_FALSE(w_idx[-50]);  // Fractional part is 0s

    uf[4] = Bit::_0;
    uf[-1] = Bit::_1;
    EXPECT_FALSE(static_cast<bool>(uf[4]));
    EXPECT_TRUE(static_cast<bool>(uf[-1]));

    auto const& const_uf = uf;
    EXPECT_FALSE(static_cast<bool>(const_uf[4]));
    EXPECT_TRUE(static_cast<bool>(const_uf[-1]));
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

    for (auto bit : val) {
        bit = Bit::_0;
    }
    EXPECT_TRUE(std::none_of(val.begin(), val.end(), [](auto bit) {
        return static_cast<bool>(bit);
    }));

    *val.rbegin() = Bit::_1;
    EXPECT_TRUE(static_cast<bool>(val[-4]));

    auto const& const_val = val;
    EXPECT_EQ(std::distance(const_val.begin(), const_val.end()), 8);

    Sfixed<100, -50> w_iter(0);
    EXPECT_EQ(std::distance(w_iter.begin(), w_iter.end()), 151);
}

TEST(TestSfixed, ResizeRoundingModes) {
    Sfixed<10, -10> negative(-9.5);

    EXPECT_EQ(
        (resize<10, 0>(negative, overflow_mode::saturate, round_mode::truncate)),
        (Sfixed<10, 0>(-10))
    );
    EXPECT_EQ(
        (resize<10, 0>(negative, overflow_mode::saturate, round_mode::round_to_zero)),
        (Sfixed<10, 0>(-9))
    );
    EXPECT_EQ(
        (resize<10, 0>(negative, overflow_mode::saturate, round_mode::round_to_pos)),
        (Sfixed<10, 0>(-9))
    );
    EXPECT_EQ(
        (resize<10, 0>(negative, overflow_mode::saturate, round_mode::round)),
        (Sfixed<10, 0>(-10))
    );
    EXPECT_EQ(
        (resize<10, 0>(negative, overflow_mode::saturate, round_mode::round_to_even)),
        (Sfixed<10, 0>(-10))
    );

    Sfixed<-1, -4> subnormal(-0.25);
    EXPECT_EQ(
        (resize<0, 0>(subnormal, overflow_mode::saturate, round_mode::truncate)),
        (Sfixed<0, 0>(-1))
    );
    EXPECT_EQ(
        (resize<1, 0>(-subnormal, overflow_mode::saturate, round_mode::round_to_pos)),
        (Sfixed<1, 0>(1))
    );

    Sfixed<10, 3> supernormal(-16);
    EXPECT_EQ(
        (resize<10, 0>(supernormal, overflow_mode::saturate, round_mode::round)),
        (Sfixed<10, 0>(-16))
    );
}

TEST(TestSfixed, Reverse) {
    auto ba = "11110110"_b;
    auto ba_r = "01101111"_b;

    auto sf_down = as<Sfixed<3, -4>>(ba);
    auto sf_to = as<Sfixed<-4, Direction::TO, 3>>(ba);

    auto r_to = reverse(sf_down);
    auto r_down = reverse(sf_to);

    EXPECT_EQ(r_to, sf_to);
    EXPECT_EQ(r_down, (as<Sfixed<3, -4>>(ba_r)));

    Sfixed<100, -50> w_rev_down(-1);
    auto w_rev_to = reverse(w_rev_down);
    EXPECT_TRUE((std::is_same_v<decltype(w_rev_to), Sfixed<-50, Direction::TO, 100>>));
}

TEST(TestSfixed, Formatter) {
    Sfixed<3, -4> val(-5.0625);
    EXPECT_EQ(std::format("{}", val), std::format("{:d}", val));
    EXPECT_EQ(std::format("{:d}", val), "Sfixed[3 downto -4]{-5.0625}");

    EXPECT_EQ(std::format("{:b}", val), "Sfixed[3 downto -4]{1010.1111}");

    Sfixed<8, -3> val_downto(-120);
    auto val_to = reverse(val_downto);
    EXPECT_THROW(
        (void)std::vformat("{:d}", std::make_format_args(val_to)), std::format_error
    );

    Sfixed<100, -50> w_fmt(-5.0625);
    EXPECT_EQ(
        std::format("{:d}", w_fmt),
        "Sfixed[100 downto -50]{-5.06250000000000000000000000000000000000000000000000}"
    );
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

    auto u_downto = as<Ufixed<3, 0>>(raw_bits);
    auto u_shifted = as<Ufixed<2, -1>>(raw_bits);
    auto u_to = as<Ufixed<Range{0, Direction::TO, 3}>>(raw_bits);
    auto s_downto = as<Sfixed<3, 0>>(raw_bits);

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

    Sfixed<100, -50> w_hash_a(5), w_hash_b(5), w_hash_c(2);
    EXPECT_EQ(
        (std::hash<Sfixed<100, -50>>{}(w_hash_a)), (std::hash<Sfixed<100, -50>>{}(w_hash_b))
    );
    EXPECT_NE(
        (std::hash<Sfixed<100, -50>>{}(w_hash_a)), (std::hash<Sfixed<100, -50>>{}(w_hash_c))
    );
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

    Sfixed<100, -50> w_bw_a(-3), w_bw_b(2);
    EXPECT_EQ((as<Sfixed<100, -50>>(w_bw_a & w_bw_b)), (Sfixed<100, -50>(0)));
    EXPECT_TRUE(or_reduce(w_bw_a));
}

TEST(TestSfixed, Concatenation) {
    Sfixed<3, 0> a(5);
    Sfixed<1, -2> b(-0.25);

    auto cat_res = concat(a, b);
    EXPECT_TRUE((std::is_same_v<decltype(cat_res), BitArray<8>>));

    EXPECT_EQ(static_cast<int>(as<Sfixed<7, 0>>(cat_res)), 95);

    Sfixed<100, 0> w_cat_a(5);
    Sfixed<30, 0> w_cat_b(3);
    auto w_cat_res = concat(w_cat_a, w_cat_b);
    EXPECT_TRUE((std::is_same_v<decltype(w_cat_res), BitArray<132>>));
}

TEST(TestSfixed, SubtypeRoundTrip) {
    static_assert(
        !std::is_constructible_v<Sfixed<3, -4>, BitArray<Range{3, Direction::DOWNTO, -4}>>
    );

    Sfixed<3, -4> s(5.0625);

    BitArray<Range{3, Direction::DOWNTO, -4}> ba = s;
    auto restored = as<Sfixed<3, -4>>(ba);

    EXPECT_EQ(s, restored);
    EXPECT_TRUE((s == as<Sfixed<3, -4>>(BitArray<Range{3, Direction::DOWNTO, -4}>(s))));

    Sfixed<100, -50> w_rt(-5.0625);
    BitArray<Range{100, Direction::DOWNTO, -50}> w_ba = w_rt;
    EXPECT_EQ(w_rt, (as<Sfixed<100, -50>>(w_ba)));
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

    EXPECT_THROW(
        (Sfixed<100, -50>(std::numeric_limits<double>::infinity(), overflow_mode::wrap)),
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

    Sfixed<100, -50> w_minmax_a(3.25), w_minmax_b(4.5);
    EXPECT_EQ(std::min(w_minmax_a, w_minmax_b), w_minmax_a);
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

    using WideToType = Sfixed<-50, Direction::TO, 100>;
    static_assert(requires(WideToType a) {
        static_cast<BitArray<Range{-50, Direction::TO, 100}>>(a);
    });
}

TEST(TestSfixed, ImplicitCrossKindWidening) {
    Ufixed<3, -4> u(5.0625);

    Sfixed<4, -4> s = u;
    EXPECT_DOUBLE_EQ(static_cast<double>(s), 5.0625);

    static_assert(!std::is_convertible_v<Sfixed<3, -4>, Ufixed<3, -4>>);

    Ufixed<100, -50> w_u_cross(5.0625);
    Sfixed<101, -50> w_s_cross = w_u_cross;
    EXPECT_DOUBLE_EQ(static_cast<double>(w_s_cross), 5.0625);
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

    Sfixed<100, -50> w_abs_val(-8.25);
    auto w_abs_res = abs(w_abs_val);
    EXPECT_TRUE((std::is_same_v<decltype(w_abs_res), Sfixed<101, -50>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(w_abs_res), 8.25);
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

    Sfixed<100, -50> w_un_val(-5.25);
    auto w_un_neg = -w_un_val;
    EXPECT_TRUE((std::is_same_v<decltype(w_un_neg), Sfixed<101, -50>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(w_un_neg), 5.25);
}

TEST(TestSfixed, Arithmetic) {
    Sfixed<5, -6> a(19.25);
    Sfixed<10, -10> b(5.55);  // Actually stored as ~5.5498

    auto c = a + b;
    EXPECT_EQ(c, (Sfixed<11, -10>(24.80)));

    auto g = b + a;
    EXPECT_EQ(g, (Sfixed<11, -10>(24.80)));

    auto d = a - b;
    EXPECT_EQ(d, (Sfixed<11, -10>(13.7)));

    auto e = b - a;
    EXPECT_EQ(e, (Sfixed<11, -10>(-13.7)));

    auto f = a * b;
    auto h = b * a;

    EXPECT_NEAR(static_cast<double>(f), 106.83374, 1e-4);
    EXPECT_NEAR(static_cast<double>(h), 106.83374, 1e-4);

    auto div_res = a / b;
    auto div_res_r = b / a;
    EXPECT_NEAR(static_cast<double>(div_res), 3.46859, 1e-4);
    EXPECT_NEAR(static_cast<double>(div_res_r), 0.28830, 1e-4);

    auto mod_res = a % b;
    auto mod_res_r = b % a;
    EXPECT_NEAR(static_cast<double>(mod_res), 2.60058, 1e-4);
    EXPECT_NEAR(static_cast<double>(mod_res_r), 5.54980, 1e-4);

    Sfixed<5, -6> a_n(-19.25);
    Sfixed<10, -10> b_n(-5.55);

    EXPECT_EQ(a_n + b_n, (Sfixed<11, -10>(-24.80)));
    EXPECT_EQ(a_n + b, (Sfixed<11, -10>(-13.7)));
    EXPECT_EQ(a + b_n, (Sfixed<11, -10>(13.7)));

    EXPECT_EQ(a_n - b_n, (Sfixed<11, -10>(-13.7)));
    EXPECT_EQ(a_n - b, (Sfixed<11, -10>(-24.80)));
    EXPECT_EQ(a - b_n, (Sfixed<11, -10>(24.80)));

    EXPECT_NEAR(static_cast<double>(a_n * b_n), 106.83374, 1e-4);
    EXPECT_NEAR(static_cast<double>(a_n * b), -106.83374, 1e-4);
    EXPECT_NEAR(static_cast<double>(a * b_n), -106.83374, 1e-4);

    EXPECT_NEAR(static_cast<double>(a_n / b_n), 3.46859, 1e-4);
    EXPECT_NEAR(static_cast<double>(a_n / b), -3.46859, 1e-4);
    EXPECT_NEAR(static_cast<double>(a / b_n), -3.46859, 1e-4);

    EXPECT_NEAR(static_cast<double>(a_n % b_n), -2.60058, 1e-4);
    EXPECT_NEAR(static_cast<double>(a_n % b), -2.60058, 1e-4);
    EXPECT_NEAR(static_cast<double>(a % b_n), 2.60058, 1e-4);

    Sfixed<100, -50> w_arith_a(19.25), w_arith_b(5.5);
    auto w_arith_c = w_arith_a + w_arith_b;
    EXPECT_TRUE((std::is_same_v<decltype(w_arith_c), Sfixed<101, -50>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(w_arith_c), 24.75);
}

TEST(TestSfixed, DivisionFamily) {
    Sfixed<2, 0> negative_two(-2);
    Sfixed<2, 0> three(3);

    auto quotient = negative_two / three;
    static_assert(std::is_same_v<decltype(quotient), Sfixed<3, -2>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(quotient), -0.75);
    EXPECT_EQ(quotient, divide(negative_two, three));
    EXPECT_DOUBLE_EQ(
        static_cast<double>(divide(negative_two, three, round_mode::round_to_even, 0)), -0.5
    );
    EXPECT_DOUBLE_EQ(
        static_cast<double>(divide(negative_two, three, round_mode::round_to_zero, 3)), -0.5
    );
    EXPECT_DOUBLE_EQ(
        static_cast<double>(divide(negative_two, three, round_mode::truncate, 3)), -0.75
    );

    Sfixed<3, 0> negative_five(-5);
    Sfixed<3, 0> positive_five(5);
    Sfixed<2, 0> negative_three(-3);

    auto remainder_result = remainder(negative_five, three);
    auto modulo_result = modulo(negative_five, three);
    static_assert(std::is_same_v<decltype(remainder_result), Sfixed<2, 0>>);
    static_assert(std::is_same_v<decltype(modulo_result), Sfixed<2, 0>>);
    EXPECT_EQ(static_cast<int>(remainder_result), -2);
    EXPECT_EQ(static_cast<int>(modulo_result), 1);
    EXPECT_EQ(rem(negative_five, three), remainder_result);
    EXPECT_EQ(mod(negative_five, three), modulo_result);
    EXPECT_EQ(static_cast<int>(remainder(positive_five, negative_three)), 2);
    EXPECT_EQ(static_cast<int>(modulo(positive_five, negative_three)), -1);
    EXPECT_EQ(negative_five % three, remainder_result);

    auto [divrem_quotient, divrem_remainder] = divrem(negative_five, three);
    EXPECT_EQ(divrem_quotient, divide(negative_five, three));
    EXPECT_EQ(divrem_remainder, remainder_result);
    auto [divmod_quotient, divmod_modulo] = divmod(negative_five, three);
    EXPECT_EQ(divmod_quotient, divide(negative_five, three));
    EXPECT_EQ(divmod_modulo, modulo_result);

    Sfixed<3, -2> fractional_dividend(-5.25);
    Sfixed<2, -1> fractional_divisor(2.5);
    auto [fractional_quotient, fractional_remainder] =
        divrem(fractional_dividend, fractional_divisor);
    auto [fractional_mod_quotient, fractional_modulo] =
        divmod(fractional_dividend, fractional_divisor);
    static_assert(std::is_same_v<decltype(fractional_quotient), Sfixed<5, -4>>);
    static_assert(std::is_same_v<decltype(fractional_remainder), Sfixed<2, -2>>);
    static_assert(std::is_same_v<decltype(fractional_modulo), Sfixed<2, -2>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(fractional_quotient), -2.125);
    EXPECT_EQ(fractional_mod_quotient, fractional_quotient);
    EXPECT_DOUBLE_EQ(static_cast<double>(fractional_remainder), -0.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(fractional_modulo), 2.25);

    Sfixed<6, 4> coarse_dividend(32);
    Sfixed<1, -1> one_and_a_half(1.5);
    static_assert(std::is_same_v<decltype(coarse_dividend / one_and_a_half), Sfixed<8, 3>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(coarse_dividend / one_and_a_half), 24.0);
    EXPECT_DOUBLE_EQ(
        static_cast<double>(
            divide(coarse_dividend, one_and_a_half, round_mode::round_to_even, 1)
        ),
        16.0
    );

    constexpr Range NullRange{-1, Direction::DOWNTO, 0};
    auto [zero_quotient, zero_remainder] = divrem(Sfixed<NullRange>{}, three);
    EXPECT_FALSE(static_cast<bool>(zero_quotient));
    EXPECT_FALSE(static_cast<bool>(zero_remainder));
    EXPECT_THROW((void)divide(three, Sfixed<NullRange>{}), std::domain_error);

    auto reciprocal_result = reciprocal(three);
    static_assert(std::is_same_v<decltype(reciprocal_result), Sfixed<1, -2>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(reciprocal_result), 0.25);

    EXPECT_THROW((void)divide(negative_two, Sfixed<2, 0>(0)), std::domain_error);
    EXPECT_THROW((void)remainder(negative_two, Sfixed<2, 0>(0)), std::domain_error);
    EXPECT_THROW((void)modulo(negative_two, Sfixed<2, 0>(0)), std::domain_error);
}

TEST(TestSfixed, CompoundArithmetic) {
    Sfixed<5, -6> a(19.25);
    Sfixed<10, -10> b(5.55);

    Sfixed<5, -6> a_n(-19.25);
    Sfixed<10, -10> b_n(-5.55);

    Sfixed<11, -10> c(a);
    c += b;
    EXPECT_EQ(c, (Sfixed<11, -10>(24.80)));

    // 7 + 19.25 = 26.25 -> wrap 4-bit = -6
    Sfixed<3, 0> z(7);
    z += a;
    EXPECT_EQ(z, (Sfixed<3, 0>(-6)));

    // -8 - 19.25 = -27.25 -> wrap 4-bit = 5
    Sfixed<3, 0> z_min(-8);
    z_min -= a;
    EXPECT_EQ(z_min, (Sfixed<3, 0>(5)));

    Sfixed<11, -10> d(a);
    d -= b;
    EXPECT_EQ(d, (Sfixed<11, -10>(13.7)));

    Sfixed<16, -16> f(a);
    f *= b;
    EXPECT_NEAR(static_cast<double>(f), 106.83374, 1e-4);

    Sfixed<15, -10> div_res(a);
    div_res /= b;
    EXPECT_NEAR(static_cast<double>(div_res), 3.4677734375, 1e-6);

    Sfixed<15, -10> div_res_r(b);
    div_res_r /= a;
    EXPECT_NEAR(static_cast<double>(div_res_r), 0.2880859375, 1e-6);

    Sfixed<10, -10> mod_res(a);
    mod_res %= b;
    EXPECT_NEAR(static_cast<double>(mod_res), 2.60058, 1e-4);

    Sfixed<10, -10> mod_res_r(b);
    mod_res_r %= a;
    EXPECT_NEAR(static_cast<double>(mod_res_r), 5.54980, 1e-4);

    Sfixed<11, -10> c_nn(a_n);
    c_nn += b_n;
    EXPECT_EQ(c_nn, (Sfixed<11, -10>(-24.80)));
    Sfixed<11, -10> c_np(a_n);
    c_np += b;
    EXPECT_EQ(c_np, (Sfixed<11, -10>(-13.7)));
    Sfixed<11, -10> c_pn(a);
    c_pn += b_n;
    EXPECT_EQ(c_pn, (Sfixed<11, -10>(13.7)));

    Sfixed<11, -10> d_nn(a_n);
    d_nn -= b_n;
    EXPECT_EQ(d_nn, (Sfixed<11, -10>(-13.7)));
    Sfixed<11, -10> d_np(a_n);
    d_np -= b;
    EXPECT_EQ(d_np, (Sfixed<11, -10>(-24.80)));
    Sfixed<11, -10> d_pn(a);
    d_pn -= b_n;
    EXPECT_EQ(d_pn, (Sfixed<11, -10>(24.80)));

    Sfixed<16, -16> f_nn(a_n);
    f_nn *= b_n;
    EXPECT_NEAR(static_cast<double>(f_nn), 106.83374, 1e-4);
    Sfixed<16, -16> f_np(a_n);
    f_np *= b;
    EXPECT_NEAR(static_cast<double>(f_np), -106.83374, 1e-4);
    Sfixed<16, -16> f_pn(a);
    f_pn *= b_n;
    EXPECT_NEAR(static_cast<double>(f_pn), -106.83374, 1e-4);

    Sfixed<15, -10> div_nn(a_n);
    div_nn /= b_n;
    EXPECT_NEAR(static_cast<double>(div_nn), 3.4677734375, 1e-6);
    Sfixed<15, -10> div_np(a_n);
    div_np /= b;
    EXPECT_NEAR(static_cast<double>(div_np), -3.4677734375, 1e-6);
    Sfixed<15, -10> div_pn(a);
    div_pn /= b_n;
    EXPECT_NEAR(static_cast<double>(div_pn), -3.4677734375, 1e-6);

    Sfixed<10, -10> mod_nn(a_n);
    mod_nn %= b_n;
    EXPECT_NEAR(static_cast<double>(mod_nn), -2.60058, 1e-4);
    Sfixed<10, -10> mod_np(a_n);
    mod_np %= b;
    EXPECT_NEAR(static_cast<double>(mod_np), -2.60058, 1e-4);
    Sfixed<10, -10> mod_pn(a);
    mod_pn %= b_n;
    EXPECT_NEAR(static_cast<double>(mod_pn), 2.60058, 1e-4);

    Sfixed<100, -50> w_comp_a(19.25), w_comp_b(5.5);
    w_comp_a += w_comp_b;
    EXPECT_DOUBLE_EQ(static_cast<double>(w_comp_a), 24.75);
}

TEST(TestSfixed, CompoundArithmeticNativeInt) {
    Sfixed<11, -10> val(5.25);

    val += 2;
    EXPECT_EQ(val, (Sfixed<11, -10>(7.25)));

    val -= 1;
    EXPECT_EQ(val, (Sfixed<11, -10>(6.25)));

    val *= 3;
    EXPECT_EQ(val, (Sfixed<11, -10>(18.75)));

    val /= 2;
    EXPECT_EQ(val, (Sfixed<11, -10>(9.375)));

    val %= 4;
    EXPECT_EQ(val, (Sfixed<11, -10>(1.375)));

    Sfixed<3, 0> tight(2);
    tight += 6;
    EXPECT_EQ(tight, (Sfixed<3, 0>(-8)));  // 2 + 6 = 8. Wrapped to 4-bits gives -8.

    tight -= 11;
    EXPECT_EQ(tight, (Sfixed<3, 0>(-3)));

    Sfixed<11, -10> val_n(-5.25);
    val_n += 2;
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-3.25)));
    val_n -= 1;
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-4.25)));
    val_n *= 3;
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-12.75)));
    val_n /= 2;
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-6.375)));
    val_n %= 4;
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-2.375)));

    Sfixed<11, -10> val_nn(5.25);
    val_nn += -2;
    EXPECT_EQ(val_nn, (Sfixed<11, -10>(3.25)));
    val_nn -= -1;
    EXPECT_EQ(val_nn, (Sfixed<11, -10>(4.25)));
    val_nn *= -3;
    EXPECT_EQ(val_nn, (Sfixed<11, -10>(-12.75)));
    val_nn /= -2;
    EXPECT_EQ(val_nn, (Sfixed<11, -10>(6.375)));
    val_nn %= -4;
    EXPECT_EQ(val_nn, (Sfixed<11, -10>(2.375)));

    Sfixed<100, -50> w_cni(5.25);
    w_cni += 2;
    EXPECT_DOUBLE_EQ(static_cast<double>(w_cni), 7.25);
}

TEST(TestSfixed, IncrementDecrement) {
    Sfixed<11, -10> val(5.25);

    auto pre_inc = ++val;
    EXPECT_EQ(pre_inc, (Sfixed<11, -10>(6.25)));
    EXPECT_EQ(val, (Sfixed<11, -10>(6.25)));

    auto post_inc = val++;
    EXPECT_EQ(post_inc, (Sfixed<11, -10>(6.25)));
    EXPECT_EQ(val, (Sfixed<11, -10>(7.25)));

    auto pre_dec = --val;
    EXPECT_EQ(pre_dec, (Sfixed<11, -10>(6.25)));
    EXPECT_EQ(val, (Sfixed<11, -10>(6.25)));

    auto post_dec = val--;
    EXPECT_EQ(post_dec, (Sfixed<11, -10>(6.25)));
    EXPECT_EQ(val, (Sfixed<11, -10>(5.25)));

    Sfixed<3, 0> max_val(7);
    EXPECT_EQ(++max_val, (Sfixed<3, 0>(-8)));
    EXPECT_EQ(max_val++, (Sfixed<3, 0>(-8)));

    Sfixed<3, 0> min_val(-8);
    EXPECT_EQ(--min_val, (Sfixed<3, 0>(7)));
    EXPECT_EQ(min_val--, (Sfixed<3, 0>(7)));

    Sfixed<11, -10> val_n(-5.25);

    auto pre_inc_n = ++val_n;
    EXPECT_EQ(pre_inc_n, (Sfixed<11, -10>(-4.25)));
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-4.25)));

    auto post_inc_n = val_n++;
    EXPECT_EQ(post_inc_n, (Sfixed<11, -10>(-4.25)));
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-3.25)));

    auto pre_dec_n = --val_n;
    EXPECT_EQ(pre_dec_n, (Sfixed<11, -10>(-4.25)));
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-4.25)));

    auto post_dec_n = val_n--;
    EXPECT_EQ(post_dec_n, (Sfixed<11, -10>(-4.25)));
    EXPECT_EQ(val_n, (Sfixed<11, -10>(-5.25)));

    Sfixed<100, -50> w_inc(5.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(++w_inc), 6.25);
}

TEST(TestSfixed, CrossKindArithmetic) {
    Ufixed<5, -6> u(19.25);

    Sfixed<10, -10> s_pos(5.55);
    Sfixed<10, -10> s_neg(-5.55);

    EXPECT_EQ(u + s_pos, (Sfixed<11, -10>(24.80)));
    EXPECT_EQ(s_pos + u, (Sfixed<11, -10>(24.80)));

    EXPECT_EQ(u + s_neg, (Sfixed<11, -10>(13.7)));
    EXPECT_EQ(s_neg + u, (Sfixed<11, -10>(13.7)));

    EXPECT_EQ(u - s_pos, (Sfixed<11, -10>(13.7)));
    EXPECT_EQ(s_pos - u, (Sfixed<11, -10>(-13.7)));

    EXPECT_EQ(u - s_neg, (Sfixed<11, -10>(24.80)));
    EXPECT_EQ(s_neg - u, (Sfixed<11, -10>(-24.80)));

    EXPECT_NEAR(static_cast<double>(u * s_pos), 106.83374, 1e-4);
    EXPECT_NEAR(static_cast<double>(s_pos * u), 106.83374, 1e-4);

    EXPECT_NEAR(static_cast<double>(u * s_neg), -106.83374, 1e-4);
    EXPECT_NEAR(static_cast<double>(s_neg * u), -106.83374, 1e-4);

    EXPECT_NEAR(static_cast<double>(u / s_pos), 3.46859, 1e-4);
    EXPECT_NEAR(static_cast<double>(s_pos / u), 0.28830, 1e-4);

    EXPECT_NEAR(static_cast<double>(u / s_neg), -3.46859, 1e-4);
    EXPECT_NEAR(static_cast<double>(s_neg / u), -0.28830, 1e-4);

    EXPECT_NEAR(static_cast<double>(u % s_pos), 2.60058, 1e-4);
    EXPECT_NEAR(static_cast<double>(s_pos % u), 5.54980, 1e-4);

    EXPECT_NEAR(static_cast<double>(u % s_neg), 2.60058, 1e-4);
    EXPECT_NEAR(static_cast<double>(s_neg % u), -5.54980, 1e-4);

    auto result = u + s_pos;
    EXPECT_TRUE((std::is_same_v<decltype(result), Sfixed<11, -10>>));

    Ufixed<100, -50> w_x_u(19.25);
    Sfixed<100, -50> w_x_s(-5.5);
    EXPECT_DOUBLE_EQ(static_cast<double>(w_x_u + w_x_s), 13.75);
}

TEST(TestSfixed, UnsignedArithmetic) {
    Unsigned<8> u(20);
    Sfixed<5, -6> s(-5.0);

    static_assert(std::same_as<decltype(u + s), Sfixed<9, -6>>);
    static_assert(std::same_as<decltype(s + u), Sfixed<9, -6>>);

    EXPECT_DOUBLE_EQ(static_cast<double>(u + s), 15.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s + u), 15.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(u - s), 25.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s - u), -25.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(u * s), -100.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s * u), -100.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(u / s), -4.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s / u), -0.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(u % s), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s % u), -5.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(Unsigned<8>(255) + Sfixed<1, 0>(0)), 255.0);

    Unsigned<100> wide_u(20);
    Sfixed<100, -50> wide_s(-5.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_u + wide_s), 15.0);
}

TEST(TestSfixed, UfixedSignedArithmetic) {
    Ufixed<5, -6> u(20.0);
    Signed<8> s(-5);

    static_assert(std::same_as<decltype(u + s), Sfixed<8, -6>>);
    static_assert(std::same_as<decltype(s + u), Sfixed<8, -6>>);

    EXPECT_DOUBLE_EQ(static_cast<double>(u + s), 15.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s + u), 15.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(u - s), 25.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s - u), -25.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(u * s), -100.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s * u), -100.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(u / s), -4.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s / u), -0.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(u % s), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(s % u), -5.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(Ufixed<7, 0>(255) + Signed<8>(0)), 255.0);

    Ufixed<100, -50> wide_u(20.0);
    Signed<100> wide_s(-5);
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_u + wide_s), 15.0);
}

TEST(TestSfixed, CrossTypeCompoundArithmetic) {
    auto check_pair = []<typename LHS, typename RHS>(LHS lhs, RHS const& rhs) {
        auto sum = lhs;
        static_assert(std::same_as<decltype(sum += rhs), LHS&>);
        sum += rhs;
        EXPECT_EQ(sum, LHS(25));

        auto difference = lhs;
        static_assert(std::same_as<decltype(difference -= rhs), LHS&>);
        difference -= rhs;
        EXPECT_EQ(difference, LHS(15));

        auto product = lhs;
        static_assert(std::same_as<decltype(product *= rhs), LHS&>);
        product *= rhs;
        EXPECT_EQ(product, LHS(100));

        auto quotient = lhs;
        static_assert(std::same_as<decltype(quotient /= rhs), LHS&>);
        quotient /= rhs;
        EXPECT_EQ(quotient, LHS(4));

        auto remainder = lhs;
        static_assert(std::same_as<decltype(remainder %= rhs), LHS&>);
        remainder %= rhs;
        EXPECT_EQ(remainder, LHS(0));
    };

    check_pair(Unsigned<8>(20), Ufixed<7, -2>(5));
    check_pair(Unsigned<8>(20), Sfixed<7, -2>(5));
    check_pair(Signed<8>(20), Ufixed<7, -2>(5));
    check_pair(Signed<8>(20), Sfixed<7, -2>(5));
    check_pair(Ufixed<7, -2>(20), Unsigned<8>(5));
    check_pair(Ufixed<7, -2>(20), Signed<8>(5));
    check_pair(Ufixed<7, -2>(20), Sfixed<7, -2>(5));
    check_pair(Sfixed<7, -2>(20), Unsigned<8>(5));
    check_pair(Sfixed<7, -2>(20), Signed<8>(5));
    check_pair(Sfixed<7, -2>(20), Ufixed<7, -2>(5));

    Ufixed<7, -2> unsigned_fixed(20);
    unsigned_fixed += Sfixed<7, -2>(-5);
    EXPECT_EQ(unsigned_fixed, (Ufixed<7, -2>(15)));

    Ufixed<7, -2> negative_result(2);
    EXPECT_THROW((negative_result += Signed<8>(-5)), std::out_of_range);

    Unsigned<8> truncated_unsigned(5);
    truncated_unsigned += Ufixed<0, -1>(0.5);
    EXPECT_EQ(truncated_unsigned, Unsigned<8>(5));

    Signed<8> truncated_signed(-5);
    truncated_signed += Sfixed<0, -1>(-0.5);
    EXPECT_EQ(truncated_signed, Signed<8>(-5));

    Unsigned<100> wide(20);
    wide *= Sfixed<100, -50>(5);
    EXPECT_EQ(wide, Unsigned<100>(100));
}

TEST(TestSfixed, ImplicitSignedArithmetic) {
    Sfixed<5, -6> a(19.25);
    Signed<7, 0> b(15);

    Sfixed<5, -6> a_n(-19.25);
    Signed<7, 0> b_n(-15);

    auto c = a + b;
    EXPECT_EQ(c, (Sfixed<8, -6>(34.25)));

    auto g = b + a;
    EXPECT_EQ(g, (Sfixed<8, -6>(34.25)));

    auto d = a - b;
    EXPECT_EQ(d, (Sfixed<8, -6>(4.25)));

    auto e = b - a;
    EXPECT_EQ(e, (Sfixed<8, -6>(-4.25)));

    auto f = a * b;
    auto h = b * a;

    EXPECT_EQ(static_cast<double>(f), 288.75);
    EXPECT_EQ(static_cast<double>(h), 288.75);

    auto div_res = a / b;
    auto div_res_r = b / a;

    EXPECT_NEAR(static_cast<double>(div_res), 1.2833, 1e-4);
    EXPECT_NEAR(static_cast<double>(div_res_r), 0.78125, 1e-6);

    auto mod_res = a % b;
    auto mod_res_r = b % a;

    EXPECT_NEAR(static_cast<double>(mod_res), 4.25, 1e-4);
    EXPECT_NEAR(static_cast<double>(mod_res_r), 15, 1e-4);

    EXPECT_EQ(a_n + b_n, (Sfixed<8, -6>(-34.25)));
    EXPECT_EQ(a_n - b_n, (Sfixed<8, -6>(-4.25)));
    EXPECT_EQ(static_cast<double>(a_n * b_n), 288.75);
    EXPECT_NEAR(static_cast<double>(a_n / b_n), 1.2833, 1e-4);
    EXPECT_NEAR(static_cast<double>(a_n % b_n), -4.25, 1e-4);

    EXPECT_EQ(a_n + b, (Sfixed<8, -6>(-4.25)));
    EXPECT_EQ(a_n - b, (Sfixed<8, -6>(-34.25)));
    EXPECT_EQ(static_cast<double>(a_n * b), -288.75);
    EXPECT_NEAR(static_cast<double>(a_n / b), -1.2833, 1e-4);
    EXPECT_NEAR(static_cast<double>(a_n % b), -4.25, 1e-4);

    EXPECT_EQ(a + b_n, (Sfixed<8, -6>(4.25)));
    EXPECT_EQ(a - b_n, (Sfixed<8, -6>(34.25)));
    EXPECT_EQ(static_cast<double>(a * b_n), -288.75);
    EXPECT_NEAR(static_cast<double>(a / b_n), -1.2833, 1e-4);
    EXPECT_NEAR(static_cast<double>(a % b_n), 4.25, 1e-4);

    Sfixed<100, -50> w_is_a(19.25);
    Signed<100> w_is_b(-15);
    EXPECT_DOUBLE_EQ(static_cast<double>(w_is_a + w_is_b), 4.25);
}

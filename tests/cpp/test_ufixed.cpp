// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <cfenv>
#include <coconext/types.hpp>
#include <tuple>
#include <type_traits>
#include <utility>

using namespace coconext::types;
using namespace coconext::literals;

template <typename T>
concept HasFixedOrdering = requires(T a, T b) { a <=> b; };

namespace {

using AsUnsigned = Unsigned<8>;
using AsSigned = Signed<8>;
using AsUfixed = Ufixed<3, -4>;
using AsSfixed = Sfixed<3, -4>;
using AsBitArray = BitArray<8>;
using AsTypes = std::tuple<AsUnsigned, AsSigned, AsUfixed, AsSfixed, AsBitArray>;

template <typename Target, typename Source>
constexpr bool check_as_pair() {
    AsBitArray expected("10100101");
    Source source = as<Source>(expected);
    auto accept_reinterpreted = [](Target value) { return value; };

    static_assert(std::same_as<decltype(as<Target>(source)), Target>);
    static_assert(std::same_as<decltype(as<Target>(Source(source))), Target>);
    static_assert(std::same_as<decltype(as<Target>(std::move(source))), Target>);

    Target explicit_lvalue = as<Target>(source);
    Target explicit_rvalue = as<Target>(Source(source));
    Source explicit_moved_source(source);
    Target explicit_moved = as<Target>(std::move(explicit_moved_source));

    Target contextual_lvalue = accept_reinterpreted(as(source));
    Target contextual_rvalue = accept_reinterpreted(as(Source(source)));
    Source contextual_moved_source(source);
    Target contextual_moved = accept_reinterpreted(as(std::move(contextual_moved_source)));

    auto has_expected_bits = [&expected](auto const& value) {
        return detail::bits(value) == detail::bits(expected);
    };
    return has_expected_bits(explicit_lvalue) && has_expected_bits(explicit_rvalue)
        && has_expected_bits(explicit_moved) && has_expected_bits(contextual_lvalue)
        && has_expected_bits(contextual_rvalue) && has_expected_bits(contextual_moved);
}

template <typename Target, typename... Sources>
consteval bool check_as_sources(std::type_identity<std::tuple<Sources...>>) {
    return (check_as_pair<Target, Sources>() && ...);
}

}  // namespace

TEST(TestUfixed, AsReinterpretationMatrix) {
    static_assert(check_as_sources<AsUnsigned>(std::type_identity<AsTypes>{}));
    static_assert(check_as_sources<AsSigned>(std::type_identity<AsTypes>{}));
    static_assert(check_as_sources<AsUfixed>(std::type_identity<AsTypes>{}));
    static_assert(check_as_sources<AsSfixed>(std::type_identity<AsTypes>{}));
    static_assert(check_as_sources<AsBitArray>(std::type_identity<AsTypes>{}));
}

TEST(TestUfixed, ReviewRegressions) {
    Ufixed<3, 0> deduced_resize = resize(Ufixed<7, 0>(200));
    EXPECT_EQ(static_cast<int>(deduced_resize), 15);

    Ufixed<200, 0> high_word(1);
    high_word <<= 150;
    EXPECT_DOUBLE_EQ(static_cast<double>(high_word), std::ldexp(1.0, 150));

    Ufixed<201, -1> widened = Ufixed<200, 0>(1);
    EXPECT_DOUBLE_EQ(static_cast<double>(widened), 1.0);

    Ufixed<7, 0> minuend(0);
    EXPECT_THROW((minuend -= Ufixed<2, 0>(5)), std::out_of_range);

    Ufixed<7, 0> positive_difference(20);
    EXPECT_NO_THROW((positive_difference -= Ufixed<2, 0>(5)));
    EXPECT_EQ(static_cast<int>(positive_difference), 15);

    Ufixed<10, 3> positive_lsb(8);
    static_assert(std::is_signed_v<decltype(Ufixed<10, 3>::frac_bits())>);
    static_assert(std::is_signed_v<decltype(Ufixed<10, 3>::int_bits())>);
    static_assert(Ufixed<10, 3>::frac_bits() == -3);
    static_assert(Ufixed<10, 3>::int_bits() == 11);
    EXPECT_DOUBLE_EQ(static_cast<double>(positive_lsb), 8.0);
    EXPECT_EQ(static_cast<int>(positive_lsb), 8);
    EXPECT_DOUBLE_EQ(static_cast<double>(Ufixed<10, 3>(2040)), 2040.0);
    EXPECT_EQ(std::format("{:d}", Ufixed<10, 3>(96)), "Ufixed[10 downto 3]{96}");

    using Subnormal = Ufixed<-5, -10>;
    static_assert(Subnormal::frac_bits() == 10);
    static_assert(Subnormal::int_bits() == -4);
    static_assert(Subnormal::int_bits() + Subnormal::frac_bits() == Subnormal::size());
    Subnormal subnormal(63.0 / 1024.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(subnormal), 63.0 / 1024.0);
    EXPECT_EQ(std::format("{:d}", subnormal), "Ufixed[-5 downto -10]{0.0615234375}");

    auto quotient = Ufixed<3, 0>(8) / Ufixed<3, 0>(4);
    static_assert(std::is_same_v<decltype(quotient), Ufixed<3, -4>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(quotient), 2.0);

    Ufixed<3, 0> shifted(1);
    static_assert(std::is_same_v<decltype(shifted <<= 1), Ufixed<3, 0>&>);
    (shifted <<= 1) <<= 1;
    EXPECT_EQ(static_cast<int>(shifted), 4);

    static_assert(!HasFixedOrdering<Ufixed<0, Direction::TO, 3>>);
}

TEST(TestUfixed, ZeroWidth) {
    constexpr Range NullRange{-1, Direction::DOWNTO, 0};
    using NullUfixed = Ufixed<NullRange>;

    NullUfixed a{};
    NullUfixed b{};
    static_assert(NullUfixed::size() == 0);
    EXPECT_EQ(a, b);
    EXPECT_FALSE(static_cast<bool>(a));
    EXPECT_EQ(a.begin(), a.end());
    EXPECT_EQ(std::format("{:b}", a), "Ufixed[-1 downto 0]{}");
    EXPECT_EQ(std::format("{:d}", a), "Ufixed[-1 downto 0]{}");

    auto sum = a + b;
    static_assert(std::is_same_v<decltype(sum), Ufixed<0, 0>>);
    EXPECT_EQ(static_cast<unsigned>(sum), 0U);
    EXPECT_FALSE(static_cast<bool>(a * b));

    a += Ufixed<3, 0>(1);
    a -= Ufixed<3, 0>(1);
    a *= Ufixed<3, 0>(2);
    a /= Ufixed<3, 0>(1);
    a %= Ufixed<3, 0>(1);
    EXPECT_EQ(a, NullUfixed{});

    NullUfixed from_unsigned(Unsigned<0>{});
    Ufixed<3, 0> widened = from_unsigned;
    EXPECT_EQ(static_cast<unsigned>(widened), 0U);
    EXPECT_FALSE(static_cast<bool>(resize<NullRange>(Ufixed<3, 0>(15))));

    EXPECT_THROW((a /= Ufixed<3, 0>(0)), std::domain_error);
    EXPECT_THROW((a %= Ufixed<3, 0>(0)), std::domain_error);

    EXPECT_EQ(static_cast<unsigned>(Ufixed<3, 0>(1) + Unsigned<0>{}), 1U);
    EXPECT_EQ(static_cast<unsigned>(Unsigned<0>{} + Ufixed<3, 0>(1)), 1U);
    EXPECT_THROW((void)(Ufixed<3, 0>(1) / Unsigned<0>{}), std::domain_error);
}

TEST(TestUfixed, SubnormalSupernormalInterfaces) {
    using Supernormal = Ufixed<10, 3>;
    using WideSupernormal = Ufixed<15, 3>;
    static_assert(
        !noexcept(static_cast<unsigned char>(std::declval<Supernormal const&>()))
    );
    static_assert(!std::is_convertible_v<uint8_t, WideSupernormal>);
    EXPECT_THROW((void)static_cast<unsigned char>(Supernormal(2040)), std::out_of_range);
    EXPECT_EQ(WideSupernormal(uint8_t{248}), WideSupernormal(248));
    EXPECT_THROW((WideSupernormal(uint8_t{255})), std::out_of_range);

    auto tiny = as<Ufixed<-95, -100>>(BitArray<6>("000001"));
    EXPECT_EQ(
        (resize<10, 3>(tiny, overflow_mode::saturate, round_mode::round_to_pos)),
        Supernormal(8)
    );
    EXPECT_EQ(
        (resize<10, 3>(tiny, overflow_mode::saturate, round_mode::round_to_even)),
        Supernormal(0)
    );

    auto min_double = Ufixed<-1074, -1075>(std::numeric_limits<double>::denorm_min());
    EXPECT_EQ(as<BitArray<2>>(min_double), BitArray<2>("10"));

    auto rounded_up =
        Ufixed<1101, 1100>(1.0, overflow_mode::saturate, round_mode::round_to_pos);
    EXPECT_EQ(as<BitArray<2>>(rounded_up), BitArray<2>("01"));

    Ufixed<-5, -10> compound(1.0 / 1024.0);
    auto const original = compound;
    EXPECT_NO_THROW(compound += 1);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(++compound);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(compound *= 1);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(compound /= 1);
    EXPECT_EQ(compound, original);
    EXPECT_NO_THROW(compound %= 1);
    EXPECT_EQ(compound, original);

    Ufixed<3, 0> signed_native(5);
    signed_native += -2;
    EXPECT_EQ(signed_native, (Ufixed<3, 0>(3)));
    signed_native -= -2;
    EXPECT_EQ(signed_native, (Ufixed<3, 0>(5)));
    EXPECT_THROW((signed_native *= -1), std::out_of_range);
}

TEST(TestUfixed, WideFloatInputAndRoundingModeIndependence) {
    double const two_to_80 = std::ldexp(1.0, 80);
    Ufixed<100, 0> wide(two_to_80);
    EXPECT_DOUBLE_EQ(static_cast<double>(wide), two_to_80);

    int const original_rounding_mode = std::fegetround();
    ASSERT_EQ(std::fesetround(FE_DOWNWARD), 0);
    Ufixed<3, -1> rounded(1.75, overflow_mode::saturate, round_mode::round_to_even);
    ASSERT_EQ(std::fesetround(original_rounding_mode), 0);
    EXPECT_DOUBLE_EQ(static_cast<double>(rounded), 2.0);
}

TEST(TestUfixed, IntegerConstructionMatrix) {
    static_assert(std::is_convertible_v<uint8_t, Ufixed<7, 0>>);
    static_assert(!std::is_convertible_v<uint16_t, Ufixed<7, 0>>);
    static_assert(std::is_constructible_v<Ufixed<7, 0>, int8_t>);
    static_assert(!std::is_convertible_v<int8_t, Ufixed<7, 0>>);

    Ufixed<7, 0> from_native_unsigned = uint8_t{200};
    EXPECT_EQ(static_cast<int>(from_native_unsigned), 200);
    EXPECT_EQ(static_cast<int>(Ufixed<7, 0>(uint16_t{200})), 200);
    EXPECT_THROW((Ufixed<7, 0>(uint16_t{256})), std::out_of_range);
    EXPECT_EQ(static_cast<int>(Ufixed<7, 0>(int16_t{100})), 100);
    EXPECT_THROW((Ufixed<7, 0>(int8_t{-1})), std::out_of_range);

    Ufixed<7, 0> from_narrow_unsigned = Unsigned<4>(15);
    EXPECT_EQ(static_cast<int>(from_narrow_unsigned), 15);
    EXPECT_EQ(static_cast<int>(Ufixed<7, 0>(Unsigned<12>(200))), 200);
    EXPECT_THROW((Ufixed<7, 0>(Unsigned<12>(256))), std::out_of_range);

    EXPECT_EQ(static_cast<int>(Ufixed<7, 0>(Signed<4>(7))), 7);
    EXPECT_EQ(static_cast<int>(Ufixed<7, 0>(Signed<12>(200))), 200);
    EXPECT_THROW((Ufixed<7, 0>(Signed<12>(300))), std::out_of_range);
    EXPECT_THROW((Ufixed<7, 0>(Signed<4>(-1))), std::out_of_range);

    Ufixed<7, -4> from_ufixed = Ufixed<3, 0>(10);
    EXPECT_DOUBLE_EQ(static_cast<double>(from_ufixed), 10.0);
    EXPECT_EQ(static_cast<int>(Ufixed<7, 0>(Sfixed<4, 0>(5))), 5);
    EXPECT_EQ(static_cast<int>(Ufixed<7, 0>(Sfixed<9, 0>(200))), 200);
    EXPECT_THROW((Ufixed<7, 0>(Sfixed<9, 0>(300))), std::out_of_range);

    Ufixed<3, -4> fractional_from_integer = Unsigned<4>(5);
    EXPECT_DOUBLE_EQ(static_cast<double>(fractional_from_integer), 5.0);

    EXPECT_DOUBLE_EQ(static_cast<double>(Ufixed<10, 3>(Unsigned<11>(8))), 8.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(Ufixed<10, 3>(Unsigned<12>(2040))), 2040.0);
    EXPECT_THROW((Ufixed<10, 3>(Unsigned<12>(2041))), std::out_of_range);
    EXPECT_THROW((Ufixed<10, 3>(Unsigned<12>(2048))), std::out_of_range);
}

TEST(TestUfixed, shape_and_typelevel) {
    Ufixed<6, -2> a(37);
    Ufixed<-2, Direction::TO, 6> b;

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

    Ufixed<100, -50> wide_a(37.25);
    static_assert(wide_a.size() == 151);
    static_assert(wide_a.int_bits() == 101);
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

    Ufixed<100, -50> wide_zero(0.0);
    Ufixed<100, -50> wide_nonzero(0.0625);
    EXPECT_FALSE(static_cast<bool>(wide_zero));
    EXPECT_TRUE(static_cast<bool>(wide_nonzero));
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
    EXPECT_NO_THROW(static_cast<void>(static_cast<short>(large_val)));
    EXPECT_THROW((void)static_cast<unsigned char>(large_val), std::out_of_range);
    EXPECT_THROW((void)static_cast<signed char>(large_val), std::out_of_range);

    Ufixed<100, -50> wide_val(5.9375);
    EXPECT_EQ(static_cast<int>(wide_val), 5);
    Ufixed<100, -50> wide_huge(1);
    wide_huge <<= 65;
    EXPECT_THROW((void)static_cast<unsigned long long>(wide_huge), std::out_of_range);
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

    Ufixed<100, -50> wide_float(5.0625);
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_float), 5.0625);
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
    auto val_ba = as<Ufixed<3, -4>>(bits);
    EXPECT_DOUBLE_EQ(static_cast<double>(val_ba), 8.0);

    Sfixed<3, -4> negative_val(-5.0);
    Sfixed<3, -4> positive_val(5.0);
    EXPECT_THROW((Ufixed<3, -4>(negative_val)), std::out_of_range);

    Ufixed<3, -4> pos_uf(positive_val);
    EXPECT_DOUBLE_EQ(static_cast<double>(pos_uf), 5.0);

    Ufixed<7, 0> wide(200);
    EXPECT_THROW((Ufixed<3, 0>(wide)), std::out_of_range);
    Ufixed<3, 0> narrow_saturated = resize<3, 0>(wide);
    Ufixed<3, 0> narrow_wrapped =
        resize<3, 0>(wide, overflow_mode::wrap, round_mode::round_to_even);

    EXPECT_EQ(static_cast<int>(narrow_saturated), 15);
    EXPECT_EQ(static_cast<int>(narrow_wrapped), 8);

    Ufixed<7, -4> fractional(5.5);
    EXPECT_THROW((Ufixed<3, 0>(fractional)), std::out_of_range);
    EXPECT_EQ(static_cast<int>(Ufixed<3, 0>(Ufixed<7, -4>(5.0))), 5);

    Ufixed<100, -50> wide_from_double(100.5);
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_from_double), 100.5);
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

    Ufixed<100, -50> wide_u(15);
    auto wide_s = as<Sfixed<100, -50>>(wide_u);
    EXPECT_EQ(static_cast<int>(wide_s), 15);
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

    Ufixed<100, -50> wide_orig_res(10.5);
    auto narrow_from_wide = resize<7, -4>(wide_orig_res);
    EXPECT_DOUBLE_EQ(static_cast<double>(narrow_from_wide), 10.5);
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

    Ufixed<100, -50> wide_shift(1.0);
    wide_shift <<= 2;
    EXPECT_DOUBLE_EQ(static_cast<double>(wide_shift), 4.0);
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

    Ufixed<100, -50> w_cmp_a(3.25), w_cmp_b(3.25), w_cmp_c(4.5);
    EXPECT_TRUE(w_cmp_a == w_cmp_b);
    EXPECT_TRUE(w_cmp_a < w_cmp_c);
}

TEST(TestUfixed, Indexing) {
    auto ba = "100110"_b;
    auto uf = as<Ufixed<4, -1>>(ba);

    EXPECT_TRUE(uf[4] && uf[1] && uf[0]);
    EXPECT_FALSE(uf[-1] || uf[3] || uf[2]);

    EXPECT_THROW(uf[6], std::out_of_range);
    EXPECT_THROW(uf[7], std::out_of_range);

    Ufixed<100, -50> w_idx(1.0);
    EXPECT_FALSE(w_idx[100]);
    EXPECT_FALSE(w_idx[-50]);
    EXPECT_TRUE(w_idx[0]);

    uf[4] = Bit::_0;
    uf[-1] = Bit::_1;
    EXPECT_FALSE(static_cast<bool>(uf[4]));
    EXPECT_TRUE(static_cast<bool>(uf[-1]));

    auto const& const_uf = uf;
    EXPECT_FALSE(static_cast<bool>(const_uf[4]));
    EXPECT_TRUE(static_cast<bool>(const_uf[-1]));
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

    Ufixed<100, -50> w_iter(0);
    EXPECT_EQ(std::distance(w_iter.begin(), w_iter.end()), 151);
}

TEST(TestUfixed, ResizeRoundingModes) {
    Ufixed<10, -10> value(9.5);

    EXPECT_EQ(
        (resize<10, 0>(value, overflow_mode::saturate, round_mode::truncate)),
        (Ufixed<10, 0>(9))
    );
    EXPECT_EQ(
        (resize<10, 0>(value, overflow_mode::saturate, round_mode::round_to_zero)),
        (Ufixed<10, 0>(9))
    );
    EXPECT_EQ(
        (resize<10, 0>(value, overflow_mode::saturate, round_mode::round_to_pos)),
        (Ufixed<10, 0>(10))
    );
    EXPECT_EQ(
        (resize<10, 0>(value, overflow_mode::saturate, round_mode::round)),
        (Ufixed<10, 0>(10))
    );
    EXPECT_EQ(
        (resize<10, 0>(value, overflow_mode::saturate, round_mode::round_to_even)),
        (Ufixed<10, 0>(10))
    );

    Ufixed<-1, -4> subnormal(0.75);
    EXPECT_EQ(
        (resize<0, 0>(subnormal, overflow_mode::saturate, round_mode::round_to_pos)),
        (Ufixed<0, 0>(1))
    );

    Ufixed<10, 3> supernormal(16);
    EXPECT_EQ(
        (resize<10, 0>(supernormal, overflow_mode::saturate, round_mode::round)),
        (Ufixed<10, 0>(16))
    );
}

TEST(TestUfixed, Reverse) {
    auto ba = "10010110"_b;
    auto ba_r = "01101001"_b;

    auto uf_down = as<Ufixed<3, -4>>(ba);
    auto uf_to = as<Ufixed<-4, Direction::TO, 3>>(ba);

    auto r_to = reverse(uf_down);  // only direction changed
    auto r_down = reverse(uf_to);  // bits also reversed

    EXPECT_EQ(r_to, uf_to);
    EXPECT_EQ(r_down, (as<Ufixed<3, -4>>(ba_r)));

    Ufixed<100, -50> w_rev_down(1);
    auto w_rev_to = reverse(w_rev_down);
    EXPECT_TRUE((std::is_same_v<decltype(w_rev_to), Ufixed<-50, Direction::TO, 100>>));
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

    Ufixed<8, -3> val_downto(120);
    auto val_to = reverse(val_downto);
    EXPECT_THROW(
        (void)std::vformat("{:d}", std::make_format_args(val_to)), std::format_error
    );

    EXPECT_THROW(
        auto s = std::vformat("{:x}", std::make_format_args(val)), std::format_error
    );
    EXPECT_THROW(
        auto s = std::vformat("{:o}", std::make_format_args(val)), std::format_error
    );
    EXPECT_THROW(
        auto s = std::vformat("{:e}", std::make_format_args(val)), std::format_error
    );

    Ufixed<100, -50> w_fmt(5.0625);
    EXPECT_EQ(
        std::format("{:d}", w_fmt),
        "Ufixed[100 downto -50]{5.06250000000000000000000000000000000000000000000000}"
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

    Ufixed<100, -50> w_hash_a(10), w_hash_b(10), w_hash_c(11);
    EXPECT_EQ(
        (std::hash<Ufixed<100, -50>>{}(w_hash_a)), (std::hash<Ufixed<100, -50>>{}(w_hash_b))
    );
    EXPECT_NE(
        (std::hash<Ufixed<100, -50>>{}(w_hash_a)), (std::hash<Ufixed<100, -50>>{}(w_hash_c))
    );
}

TEST(TestUfixed, BitwiseAndReduction) {
    Ufixed<3, 0> a(10);  // 1010
    Ufixed<3, 0> b(12);  // 1100

    auto and_res = a & b;
    EXPECT_TRUE((
        std::is_same_v<decltype(and_res), BitArray<Range{3, Direction::DOWNTO, 0}>>
    ));

    EXPECT_EQ((as<Ufixed<3, 0>>(and_res)), (Ufixed<3, 0>(8)));  // 1000

    auto or_res = a | b;
    EXPECT_EQ((as<Ufixed<3, 0>>(or_res)), (Ufixed<3, 0>(14)));  // 1110

    auto xor_res = a ^ b;
    EXPECT_EQ((as<Ufixed<3, 0>>(xor_res)), (Ufixed<3, 0>(6)));  // 0110

    auto not_res = ~a;
    EXPECT_EQ((as<Ufixed<3, 0>>(not_res)), (Ufixed<3, 0>(5)));  // 0101

    EXPECT_FALSE(and_reduce(a));
    EXPECT_TRUE(or_reduce(a));
    EXPECT_FALSE(xor_reduce(a));

    Ufixed<100, -50> w_bw_a(10), w_bw_b(12);
    EXPECT_EQ((as<Ufixed<100, -50>>(w_bw_a & w_bw_b)), (Ufixed<100, -50>(8)));
}

TEST(TestUfixed, Concatenation) {
    Ufixed<3, 0> a(10);     // 1010
    Ufixed<1, -2> b(3.75);  // 1111 (1*2 + 1*1 + 1*0.5 + 1*0.25)

    auto cat_res = concat(a, b);
    EXPECT_TRUE((std::is_same_v<decltype(cat_res), BitArray<8>>));

    // 1010 concatenated with 1111 -> 10101111 = 175
    EXPECT_EQ(static_cast<int>(as<Ufixed<7, 0>>(cat_res)), 175);

    Ufixed<100, 0> w_cat_a(10);
    Ufixed<30, 0> w_cat_b(3);
    auto w_cat_res = concat(w_cat_a, w_cat_b);
    EXPECT_TRUE((std::is_same_v<decltype(w_cat_res), BitArray<132>>));
}

TEST(TestUfixed, SubtypeRoundTrip) {
    static_assert(
        !std::is_constructible_v<Ufixed<3, -4>, BitArray<Range{3, Direction::DOWNTO, -4}>>
    );

    Ufixed<3, -4> s(5.0625);

    BitArray<Range{3, Direction::DOWNTO, -4}> ba = s;
    auto restored = as<Ufixed<3, -4>>(ba);

    EXPECT_EQ(s, restored);
    EXPECT_TRUE((s == as<Ufixed<3, -4>>(BitArray<Range{3, Direction::DOWNTO, -4}>(s))));

    Ufixed<100, -50> w_rt(5.0625);
    BitArray<Range{100, Direction::DOWNTO, -50}> w_ba = w_rt;
    EXPECT_EQ(w_rt, (as<Ufixed<100, -50>>(w_ba)));
}

TEST(TestUfixed, InfinityWrapThrows) {
    EXPECT_THROW(
        (Ufixed<3, -4>(std::numeric_limits<double>::infinity(), overflow_mode::wrap)),
        std::domain_error
    );
    EXPECT_THROW(
        (Ufixed<3, -4>(-std::numeric_limits<double>::infinity(), overflow_mode::wrap)),
        std::domain_error
    );

    EXPECT_THROW(
        (Ufixed<100, -50>(std::numeric_limits<double>::infinity(), overflow_mode::wrap)),
        std::domain_error
    );
}

TEST(TestUfixed, MinMax) {
    Ufixed<5, -2> a(3.25);
    Ufixed<5, -2> b(4.5);

    EXPECT_EQ(std::min(a, b), a);
    EXPECT_EQ(std::max(a, b), b);

    Ufixed<100, -50> w_minmax_a(3.25), w_minmax_b(4.5);
    EXPECT_EQ(std::min(w_minmax_a, w_minmax_b), w_minmax_a);
}

TEST(TestUfixed, StrictTODirectionRejection) {
    using ToType = Ufixed<-2, Direction::TO, 6>;
    using DowntoType = Ufixed<6, -2>;

    static_assert(requires(DowntoType a, DowntoType b) { a == b; });
    static_assert(requires(DowntoType a) { static_cast<double>(a); });
    static_assert(requires(ToType a) { std::format("{:b}", a); });
    static_assert(requires(ToType a) {
        static_cast<BitArray<Range{-2, Direction::TO, 6}>>(a);
    });

    using WideToType = Ufixed<-50, Direction::TO, 100>;
    static_assert(requires(WideToType a) {
        static_cast<BitArray<Range{-50, Direction::TO, 100}>>(a);
    });
}

TEST(TestUfixed, ImplicitCrossKindWidening) {
    Ufixed<3, -4> u(5.0625);

    Sfixed<4, -4> s = u;
    EXPECT_DOUBLE_EQ(static_cast<double>(s), 5.0625);

    static_assert(!std::is_convertible_v<Ufixed<3, -4>, Sfixed<3, -4>>);

    Ufixed<100, -50> w_u(5.0625);
    Sfixed<101, -50> w_s = w_u;
    EXPECT_DOUBLE_EQ(static_cast<double>(w_s), 5.0625);
}

TEST(TestUfixed, UnaryOperators) {
    Ufixed<3, -4> u_val(5.25);
    auto u_neg = -u_val;
    EXPECT_TRUE((std::is_same_v<decltype(u_neg), Sfixed<4, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(u_neg), -5.25);

    Ufixed<3, 0> u_max(15);
    auto u_max_neg = -u_max;
    EXPECT_TRUE((std::is_same_v<decltype(u_max_neg), Sfixed<4, 0>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(u_max_neg), -15.0);

    auto u_pos = +u_val;
    EXPECT_TRUE((std::is_same_v<decltype(u_pos), Sfixed<4, -4>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(u_pos), 5.25);

    Ufixed<100, -50> w_un_val(5.25);
    auto w_un_neg = -w_un_val;
    EXPECT_TRUE((std::is_same_v<decltype(w_un_neg), Sfixed<101, -50>>));
}

TEST(TestUfixed, Arithmetic) {
    Ufixed<5, -6> a(19.25);
    Ufixed<10, -10> b(5.55);  // Actually stored as ~5.5498

    auto c = a + b;
    EXPECT_EQ(c, (Ufixed<11, -10>(24.80)));

    auto g = b + a;
    EXPECT_EQ(g, (Ufixed<11, -10>(24.80)));

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

    Ufixed<100, -50> w_arith_a(19.25), w_arith_b(5.5);
    auto w_arith_c = w_arith_a + w_arith_b;
    EXPECT_TRUE((std::is_same_v<decltype(w_arith_c), Ufixed<101, -50>>));
    EXPECT_DOUBLE_EQ(static_cast<double>(w_arith_c), 24.75);
}

TEST(TestUfixed, DivisionFamily) {
    static_assert(fixed_guard_bits == 3);

    Ufixed<1, 0> two(2);
    Ufixed<1, 0> three(3);

    auto quotient = two / three;
    static_assert(std::is_same_v<decltype(quotient), Ufixed<1, -2>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(quotient), 0.75);
    EXPECT_EQ(quotient, divide(two, three));

    // With one guard bit, 2/3 looks like an exact tie whose retained LSB is
    // even. Three guard bits expose another nonzero bit and round upward.
    EXPECT_DOUBLE_EQ(
        static_cast<double>(divide(two, three, round_mode::round_to_even, 1)), 0.5
    );
    EXPECT_DOUBLE_EQ(
        static_cast<double>(divide(two, three, round_mode::round_to_even, 3)), 0.75
    );
    EXPECT_DOUBLE_EQ(
        static_cast<double>(divide(two, three, round_mode::round_to_zero, 3)), 0.5
    );

    Ufixed<2, 0> five(5);
    auto remainder_result = remainder(five, three);
    auto modulo_result = modulo(five, three);
    static_assert(std::is_same_v<decltype(remainder_result), Ufixed<1, 0>>);
    static_assert(std::is_same_v<decltype(modulo_result), Ufixed<1, 0>>);
    EXPECT_EQ(static_cast<unsigned>(remainder_result), 2U);
    EXPECT_EQ(rem(five, three), remainder_result);
    EXPECT_EQ(modulo_result, remainder_result);
    EXPECT_EQ(mod(five, three), modulo_result);

    auto [divrem_quotient, divrem_remainder] = divrem(five, three);
    EXPECT_EQ(divrem_quotient, divide(five, three));
    EXPECT_EQ(divrem_remainder, remainder_result);
    auto [divmod_quotient, divmod_modulo] = divmod(five, three);
    EXPECT_EQ(divmod_quotient, divide(five, three));
    EXPECT_EQ(divmod_modulo, modulo_result);

    // The combined operation aligns unlike binary-point locations before its
    // one magnitude division, so its remainder is expressed exactly at the
    // finer operand resolution.
    Ufixed<3, -2> fractional_dividend(5.25);
    Ufixed<2, -1> fractional_divisor(2.5);
    auto [fractional_quotient, fractional_remainder] =
        divrem(fractional_dividend, fractional_divisor);
    static_assert(std::is_same_v<decltype(fractional_quotient), Ufixed<4, -5>>);
    static_assert(std::is_same_v<decltype(fractional_remainder), Ufixed<2, -2>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(fractional_quotient), 2.09375);
    EXPECT_DOUBLE_EQ(static_cast<double>(fractional_remainder), 0.25);

    // Guard positions can be integral when the quotient range ends above zero.
    Ufixed<5, 4> coarse_dividend(32);
    Ufixed<0, -1> one_and_a_half(1.5);
    static_assert(std::is_same_v<decltype(coarse_dividend / one_and_a_half), Ufixed<6, 3>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(coarse_dividend / one_and_a_half), 24.0);
    EXPECT_DOUBLE_EQ(
        static_cast<double>(
            divide(coarse_dividend, one_and_a_half, round_mode::round_to_even, 1)
        ),
        16.0
    );
    EXPECT_DOUBLE_EQ(static_cast<double>(remainder(coarse_dividend, one_and_a_half)), 0.5);

    constexpr Range NullRange{-1, Direction::DOWNTO, 0};
    auto [zero_quotient, zero_remainder] = divrem(Ufixed<NullRange>{}, three);
    EXPECT_FALSE(static_cast<bool>(zero_quotient));
    EXPECT_FALSE(static_cast<bool>(zero_remainder));
    EXPECT_THROW((void)divide(three, Ufixed<NullRange>{}), std::domain_error);

    auto reciprocal_result = reciprocal(three);
    static_assert(std::is_same_v<decltype(reciprocal_result), Ufixed<0, -2>>);
    EXPECT_DOUBLE_EQ(static_cast<double>(reciprocal_result), 0.25);

    EXPECT_THROW((void)divide(two, Ufixed<1, 0>(0)), std::domain_error);
    EXPECT_THROW((void)remainder(two, Ufixed<1, 0>(0)), std::domain_error);
    EXPECT_THROW((void)modulo(two, Ufixed<1, 0>(0)), std::domain_error);
}

TEST(TestUfixed, CompoundArithmetic) {
    Ufixed<5, -6> a(19.25);
    Ufixed<10, -10> b(5.55);

    Ufixed<11, -10> c(a);
    c += b;
    EXPECT_EQ(c, (Ufixed<11, -10>(24.80)));

    Ufixed<10, -10> z(5.55);
    EXPECT_THROW((z -= a), std::out_of_range);

    Ufixed<11, -10> d(a);
    d -= b;
    EXPECT_EQ(d, (Ufixed<11, -10>(13.7)));

    Ufixed<16, -16> f(a);
    f *= b;
    EXPECT_NEAR(static_cast<double>(f), 106.83374, 1e-4);

    Ufixed<15, -10> div_res(a);
    div_res /= b;
    EXPECT_NEAR(static_cast<double>(div_res), 3.4677734375, 1e-6);

    Ufixed<15, -10> div_res_r(b);
    div_res_r /= a;
    EXPECT_NEAR(static_cast<double>(div_res_r), 0.2880859375, 1e-6);

    Ufixed<10, -10> mod_res(a);
    mod_res %= b;
    EXPECT_NEAR(static_cast<double>(mod_res), 2.60058, 1e-4);

    Ufixed<10, -10> mod_res_r(b);
    mod_res_r %= a;
    EXPECT_NEAR(static_cast<double>(mod_res_r), 5.54980, 1e-4);

    Ufixed<100, -50> w_comp_a(19.25), w_comp_b(5.5);
    w_comp_a += w_comp_b;
    EXPECT_DOUBLE_EQ(static_cast<double>(w_comp_a), 24.75);
}

TEST(TestUfixed, CompoundArithmeticNativeInt) {
    Ufixed<11, -10> val(5.25);

    val += 2;
    EXPECT_EQ(val, (Ufixed<11, -10>(7.25)));

    val -= 1;
    EXPECT_EQ(val, (Ufixed<11, -10>(6.25)));

    val *= 3;
    EXPECT_EQ(val, (Ufixed<11, -10>(18.75)));

    val /= 2;
    EXPECT_EQ(val, (Ufixed<11, -10>(9.375)));

    val %= 4;
    EXPECT_EQ(val, (Ufixed<11, -10>(1.375)));

    EXPECT_THROW((val -= 5), std::out_of_range);

    Ufixed<100, -50> w_cni(5.25);
    w_cni += 2;
    EXPECT_DOUBLE_EQ(static_cast<double>(w_cni), 7.25);
}

TEST(TestUfixed, IncrementDecrement) {
    Ufixed<11, -10> val(5.25);

    auto pre_inc = ++val;
    EXPECT_EQ(pre_inc, (Ufixed<11, -10>(6.25)));
    EXPECT_EQ(val, (Ufixed<11, -10>(6.25)));

    auto post_inc = val++;
    EXPECT_EQ(post_inc, (Ufixed<11, -10>(6.25)));
    EXPECT_EQ(val, (Ufixed<11, -10>(7.25)));

    auto pre_dec = --val;
    EXPECT_EQ(pre_dec, (Ufixed<11, -10>(6.25)));
    EXPECT_EQ(val, (Ufixed<11, -10>(6.25)));

    auto post_dec = val--;
    EXPECT_EQ(post_dec, (Ufixed<11, -10>(6.25)));
    EXPECT_EQ(val, (Ufixed<11, -10>(5.25)));

    Ufixed<11, -10> small_val(0.75);
    EXPECT_THROW(--small_val, std::out_of_range);
    EXPECT_THROW(small_val--, std::out_of_range);

    Ufixed<100, -50> w_inc(5.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(++w_inc), 6.25);
}

TEST(TestUfixed, ImplicitUnsignedArithmetic) {
    Ufixed<5, -6> a(19.25);
    Unsigned<7, 0> b(15);

    auto c = a + b;
    EXPECT_EQ(c, (Ufixed<8, -6>(34.25)));

    auto g = b + a;
    EXPECT_EQ(g, (Ufixed<8, -6>(34.25)));

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

    Ufixed<100, -50> w_is_a(19.25);
    Unsigned<100> w_is_b(15);
    EXPECT_DOUBLE_EQ(static_cast<double>(w_is_a + w_is_b), 34.25);
}

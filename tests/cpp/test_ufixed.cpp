// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>

using namespace coconext::types;

TEST(TestUfixed, shape_and_typelevel) {
    Ufixed<6, -2> a(37);
    Ufixed<-2, Direction::TO, 6> b(37);

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

    // TODO
    // Sfixed<3, -4> negative_val(-5.0);
    // Sfixed<3, -4> positive_val(5.0);

    // // Negative value should throw an out_of_range exception or clamp to 0 based on
    // saturate/wrap mode
    // // Default mode is saturate, which throws on negative values out of Ufixed range
    // EXPECT_THROW(Ufixed<3, -4>(negative_val), std::out_of_range);

    // // Positive fits perfectly
    // EXPECT_NO_THROW({
    //     Ufixed<3, -4> pos_uf(positive_val);
    //     EXPECT_DOUBLE_EQ(static_cast<double>(pos_uf), 5.0);
    // });

    Ufixed<7, 0> wide(200);
    Ufixed<3, 0> narrow_saturated(wide);
    Ufixed<3, 0> narrow_wrapped(wide, overflow_mode::wrap, round_mode::round_to_even);

    // TODO
    // EXPECT_EQ(static_cast<int>(narrow_saturated), 15);
    // EXPECT_EQ(static_cast<int>(narrow_wrapped), 8);
}

TEST(TestUfixed, as_overloads) {
    // TODO
}

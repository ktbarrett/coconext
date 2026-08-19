// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>

using namespace coconext::types;

TEST(TestSfixed, shape_and_typelevel) {
    Sfixed<6, -2> a(37);
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
}

TEST(TestSfixed, resize) {
    Sfixed<3, 0> s_orig(-5);
    auto s_wide = resize<7, -4>(s_orig);
    EXPECT_DOUBLE_EQ(static_cast<double>(s_wide), -5.0);

    Sfixed<3, -4> src_f(3.5);
    auto res = resize<Range{2, Direction::DOWNTO, -1}>(
        src_f, overflow_mode::saturate, round_mode::truncate
    );
    EXPECT_DOUBLE_EQ(static_cast<double>(res), 3.5);

    Sfixed<7, 0> big_neg(-100);
    auto sat_res_neg = resize<3, 0>(big_neg, overflow_mode::saturate);
    EXPECT_EQ(static_cast<int>(sat_res_neg), -8);

    Sfixed<3, -4> val(2.75);
    auto t_trunc = resize<3, 0>(val, overflow_mode::saturate, round_mode::truncate);
    auto t_rtz = resize<3, 0>(val, overflow_mode::saturate, round_mode::round_to_zero);
    auto t_rtp = resize<3, 0>(val, overflow_mode::saturate, round_mode::round_to_pos);
    auto t_rnd = resize<3, 0>(val, overflow_mode::saturate, round_mode::round);
    EXPECT_DOUBLE_EQ(static_cast<double>(t_trunc), 2.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(t_rtz), 2.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(t_rtp), 3.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(t_rnd), 3.0);

    Sfixed<3, -4> val_nf(-2.75);
    auto n_trunc = resize<3, 0>(val_nf, overflow_mode::saturate, round_mode::truncate);
    auto n_rtz = resize<3, 0>(val_nf, overflow_mode::saturate, round_mode::round_to_zero);
    EXPECT_DOUBLE_EQ(static_cast<double>(n_trunc), -3.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(n_rtz), -2.0);

    Sfixed<3, -4> edge(7.5);
    auto edge_sat = resize<3, 0>(edge, overflow_mode::saturate, round_mode::round_to_pos);
    EXPECT_EQ(static_cast<int>(edge_sat), 7);
    auto edge_wrap = resize<3, 0>(edge, overflow_mode::wrap, round_mode::round_to_pos);
    EXPECT_EQ(static_cast<int>(edge_wrap), -8);
}

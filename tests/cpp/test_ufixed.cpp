// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>

using namespace coconext::types;

TEST(TestUfixed, Construction) {
    // test make fixed range() with 1 and 2 args

    // test default constructor
    // test native int constructor
    // test Unsigned constructor
}

TEST(TestUfixed, shape_and_typelevel) {
    Ufixed<6, -2> a(37);
    Ufixed<-2, Direction::TO, 6> b(37);

    static_assert(a.static_range == Range(6, Direction::DOWNTO, -2));
    static_assert(b.static_range == Range(-2, Direction::TO, 6));

    static_assert(is_fixed<decltype(a)>);
    static_assert(is_fixed<decltype(b)>);

    static_assert(a.resolution() == 0.25);
    static_assert(b.resolution() == 0.25);

    static_assert(a.int_bits() == 7);
    static_assert(b.int_bits() == 7);

    static_assert(a.frac_bits() == 2);
    static_assert(b.frac_bits() == 2);
}

TEST(TestUfixed, implicit_conversions) {
    // test bit array implicit upcast
}

TEST(TestUfixed, Formatter) {
    // Ufixed<3, -4> a{5.0625};
    // EXPECT_EQ(std::format("{:b}", a), "Ufixed[3 downto -4]{"0101.0001"}");
    // EXPECT_EQ(std::format("{:b}", a), "Ufixed[3 downto -4]{"5.0625"}");
}

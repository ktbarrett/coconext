// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>

using namespace coconext::types;

TEST(TestSfixed, shape_and_typelevel) {
    Sfixed<6, -2> a(37);
    Sfixed<-2, Direction::TO, 6> b(37);

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

// Includes exactly one coconext header, to check that the header is complete by itself.

#include <gtest/gtest.h>

#include <coconext/types/dyn_ufixed.hpp>

using namespace coconext::types;

TEST(Includes, DynUfixed) {
    Ufixed<3, -4> f{1.5};
    Sfixed<3, -4> g{-0.5};
    DynUfixed df(f, f.range());
    DynSfixed dg(g, g.range());
    // DynUfixed operations that return DynSfixed.
    EXPECT_EQ(df - df, DynSfixed(0.0, Range{4, Direction::DOWNTO, -4}));
    EXPECT_EQ(df + dg, DynSfixed(1.0, Range{5, Direction::DOWNTO, -4}));
    // Operations between the static and the dynamic types.
    EXPECT_EQ(df + g, DynSfixed(1.0, Range{5, Direction::DOWNTO, -4}));
    EXPECT_EQ((Ufixed<3, -4>(df)), f);
    EXPECT_EQ((Sfixed<3, -4>(dg)), g);
    // Operations with the dynamic integer types.
    EXPECT_EQ(df + DynUnsigned(2, 4), DynUfixed(3.5, Range{4, Direction::DOWNTO, -4}));
}

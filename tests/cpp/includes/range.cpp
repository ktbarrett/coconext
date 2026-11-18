// Includes exactly one coconext header, to check that the header is complete by itself.

#include <gtest/gtest.h>

#include <coconext/types/range.hpp>

using namespace coconext::types;

TEST(Includes, Range) {
    Range r{7, Direction::DOWNTO, 0};
    EXPECT_EQ(r.length(), 8U);
    EXPECT_EQ(r.reverse(), (Range{0, Direction::TO, 7}));
}

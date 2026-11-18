// Includes exactly one coconext header, to check that the header is complete by itself.

#include <gtest/gtest.h>

#include <coconext/types/direction.hpp>

using namespace coconext::types;

TEST(Includes, Direction) { EXPECT_EQ(to_string(Direction::DOWNTO), "downto"); }

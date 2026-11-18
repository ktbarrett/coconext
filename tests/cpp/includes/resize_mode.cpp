// Includes exactly one coconext header, to check that the header is complete by itself.

#include <gtest/gtest.h>

#include <coconext/types/resize_mode.hpp>

using namespace coconext::types;

TEST(Includes, ResizeMode) {
    EXPECT_NE(overflow_mode::wrap, overflow_mode::saturate);
    EXPECT_NE(round_mode::truncate, round_mode::round);
}

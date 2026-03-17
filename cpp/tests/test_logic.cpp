#include <gtest/gtest.h>

#include <coconext/types.hpp>

using namespace coconext::types;

// Demonstrate some basic assertions.
TEST(TestLogic, LogicConstruction) {
    EXPECT_NO_THROW(Logic{});
    EXPECT_NO_THROW(Logic{Logic::X});
    EXPECT_EQ(Logic{}, Logic::_0);
}

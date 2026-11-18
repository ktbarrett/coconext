// Includes exactly one coconext header, to check that the header is complete by itself.

#include <format>

#include <gtest/gtest.h>

#include <coconext/types/logic.hpp>

using namespace coconext::types;

TEST(Includes, Logic) {
    Bit b{'1'};
    Logic l = b;
    EXPECT_EQ(l.resolve(), b);
    EXPECT_EQ(Bit{l}, b);
    EXPECT_EQ(std::format("{} {}", l, b), "Logic{1} Bit{1}");
}

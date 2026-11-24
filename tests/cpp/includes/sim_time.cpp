// Includes exactly one coconext header, to check that the header is complete by itself.

#include <gtest/gtest.h>

#include <coconext/types/sim_time.hpp>

using namespace coconext::types;
using namespace coconext::literals;

TEST(Includes, SimTime) {
    SimTime st{5_ps};
    EXPECT_EQ(st, 5_ps);
    EXPECT_EQ(1_ps, 1000_fs);
}

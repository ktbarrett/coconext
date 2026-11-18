// Includes exactly one coconext header, to check that the header is complete by itself.

#include <format>

#include <gtest/gtest.h>

#include <coconext/types/dyn_unsigned.hpp>

using namespace coconext::types;

TEST(Includes, DynUnsigned) {
    DynUnsigned u(200, 8);
    DynSigned s(-3, 8);
    // DynUnsigned operations that return DynSigned.
    EXPECT_EQ(-u, DynSigned(-200, 9));
    EXPECT_EQ(u - u, DynSigned(0, 9));
    s += DynUnsigned(2, 8);
    EXPECT_EQ(s, DynSigned(-1, 8));
    EXPECT_EQ(std::format("{}", *u.resolve()), "BitVector[7 downto 0]{\"11001000\"}");
}

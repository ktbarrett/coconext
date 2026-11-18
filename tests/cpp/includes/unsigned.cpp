// Includes exactly one coconext header, to check that the header is complete by itself.

#include <format>

#include <gtest/gtest.h>

#include <coconext/types/unsigned.hpp>

using namespace coconext::types;

TEST(Includes, Unsigned) {
    Unsigned<8> u{200};
    Signed<8> s{-3};
    // Unsigned operations that return Signed.
    EXPECT_EQ(std::format("{}", -u), "Signed[8 downto 0]{-200}");
    EXPECT_EQ(std::format("{}", u - u), "Signed[8 downto 0]{0}");
    EXPECT_EQ(std::format("{}", u + s), "Signed[9 downto 0]{197}");
    EXPECT_EQ(Unsigned<8>(Signed<9>(u)), u);
    BitArray<8> bits = u;
    EXPECT_EQ(std::format("{}", bits), "BitArray[7 downto 0]{\"11001000\"}");
    EXPECT_EQ(u.resolve(), bits);
    EXPECT_EQ(std::format("{}", u & u), "BitArray[7 downto 0]{\"11001000\"}");
}

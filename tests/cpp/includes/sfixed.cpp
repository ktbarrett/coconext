// Includes exactly one coconext header, to check that the header is complete by itself.

#include <format>

#include <gtest/gtest.h>

#include <coconext/types/sfixed.hpp>

using namespace coconext::types;

TEST(Includes, Sfixed) {
    Ufixed<3, -4> f{1.5};
    Sfixed<3, -4> g{-0.5};
    // Ufixed operations that return Sfixed.
    EXPECT_EQ(std::format("{}", f - f), "Sfixed[4 downto -4]{0.0000}");
    EXPECT_EQ(std::format("{}", -f), "Sfixed[4 downto -4]{-1.5000}");
    EXPECT_EQ(std::format("{}", f + g), "Sfixed[5 downto -4]{1.0000}");
    // Operations with the integer types.
    f += Signed<4>{1};
    EXPECT_EQ(std::format("{}", f), "Ufixed[3 downto -4]{2.5000}");
    EXPECT_EQ(std::format("{}", f + Unsigned<4>{2}), "Ufixed[4 downto -4]{4.5000}");
    EXPECT_EQ(Unsigned<4>(f), Unsigned<4>{2});
}

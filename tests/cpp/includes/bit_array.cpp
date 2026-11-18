// Includes exactly one coconext header, to check that the header is complete by itself.

#include <format>
#include <functional>

#include <gtest/gtest.h>

#include <coconext/types/bit_array.hpp>

using namespace coconext::types;

TEST(Includes, BitArray) {
    BitArray<8> a{0xA5};
    BitVector v(8);
    v[0] = Bit{'1'};
    EXPECT_EQ(a, a);
    EXPECT_EQ(v, v);
    EXPECT_EQ(std::format("{}", a & a), "BitArray[7 downto 0]{\"10100101\"}");
    EXPECT_EQ(std::format("{}", a | v), "BitVector[7 downto 0]{\"10100101\"}");
    EXPECT_EQ(
        std::format("{}", concat(a, v)), "BitVector[15 downto 0]{\"1010010100000001\"}"
    );
    // A slice of one container reverses into the other container.
    EXPECT_EQ(
        std::format("{}", a[Range{3, Direction::DOWNTO, 0}].reverse()),
        "BitVector[0 to 3]{\"1010\"}"
    );
    EXPECT_EQ(
        std::format("{}", (v.slice<Range{3, Direction::DOWNTO, 0}>().reverse())),
        "BitArray[0 to 3]{\"1000\"}"
    );
    EXPECT_EQ(a.resolve(), a);
    EXPECT_EQ(std::hash<BitArray<8>>{}(a), std::hash<BitArray<8>>{}(a));
    EXPECT_EQ(std::hash<BitVector>{}(v), std::hash<BitVector>{}(v));
}

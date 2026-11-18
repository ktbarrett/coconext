// Includes exactly one coconext header, to check that the header is complete by itself.

#include <format>
#include <functional>

#include <gtest/gtest.h>

#include <coconext/types/logic_vector.hpp>

using namespace coconext::types;

TEST(Includes, LogicVector) {
    LogicArray<4> a("1010");
    LogicVector v("10XZ");
    EXPECT_EQ(a, a);
    EXPECT_EQ(v, v);
    // resolve() returns the Bit containers.
    EXPECT_EQ(std::format("{}", *a.resolve()), "BitArray[3 downto 0]{\"1010\"}");
    EXPECT_FALSE(v.resolve().has_value());
    EXPECT_EQ(
        std::format("{}", *LogicVector("1010").resolve()), "BitVector[3 downto 0]{\"1010\"}"
    );
    EXPECT_EQ(std::format("{}", a & v), "LogicVector[3 downto 0]{\"10X0\"}");
    EXPECT_EQ(std::format("{}", a & *a.resolve()), "LogicArray[3 downto 0]{\"1010\"}");
    // A slice of one container reverses into the other container.
    EXPECT_EQ(
        std::format("{}", a[Range{1, Direction::DOWNTO, 0}].reverse()),
        "LogicVector[0 to 1]{\"01\"}"
    );
    EXPECT_EQ(
        std::format("{}", (v.slice<Range{3, Direction::DOWNTO, 2}>().reverse())),
        "LogicArray[2 to 3]{\"01\"}"
    );
    EXPECT_EQ(std::hash<LogicArray<4>>{}(a), std::hash<LogicArray<4>>{}(a));
    EXPECT_EQ(std::hash<LogicVector>{}(v), std::hash<LogicVector>{}(v));
}

// Includes exactly one coconext header, to check that the header is complete by itself.

#include <format>
#include <functional>

#include <gtest/gtest.h>

#include <coconext/types/vector.hpp>

using namespace coconext::types;

TEST(Includes, Vector) {
    Array<int, 4> a{1, 2, 3, 4};
    Vector<int> v{
        {1, 2, 3, 4},
        Range(4)
    };
    EXPECT_EQ(a, a);
    EXPECT_EQ(v, v);
    // A slice of one container reverses into the other container.
    EXPECT_EQ(std::format("{}", a[Range{1, 2}].reverse()), "Vector[2 downto 1]{3, 2}");
    EXPECT_EQ(
        std::format("{}", (v.slice<Range{0, Direction::TO, 1}>().reverse())),
        "Array[1 downto 0]{2, 1}"
    );
    std::hash<Array<int, 4>> array_hash;
    EXPECT_EQ(array_hash(a), array_hash(a));
    std::hash<Vector<int>> vector_hash;
    EXPECT_EQ(vector_hash(v), vector_hash(v));
}

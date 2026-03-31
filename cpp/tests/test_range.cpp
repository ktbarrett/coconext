#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace coconext::types;

TEST(TestRange, ToRange) {
    const Range r(1, Direction::TO, 8);
    EXPECT_EQ(r.left(), 1);
    EXPECT_EQ(r.direction(), Direction::TO);
    EXPECT_EQ(r.right(), 8);
    EXPECT_EQ(r.length(), 8U);

    const std::vector<int32_t> forward(r.begin(), r.end());
    EXPECT_EQ(forward, (std::vector<int32_t>{1, 2, 3, 4, 5, 6, 7, 8}));

    const std::vector<int32_t> reverse(r.rbegin(), r.rend());
    EXPECT_EQ(reverse, (std::vector<int32_t>{8, 7, 6, 5, 4, 3, 2, 1}));

    EXPECT_EQ(r[0], 1);
    EXPECT_EQ(r[7], 8);
    EXPECT_THROW((void)r[8], std::out_of_range);

    EXPECT_EQ(r(3, 7), Range(4, Direction::TO, 7));
    EXPECT_NE(find(r, 8), r.end());
    EXPECT_EQ(find(r, 10), r.end());
}

TEST(TestRange, DowntoRange) {
    const Range r(4, Direction::DOWNTO, -3);
    EXPECT_EQ(r.left(), 4);
    EXPECT_EQ(r.direction(), Direction::DOWNTO);
    EXPECT_EQ(r.right(), -3);
    EXPECT_EQ(r.length(), 8U);

    const std::vector<int32_t> forward(r.begin(), r.end());
    EXPECT_EQ(forward, (std::vector<int32_t>{4, 3, 2, 1, 0, -1, -2, -3}));

    const std::vector<int32_t> reverse(r.rbegin(), r.rend());
    EXPECT_EQ(reverse, (std::vector<int32_t>{-3, -2, -1, 0, 1, 2, 3, 4}));

    EXPECT_EQ(r[0], 4);
    EXPECT_EQ(r[7], -3);
    EXPECT_THROW((void)r[8], std::out_of_range);

    EXPECT_EQ(r(3, 7), Range(1, Direction::DOWNTO, -2));
    EXPECT_NE(find(r, 0), r.end());
    EXPECT_EQ(find(r, 10), r.end());
}

TEST(TestRange, NullRange) {
    const Range r(1, Direction::DOWNTO, 4);
    EXPECT_EQ(r.left(), 1);
    EXPECT_EQ(r.direction(), Direction::DOWNTO);
    EXPECT_EQ(r.right(), 4);
    EXPECT_EQ(r.length(), 0U);

    const std::vector<int32_t> forward(r.begin(), r.end());
    EXPECT_TRUE(forward.empty());

    const std::vector<int32_t> reverse(r.rbegin(), r.rend());
    EXPECT_TRUE(reverse.empty());

    EXPECT_THROW((void)r[0], std::out_of_range);
    EXPECT_EQ(find(r, 2), r.end());
}

TEST(TestRange, BadArgumentsEquivalent) {
    EXPECT_THROW((void)to_direction("BAD DIRECTION"), std::invalid_argument);
    EXPECT_THROW((void)to_direction("nope"), std::invalid_argument);
}

TEST(TestRange, Equality) {
    EXPECT_EQ(Range(7, Direction::DOWNTO, -7), Range(7, Direction::DOWNTO, -7));
    EXPECT_NE(Range(7, Direction::DOWNTO, -7), Range(0, Direction::TO, 8));
    EXPECT_EQ(Range(1, Direction::TO, 0), Range(8, Direction::TO, -8));
}

TEST(TestRange, OtherConstructors) {
    EXPECT_EQ(Range(1, 8), Range(1, Direction::TO, 8));
    EXPECT_EQ(Range(3, -4), Range(3, Direction::DOWNTO, -4));
}

TEST(TestRange, ReprEquivalent) {
    const Range r(5, Direction::TO, 9);
    EXPECT_EQ(to_string(r), "Range(5, 'to', 9)");
}

TEST(TestRange, UppercaseDirection) {
    const Range r(1, to_direction("TO"), 8);
    EXPECT_EQ(r.direction(), Direction::TO);
}

TEST(TestRange, BadGetitemEquivalent) {
    const Range r(10, Direction::DOWNTO, 4);
    EXPECT_THROW((void)r(8, 4), std::invalid_argument);
}

TEST(TestRange, Copy) {
    const Range r(-2, Direction::TO, 1);
    const Range copy_constructed(r);
    const Range copied_assigned = r;

    EXPECT_EQ(r, copy_constructed);
    EXPECT_EQ(r, copied_assigned);
}

TEST(TestRange, RangeIsHashable) {
    std::hash<Range> h;
    const Range r1(1, Direction::TO, 8);
    const Range r2(4, Direction::DOWNTO, -3);
    EXPECT_EQ(h(r1), h(r1));
    EXPECT_EQ(h(r2), h(r2));
    const std::unordered_set<Range> same{Range(1, Direction::TO, 8),
                                         Range(1, Direction::TO, 8)};
    EXPECT_EQ(same.size(), 1U);

    const std::unordered_set<Range> different{Range(1, Direction::TO, 8),
                                              Range(8, Direction::DOWNTO, 1)};
    EXPECT_EQ(different.size(), 2U);
}

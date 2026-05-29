// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <format>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace coconext::types;

TEST(TestRange, ToRange) {
    Range const r(1, Direction::TO, 8);
    EXPECT_EQ(r.left, 1);
    EXPECT_EQ(r.direction, Direction::TO);
    EXPECT_EQ(r.right, 8);
    EXPECT_EQ(r.length(), 8U);

    std::vector<int32_t> const forward(r.begin(), r.end());
    EXPECT_EQ(forward, (std::vector<int32_t>{1, 2, 3, 4, 5, 6, 7, 8}));

    std::vector<int32_t> const reverse(r.rbegin(), r.rend());
    EXPECT_EQ(reverse, (std::vector<int32_t>{8, 7, 6, 5, 4, 3, 2, 1}));

    EXPECT_EQ(r[0], 1);
    EXPECT_EQ(r[7], 8);
    EXPECT_THROW((void)r[8], std::out_of_range);

    EXPECT_NE(find(r, 8), r.end());
    EXPECT_EQ(find(r, 10), r.end());
}

TEST(TestRange, DowntoRange) {
    Range const r(4, Direction::DOWNTO, -3);
    EXPECT_EQ(r.left, 4);
    EXPECT_EQ(r.direction, Direction::DOWNTO);
    EXPECT_EQ(r.right, -3);
    EXPECT_EQ(r.length(), 8U);

    std::vector<int32_t> const forward(r.begin(), r.end());
    EXPECT_EQ(forward, (std::vector<int32_t>{4, 3, 2, 1, 0, -1, -2, -3}));

    std::vector<int32_t> const reverse(r.rbegin(), r.rend());
    EXPECT_EQ(reverse, (std::vector<int32_t>{-3, -2, -1, 0, 1, 2, 3, 4}));

    EXPECT_EQ(r[0], 4);
    EXPECT_EQ(r[7], -3);
    EXPECT_THROW((void)r[8], std::out_of_range);

    EXPECT_NE(find(r, 0), r.end());
    EXPECT_EQ(find(r, 10), r.end());
}

TEST(TestRange, NullRange) {
    Range const r(1, Direction::DOWNTO, 4);
    EXPECT_EQ(r.left, 1);
    EXPECT_EQ(r.direction, Direction::DOWNTO);
    EXPECT_EQ(r.right, 4);
    EXPECT_EQ(r.length(), 0U);

    std::vector<int32_t> const forward(r.begin(), r.end());
    EXPECT_TRUE(forward.empty());

    std::vector<int32_t> const reverse(r.rbegin(), r.rend());
    EXPECT_TRUE(reverse.empty());

    EXPECT_THROW((void)r[0], std::out_of_range);
    EXPECT_EQ(find(r, 2), r.end());
}

TEST(TestRange, BadArgumentsEquivalent) {
    EXPECT_THROW((void)to_direction("BAD DIRECTION"), std::invalid_argument);
    EXPECT_THROW((void)to_direction("nope"), std::invalid_argument);
    EXPECT_THROW((void)to_direction("xx"), std::invalid_argument);
    EXPECT_THROW((void)to_direction("nottto"), std::invalid_argument);
}

TEST(TestRange, Equality) {
    EXPECT_EQ(Range(7, Direction::DOWNTO, -7), Range(7, Direction::DOWNTO, -7));
    EXPECT_NE(Range(7, Direction::DOWNTO, -7), Range(0, Direction::TO, 8));
    EXPECT_EQ(Range(1, Direction::TO, 0), Range(8, Direction::TO, -8));
}

TEST(TestRange, EqualitySingleElementIgnoresDirection) {
    // A single-element range contains the same numbers regardless of
    // direction, so 3 TO 3 and 3 DOWNTO 3 must compare equal.
    EXPECT_EQ(Range(3, Direction::TO, 3), Range(3, Direction::DOWNTO, 3));

    std::hash<Range> h;
    EXPECT_EQ(h(Range(3, Direction::TO, 3)), h(Range(3, Direction::DOWNTO, 3)));

    std::unordered_set<Range> const single{
        Range(3, Direction::TO, 3), Range(3, Direction::DOWNTO, 3)
    };
    EXPECT_EQ(single.size(), 1U);
}

TEST(TestRange, OtherConstructors) {
    EXPECT_EQ(Range(1, 8), Range(1, Direction::TO, 8));
    EXPECT_EQ(Range(3, -4), Range(3, Direction::DOWNTO, -4));
    // L == H is a single-element range; direction is canonically TO.
    EXPECT_EQ(Range(5, 5).direction, Direction::TO);
}

TEST(TestRange, Formatter) {
    EXPECT_EQ(std::format("{}", Range(5, Direction::TO, 9)), "[5 to 9]");
    EXPECT_EQ(std::format("{}", Range(9, Direction::DOWNTO, 5)), "[9 downto 5]");
}

TEST(TestDirection, Formatter) {
    EXPECT_EQ(std::format("{}", Direction::TO), "Direction{to}");
    EXPECT_EQ(std::format("{}", Direction::DOWNTO), "Direction{downto}");
}

TEST(TestRange, UppercaseDirection) {
    Range const r(1, to_direction("TO"), 8);
    EXPECT_EQ(r.direction, Direction::TO);
}

TEST(TestRange, Copy) {
    Range const r(-2, Direction::TO, 1);
    Range const copy_constructed(r);
    Range const copied_assigned = r;

    EXPECT_EQ(r, copy_constructed);
    EXPECT_EQ(r, copied_assigned);
}

TEST(TestRange, RangeIsHashable) {
    std::hash<Range> h;
    Range const r1(1, Direction::TO, 8);
    Range const r2(4, Direction::DOWNTO, -3);
    EXPECT_EQ(h(r1), h(r1));
    EXPECT_EQ(h(r2), h(r2));
    std::unordered_set<Range> const same{
        Range(1, Direction::TO, 8), Range(1, Direction::TO, 8)
    };
    EXPECT_EQ(same.size(), 1U);

    std::unordered_set<Range> const different{
        Range(1, Direction::TO, 8), Range(8, Direction::DOWNTO, 1)
    };
    EXPECT_EQ(different.size(), 2U);
}

// -- is_subsequence_of ------------------------------------------------------

TEST(TestRange, IsSubsequenceLengthZero) {
    // A length-0 child is a subsequence of any parent, regardless of bounds
    // or direction.
    EXPECT_TRUE(
        (Range{99, Direction::TO, 50}).is_subsequence_of(Range{0, Direction::TO, 4})
    );
    EXPECT_TRUE(
        (Range{50, Direction::DOWNTO, 99}).is_subsequence_of(Range{0, Direction::TO, 4})
    );
}

TEST(TestRange, IsSubsequenceLengthOneDirectionAgnostic) {
    // A length-1 child is valid iff its single value is in the parent;
    // direction is irrelevant for one element.
    Range parent{0, Direction::TO, 4};
    EXPECT_TRUE((Range{2, Direction::TO, 2}).is_subsequence_of(parent));
    EXPECT_TRUE((Range{2, Direction::DOWNTO, 2}).is_subsequence_of(parent));
    EXPECT_FALSE((Range{99, Direction::TO, 99}).is_subsequence_of(parent));
}

TEST(TestRange, IsSubsequenceLengthTwoPlus) {
    Range parent{0, Direction::TO, 4};
    EXPECT_TRUE((Range{1, Direction::TO, 3}).is_subsequence_of(parent));
    EXPECT_FALSE((Range{1, Direction::DOWNTO, 0}).is_subsequence_of(parent));  // dir
    EXPECT_FALSE((Range{99, Direction::TO, 100}).is_subsequence_of(parent));   // left
    EXPECT_FALSE((Range{0, Direction::TO, 99}).is_subsequence_of(parent));     // right
}

TEST(TestRange, IsSubsequenceDOWNTOParent) {
    // Exercise the DOWNTO branch of the in_parent helper, mirrored from the
    // TO test above.
    Range parent{4, Direction::DOWNTO, 0};
    EXPECT_TRUE((Range{3, Direction::DOWNTO, 1}).is_subsequence_of(parent));
    EXPECT_FALSE((Range{1, Direction::TO, 3}).is_subsequence_of(parent));  // dir
    EXPECT_FALSE((Range{99, Direction::DOWNTO, 1}).is_subsequence_of(parent));
    EXPECT_FALSE((Range{3, Direction::DOWNTO, -99}).is_subsequence_of(parent));
}

TEST(TestRange, IsSubsequenceConstexpr) {
    // Usable in constant evaluation; downstream slice<R>() forms rely on this
    // for their static_assert.
    static_assert(Range{1, Direction::TO, 3}.is_subsequence_of(Range{0, Direction::TO, 4}));
    static_assert(
        !Range{1, Direction::DOWNTO, 0}.is_subsequence_of(Range{0, Direction::TO, 4})
    );
}

// -- Length overflow protection --------------------------------------------
//
// A Range spanning the full int64_t domain (INT64_MIN..INT64_MAX in TO, or
// reversed in DOWNTO) would mathematically have length 2^64 -- doesn't fit
// in size_t. length() is the only place this check can live: Range's fields
// are public and a Range can be mutated into the full-span state after
// construction, so a construction-time check would be bypassable. Throws
// std::length_error when invoked on such a Range.

TEST(TestRange, LengthFullSpanTOThrows) {
    constexpr auto lo = std::numeric_limits<Range::value_type>::min();
    constexpr auto hi = std::numeric_limits<Range::value_type>::max();
    Range r1{lo, Direction::TO, hi};
    EXPECT_THROW((void)r1.length(), std::length_error);
    // Two-arg auto-direction picks TO since lo < hi -- same behavior.
    Range r2{lo, hi};
    EXPECT_THROW((void)r2.length(), std::length_error);
}

TEST(TestRange, LengthFullSpanDOWNTOThrows) {
    constexpr auto lo = std::numeric_limits<Range::value_type>::min();
    constexpr auto hi = std::numeric_limits<Range::value_type>::max();
    Range r1{hi, Direction::DOWNTO, lo};
    EXPECT_THROW((void)r1.length(), std::length_error);
    // Two-arg auto-direction picks DOWNTO since hi > lo -- same behavior.
    Range r2{hi, lo};
    EXPECT_THROW((void)r2.length(), std::length_error);
}

TEST(TestRange, LengthAfterPostConstructionMutationThrows) {
    // Confirms that the check fires when fields are mutated into the full-
    // span state after construction (the case construction-time checks
    // can't catch).
    Range r{0, Direction::TO, 5};
    EXPECT_EQ(r.length(), 6U);  // sanity
    r.left = std::numeric_limits<Range::value_type>::min();
    r.right = std::numeric_limits<Range::value_type>::max();
    EXPECT_THROW((void)r.length(), std::length_error);
}

TEST(TestRange, NearMaxLengthIsValidTO) {
    // One step short of the full span: max representable length (SIZE_MAX
    // on a 64-bit platform). Constructs fine and length() returns the right
    // value via uint64_t arithmetic.
    constexpr auto lo = std::numeric_limits<Range::value_type>::min();
    constexpr auto hi = std::numeric_limits<Range::value_type>::max();
    Range r1{lo, Direction::TO, hi - 1};
    EXPECT_EQ(r1.length(), std::numeric_limits<size_t>::max());
    Range r2{lo + 1, Direction::TO, hi};
    EXPECT_EQ(r2.length(), std::numeric_limits<size_t>::max());
}

TEST(TestRange, NearMaxLengthIsValidDOWNTO) {
    // DOWNTO mirror of NearMaxLengthIsValidTO. Without this, the validation
    // path `left != hi && right == lo` (DOWNTO with one endpoint at the
    // extreme but not the other) wasn't exercised directly, and length()'s
    // DOWNTO branch wasn't tested at SIZE_MAX.
    constexpr auto lo = std::numeric_limits<Range::value_type>::min();
    constexpr auto hi = std::numeric_limits<Range::value_type>::max();
    Range r1{hi, Direction::DOWNTO, lo + 1};
    EXPECT_EQ(r1.length(), std::numeric_limits<size_t>::max());
    Range r2{hi - 1, Direction::DOWNTO, lo};
    EXPECT_EQ(r2.length(), std::numeric_limits<size_t>::max());
}

TEST(TestRange, LargeNegativeToPositiveLengthTO) {
    // Endpoints straddling zero with a large gap -- naive signed subtraction
    // (right - left) would overflow int64_t even though the unsigned diff
    // fits. The uint64_t-based length() handles this correctly.
    Range r{-1'000'000'000LL, Direction::TO, 1'000'000'000LL};
    EXPECT_EQ(r.length(), 2'000'000'001U);
}

TEST(TestRange, LargeNegativeToPositiveLengthDOWNTO) {
    // DOWNTO mirror -- exercises the uint64_t arithmetic in the DOWNTO
    // branch of length() for endpoints straddling zero.
    Range r{1'000'000'000LL, Direction::DOWNTO, -1'000'000'000LL};
    EXPECT_EQ(r.length(), 2'000'000'001U);
}

TEST(TestRange, DirectionMismatchStillEmpty) {
    // TO direction with right < left, or DOWNTO with left < right, remains
    // a valid (empty) range -- used intentionally for null slices.
    Range r1{5, Direction::TO, 3};
    EXPECT_EQ(r1.length(), 0U);
    Range r2{3, Direction::DOWNTO, 5};
    EXPECT_EQ(r2.length(), 0U);
}
// LCOV_EXCL_BR_STOP

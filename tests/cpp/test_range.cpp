// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types/range.hpp>

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
}

TEST(TestRange, Equality) {
    // Range equality is fully structural: all three fields must match. Two
    // ranges that denote the same sequence but differ in any field (direction,
    // or empty-range bounds) compare unequal. Users who want sequence equality
    // should compare elements via std::ranges::equal.
    EXPECT_EQ(Range(7, Direction::DOWNTO, -7), Range(7, Direction::DOWNTO, -7));
    EXPECT_NE(Range(7, Direction::DOWNTO, -7), Range(0, Direction::TO, 8));
    // Same sequence (both empty), different fields: not equal.
    EXPECT_NE(Range(1, Direction::TO, 0), Range(8, Direction::TO, -8));
    // Same single value, different direction: not equal.
    EXPECT_NE(Range(3, Direction::TO, 3), Range(3, Direction::DOWNTO, 3));
    // Length-2+ shape that the previous loose-equality bug missed.
    EXPECT_NE(Range(5, Direction::TO, 10), Range(5, Direction::DOWNTO, 0));
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

    // Structural equality: ranges whose denoted sequence is identical but
    // whose fields differ are distinct keys.
    std::unordered_set<Range> const distinct_single{
        Range(3, Direction::TO, 3), Range(3, Direction::DOWNTO, 3)
    };
    EXPECT_EQ(distinct_single.size(), 2U);

    std::unordered_set<Range> const distinct_empty{
        Range(1, Direction::TO, 0), Range(8, Direction::TO, -8)
    };
    EXPECT_EQ(distinct_empty.size(), 2U);
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

// -- Near-max length -------------------------------------------------------
//
// One step short of the full int64_t domain: the largest representable length
// (SIZE_MAX on a 64-bit platform). Full-domain spans are Undefined Behavior
// per the spec (no runtime check anywhere), so we don't test them.

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

// -- size() alias ----------------------------------------------------------

TEST(TestRange, SizeAliasesLength) {
    Range const to{1, Direction::TO, 8};
    EXPECT_EQ(to.size(), to.length());
    Range const dt{4, Direction::DOWNTO, -3};
    EXPECT_EQ(dt.size(), dt.length());
    Range const empty{1, Direction::DOWNTO, 4};
    EXPECT_EQ(empty.size(), empty.length());
    EXPECT_EQ(empty.size(), 0U);
}

// -- detail::offset_of / detail::contains ---------------------------------

TEST(TestRange, OffsetOfTO) {
    Range const r{1, Direction::TO, 8};
    EXPECT_EQ(detail::offset_of(r, 1).value(), 0);
    EXPECT_EQ(detail::offset_of(r, 8).value(), 7);
    EXPECT_EQ(detail::offset_of(r, 5).value(), 4);
    EXPECT_FALSE(detail::offset_of(r, 0).has_value());
    EXPECT_FALSE(detail::offset_of(r, 9).has_value());
}

TEST(TestRange, OffsetOfDOWNTO) {
    Range const r{4, Direction::DOWNTO, -3};
    EXPECT_EQ(detail::offset_of(r, 4).value(), 0);
    EXPECT_EQ(detail::offset_of(r, -3).value(), 7);
    EXPECT_EQ(detail::offset_of(r, 0).value(), 4);
    EXPECT_FALSE(detail::offset_of(r, 5).has_value());
    EXPECT_FALSE(detail::offset_of(r, -4).has_value());
}

TEST(TestRange, ContainsTO) {
    Range const r{1, Direction::TO, 8};
    EXPECT_TRUE(detail::contains(r, 1));
    EXPECT_TRUE(detail::contains(r, 8));
    EXPECT_TRUE(detail::contains(r, 5));
    EXPECT_FALSE(detail::contains(r, 0));
    EXPECT_FALSE(detail::contains(r, 9));
}

TEST(TestRange, ContainsDOWNTO) {
    Range const r{4, Direction::DOWNTO, -3};
    EXPECT_TRUE(detail::contains(r, 4));
    EXPECT_TRUE(detail::contains(r, -3));
    EXPECT_TRUE(detail::contains(r, 0));
    EXPECT_FALSE(detail::contains(r, 5));
    EXPECT_FALSE(detail::contains(r, -4));
}

TEST(TestRange, ContainsConstexpr) {
    // `contains` is used in the compile-time index<I>() bounds check; verify
    // it's actually constexpr-evaluable at both directions.
    static_assert(detail::contains(Range{0, Direction::TO, 4}, 2));
    static_assert(!detail::contains(Range{0, Direction::TO, 4}, 99));
    static_assert(detail::contains(Range{4, Direction::DOWNTO, 0}, 2));
    static_assert(!detail::contains(Range{4, Direction::DOWNTO, 0}, -1));
}
// LCOV_EXCL_BR_STOP

// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <format>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace coconext::types;

// -- Construction -----------------------------------------------------------

TEST(TestVector, ConstructFromInitializerList) {
    Vector<int> a({1, 2, 3, 4});
    EXPECT_EQ(a.range().left, 0);
    EXPECT_EQ(a.range().right, 3);
    EXPECT_EQ(a.range().direction, Direction::TO);
}

TEST(TestVector, ConstructFromInitializerListEmpty) {
    Vector<int> a({});
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestVector, ConstructFromInitializerListWithRange) {
    Vector<int> a({10, 20, 30, 40}, Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range(), Range(-2, Direction::TO, 1));
    EXPECT_EQ(a[-2], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestVector, ConstructFromInitializerListWithRangeLengthMismatch) {
    EXPECT_THROW(
        Vector<int> a({1, 2, 3}, Range(0, Direction::TO, 7)), std::invalid_argument
    );
}

TEST(TestVector, ConstructFromRange) {
    Vector<int> a(Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range(), Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range().length(), 4U);
}

TEST(TestVector, ConstructFromSizedRange) {
    std::vector<int> src{1, 2, 3};
    Vector<int> a(src);
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 2));
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
}

TEST(TestVector, ConstructFromSizedRangeWithRange) {
    std::vector<int> src{10, 20, 30, 40};
    Vector<int> a(src, Range(-2, Direction::TO, 1));
    EXPECT_EQ(a[-2], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestVector, ConstructFromSizedRangeLengthMismatch) {
    std::vector<int> src{1, 2, 3};
    EXPECT_THROW(Vector<int> a(src, Range(0, Direction::TO, 7)), std::invalid_argument);
}

// -- range() ----------------------------------------------------------------

TEST(TestVector, RangeAccessor) {
    Vector<int> a({1, 2, 3});
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 2));
}

// -- size() -----------------------------------------------------------------
//
// Mirrors range().length(); also makes std::ranges::size(a) discoverable on
// each Array shape.

TEST(TestVector, SizeMember) {
    Vector<int> a({10, 20, 30, 40});
    EXPECT_EQ(a.size(), 4U);
    EXPECT_EQ(std::ranges::size(a), 4U);
}

TEST(TestStaticArray, SizeMemberIsStatic) {
    using A = Array<int, Range{0, Direction::TO, 7}>;
    static_assert(A::static_range.length() == 8U);
    A a{};
    EXPECT_EQ(a.size(), 8U);
    EXPECT_EQ(std::ranges::size(a), 8U);
}

TEST(TestVector, DynSliceSizeMember) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    EXPECT_EQ(s.size(), 3U);
    EXPECT_EQ(std::ranges::size(s), 3U);
}

TEST(TestVectorStaticSlice, StaticSliceSizeIsStatic) {
    Vector<int> a({10, 20, 30, 40, 50});
    using S = StaticArraySlice<Vector<int>, Range{1, Direction::TO, 3}>;
    static_assert(S::static_range.length() == 3U);
    auto s = a.slice<Range{1, Direction::TO, 3}>();
    EXPECT_EQ(s.size(), 3U);
}

// -- Iteration --------------------------------------------------------------

TEST(TestVector, ForwardIteration) {
    Vector<int> a({1, 2, 3, 4, 5});
    std::vector<int> seen(a.begin(), a.end());
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(TestVector, ReverseIteration) {
    Vector<int> a({1, 2, 3, 4, 5});
    std::vector<int> seen(a.rbegin(), a.rend());
    EXPECT_EQ(seen, (std::vector<int>{5, 4, 3, 2, 1}));
}

TEST(TestVector, IterationConst) {
    Vector<int> const a({1, 2, 3});
    int sum = std::accumulate(a.begin(), a.end(), 0);
    EXPECT_EQ(sum, 6);
}

// -- Find -------------------------------------------------------------------

TEST(TestVector, FindElement) {
    Vector<int> a({10, 20, 30, 40, 50});
    auto it = std::find(a.begin(), a.end(), 30);
    ASSERT_NE(it, a.end());
    EXPECT_EQ(*it, 30);
    EXPECT_EQ(std::distance(a.begin(), it), 2);
}

TEST(TestVector, FindElementMissing) {
    Vector<int> a({10, 20, 30});
    EXPECT_EQ(std::find(a.begin(), a.end(), 99), a.end());
}

// -- index / rindex ---------------------------------------------------------
//
// index_of(seq, v) returns the first HDL coordinate (from the left in
// iteration order) whose element equals v; rindex_of is the same from the
// right (i.e. the last matching element). Both return nullopt when not found.

TEST(TestVector, IndexFoundTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 20}, Range(0, Direction::TO, 3));
    auto i = index_of(a, 20);
    ASSERT_TRUE(i.has_value());
    EXPECT_EQ(*i, 1);  // first 20 is at HDL coord 1
}

TEST(TestVector, IndexFoundDOWNTO) {
    Vector<int> a({10, 20, 30});  // default DOWNTO {2..0}: a[2]=10, a[1]=20, a[0]=30
    auto i = index_of(a, 20);
    ASSERT_TRUE(i.has_value());
    EXPECT_EQ(*i, 1);  // 20 is at HDL coord 1 (DOWNTO)
}

TEST(TestVector, IndexNotFound) {
    Vector<int> a({10, 20, 30});
    EXPECT_FALSE(index_of(a, 99).has_value());
}

TEST(TestVector, IndexEmpty) {
    Vector<int> a({});
    EXPECT_FALSE(index_of(a, 0).has_value());
}

TEST(TestVector, RindexFindsLastOccurrenceTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 20}, Range(0, Direction::TO, 3));
    auto i = rindex_of(a, 20);
    ASSERT_TRUE(i.has_value());
    EXPECT_EQ(*i, 3);  // last 20 is at HDL coord 3
}

TEST(TestVector, RindexFindsLastOccurrenceDOWNTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 20}, Range(3, Direction::DOWNTO, 0));
    // a[3]=10, a[2]=20, a[1]=30, a[0]=20. Last 20 in iteration is at HDL 0.
    auto i = rindex_of(a, 20);
    ASSERT_TRUE(i.has_value());
    EXPECT_EQ(*i, 0);
}

TEST(TestVector, RindexNotFound) {
    Vector<int> a({10, 20, 30});
    EXPECT_FALSE(rindex_of(a, 99).has_value());
}

TEST(TestVector, IndexOnSlice) {
    // Generic Vector<int> defaults to TO {0..4}.
    Vector<int> a({10, 20, 30, 40, 50});
    auto s = a[{1, 3}];  // TO slice covering HDL coords 1, 2, 3 -> 20, 30, 40
    auto i = index_of(s, 30);
    ASSERT_TRUE(i.has_value());
    EXPECT_EQ(*i, 2);
}

TEST(TestStaticArray, IndexAndRindex) {
    Array<int, Range{0, Direction::TO, 4}> a({10, 20, 30, 20, 50});
    auto first = index_of(a, 20);
    auto last = rindex_of(a, 20);
    ASSERT_TRUE(first.has_value() && last.has_value());
    EXPECT_EQ(*first, 1);
    EXPECT_EQ(*last, 3);
    EXPECT_FALSE(index_of(a, 99).has_value());
}

TEST(TestVectorStaticSlice, IndexAndRindex) {
    Vector<int> a({10, 20, 30, 20, 50}, Range(0, Direction::TO, 4));
    auto s = a.slice<Range{1, Direction::TO, 3}>();  // values 20, 30, 20
    auto first = index_of(s, 20);
    auto last = rindex_of(s, 20);
    ASSERT_TRUE(first.has_value() && last.has_value());
    EXPECT_EQ(*first, 1);
    EXPECT_EQ(*last, 3);
}

// -- Indexing ---------------------------------------------------------------

TEST(TestVector, IndexingTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 40}, Range(8, Direction::TO, 11));
    EXPECT_EQ(a[8], 10);
    EXPECT_EQ(a[11], 40);
}

TEST(TestVector, IndexingDOWNTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 40}, Range(10, Direction::DOWNTO, 7));
    EXPECT_EQ(a[10], 10);
    EXPECT_EQ(a[7], 40);
}

TEST(TestVector, IndexingMutates) {
    Vector<int> a({1, 2, 3, 4});
    a[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestVector, IndexingOutOfRange) {
    Vector<int> a(std::vector<int>{1, 2, 3}, Range(8, Direction::TO, 10));
    EXPECT_THROW((void)a[0], std::out_of_range);
    EXPECT_THROW((void)a[100], std::out_of_range);
}

TEST(TestVector, IndexingConst) {
    Vector<int> const a({1, 2, 3});
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
    static_assert(std::is_same_v<decltype(a[0]), int const&>);
}

// -- Slicing ----------------------------------------------------------------

TEST(TestVector, SliceTO) {
    Vector<int> a({1, 2, 3, 4, 5, 6});
    auto s = a[{1, 4}];
    EXPECT_EQ(s.range(), Range(1, Direction::TO, 4));
    EXPECT_EQ(s.range().length(), 4U);
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s[4], 5);
}

TEST(TestVector, SliceDOWNTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 40}, Range(3, Direction::DOWNTO, 0));
    auto s = a[{2, 1}];
    EXPECT_EQ(s.range().length(), 2U);
    EXPECT_EQ(s[2], 20);
    EXPECT_EQ(s[1], 30);
}

TEST(TestVector, SliceMutatesUnderlying) {
    Vector<int> a({10, 20, 30, 40, 50});
    auto s = a[{1, 3}];
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestVector, SliceAssignFromRange) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s = std::vector<int>{20, 30, 40};
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[3], 40);
}

TEST(TestVector, SliceAssignFromInitializerList) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s = {7, 8, 9};
    EXPECT_EQ(a[2], 8);
}

TEST(TestVector, SliceAssignWrongLength) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    EXPECT_THROW((s = std::vector<int>{1, 2, 3, 4}), std::invalid_argument);
    EXPECT_THROW((s = {1, 2}), std::invalid_argument);
}

TEST(TestVector, SliceStartOutOfRange) {
    Vector<int> a({1, 2, 3});
    EXPECT_THROW((void)(a[{99, 100}]), std::invalid_argument);
}

TEST(TestVector, SliceEndOutOfRange) {
    Vector<int> a({1, 2, 3});
    EXPECT_THROW((void)(a[{0, 99}]), std::invalid_argument);
}

TEST(TestVector, SliceDirectionMismatch) {
    Vector<int> a(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    // start=0, end=4 walks against the array's DOWNTO direction.
    EXPECT_THROW((void)(a[{0, 4}]), std::invalid_argument);
}

// -- Null slice corner cases (subsequence validity rule) -------------------
//
// A null range (length 0) is always a valid subsequence, so the slice should
// succeed regardless of bounds or direction.

TEST(TestVector, SliceNullDirectionMismatchOK) {
    Vector<int> a({1, 2, 3, 4, 5});  // Range(0, TO, 4)
    // Range(3, TO, 1) has length 0; the wrong-direction-vs-owner doesn't
    // matter because there are no values to walk.
    auto s = a[{3, Direction::TO, 1}];
    EXPECT_EQ(s.range().length(), 0U);
    EXPECT_EQ(s.begin(), s.end());
}

TEST(TestVector, SliceNullOutOfBoundsOK) {
    Vector<int> a({1, 2, 3, 4, 5});
    // Range(99, TO, 50) has length 0; bounds outside the parent are fine.
    auto s = a[{99, Direction::TO, 50}];
    EXPECT_EQ(s.range().length(), 0U);
}

TEST(TestVector, SliceLengthOneDirectionAgnostic) {
    // Length-1 slice doesn't care about direction; only the single value
    // needs to exist in the parent.
    Vector<int> a({10, 20, 30, 40});  // Range(0, TO, 3)
    auto s = a[{2, Direction::DOWNTO, 2}];
    EXPECT_EQ(s.range().length(), 1U);
    EXPECT_EQ(s[2], 30);
}

TEST(TestVector, SliceOfSliceFlattens) {
    Vector<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    auto s2 = s1[{2, 4}];
    static_assert(
        std::is_same_v<decltype(s2), ArraySlice<Vector<int>>>, "slice-of-slice must flatten"
    );
    EXPECT_EQ(s2[2], 3);
    EXPECT_EQ(s2[4], 5);
}

TEST(TestVector, SliceOfSliceStartOutOfRange) {
    Vector<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    // 99 is outside s1's range.
    EXPECT_THROW((void)(s1[{99, 100}]), std::invalid_argument);
}

TEST(TestVector, SliceOfSliceEndOutOfRange) {
    Vector<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    // 1 is in s1's range, 99 isn't.
    EXPECT_THROW((void)(s1[{1, 99}]), std::invalid_argument);
}

TEST(TestVector, SliceOfSliceDirectionMismatch) {
    Vector<int> a(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    auto s1 = a[{4, 1}];  // DOWNTO slice over coords 4..1
    // Asking for start=1, end=4 walks against s1's DOWNTO direction.
    EXPECT_THROW((void)(s1[{1, 4}]), std::invalid_argument);
}

// Same error paths but on const Vector / const ArraySlice; index() and
// slice() are templates, so const and non-const callers instantiate separate
// specializations and each throw needs to be exercised independently.
TEST(TestVector, IndexingConstOutOfRange) {
    Vector<int> const a({1, 2, 3});
    EXPECT_THROW((void)a[100], std::out_of_range);
}

TEST(TestVector, SliceConstStartOutOfRange) {
    Vector<int> const a({1, 2, 3});
    EXPECT_THROW((void)(a[{99, 100}]), std::invalid_argument);
}

TEST(TestVector, SliceConstEndOutOfRange) {
    Vector<int> const a({1, 2, 3});
    EXPECT_THROW((void)(a[{0, 99}]), std::invalid_argument);
}

TEST(TestVector, SliceConstDirectionMismatch) {
    Vector<int> mut(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    Vector<int> const& a = mut;
    EXPECT_THROW((void)(a[{0, 4}]), std::invalid_argument);
}

TEST(TestVector, ConstSliceErrors) {
    Vector<int> const a({1, 2, 3, 4, 5});
    auto s = a[{0, 4}];
    EXPECT_THROW((void)(s[{99, 100}]), std::invalid_argument);
    EXPECT_THROW((void)(s[{0, 99}]), std::invalid_argument);
}

TEST(TestVector, ConstSliceDirectionMismatch) {
    Vector<int> mut(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    Vector<int> const& a = mut;
    auto s = a[{4, 0}];
    EXPECT_THROW((void)(s[{0, 4}]), std::invalid_argument);
}

TEST(TestVector, ConstSliceOfConstSlice) {
    Vector<int> const a({1, 2, 3, 4, 5});
    auto outer = a[{0, 4}];
    auto inner = outer[{1, 3}];
    EXPECT_EQ(inner.range().length(), 3U);
    EXPECT_EQ(inner[1], 2);
    EXPECT_EQ(inner[3], 4);
}

TEST(TestVector, ConstructLogicFromRange) {
    Vector<Logic> a(Range(3));
    EXPECT_EQ(a.range().length(), 3U);
}

TEST(TestVector, ConstSliceOverConstArray) {
    Vector<int> const a({10, 20, 30, 40});
    auto s = a[{1, 2}];
    static_assert(std::is_same_v<decltype(s), ArraySlice<Vector<int> const>>);
    EXPECT_EQ(s[1], 20);
    static_assert(std::is_same_v<decltype(s[1]), int const&>);
}

TEST(TestVector, ConstSliceIteration) {
    Vector<int> const a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    int sum = std::accumulate(s.begin(), s.end(), 0);
    EXPECT_EQ(sum, 2 + 3 + 4);
}

TEST(TestVector, ConstSliceOverMutableArrayMutates) {
    // std::span-style const propagation: top-level const on the slice does
    // not restrict element access. The slice's own const-ness only fixes the
    // pointer/range; the underlying ArrayT determines element mutability.
    Vector<int> a({1, 2, 3, 4});
    auto const cs = a[{0, 3}];  // const ArraySlice<Vector<int>>
    cs[0] = 99;                 // mutation through const slice
    cs = std::vector<int>{10, 20, 30, 40};
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[3], 40);
}

// -- Equality ---------------------------------------------------------------

TEST(TestVector, EqualityValuesAndRange) {
    EXPECT_EQ(Vector<int>({1, 2, 3, 4}), Vector<int>({1, 2, 3, 4}));
}

TEST(TestVector, InequalityDifferentRange) {
    // Arrays with different ranges have different indexing semantics, so they
    // are not substitutable and must not compare equal.
    Vector<int> a({1, 2, 3});
    Vector<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    EXPECT_NE(a, b);
}

TEST(TestVector, InequalityDifferentValues) {
    EXPECT_NE(Vector<int>({1, 2, 3}), Vector<int>({1, 2, 4}));
}

TEST(TestVector, InequalityDifferentLength) {
    EXPECT_NE(Vector<int>({1, 2, 3}), Vector<int>({1, 2}));
}

TEST(TestVector, EqualityEmptyArrays) {
    Vector<int> a({});
    Vector<int> b({});
    EXPECT_EQ(a, b);
}

// -- Hash -------------------------------------------------------------------

TEST(TestVector, HashEqualArraysSameRange) {
    std::hash<Vector<int>> h;
    Vector<int> a({1, 2, 3, 4});
    Vector<int> b({1, 2, 3, 4});
    EXPECT_EQ(h(a), h(b));
}

TEST(TestVector, HashEmptyArraysWithDifferentBounds) {
    std::hash<Vector<int>> h;
    Vector<int> a({});
    Vector<int> b(std::vector<int>{}, Range(5, Direction::DOWNTO, 8));
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
}

TEST(TestVector, HashSingleElementSameLeftDifferentDirection) {
    std::hash<Vector<int>> h;
    Vector<int> a({42});  // range: 0 TO 0
    Vector<int> b(std::vector<int>{42}, Range(0, Direction::DOWNTO, 0));
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
}

TEST(TestVector, MultiElementDifferentRangeNotEqual) {
    Vector<int> a({1, 2, 3});
    Vector<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    EXPECT_NE(a, b);
}

TEST(TestVector, UnorderedSetDistinguishesByRange) {
    Vector<int> a({1, 2, 3});
    Vector<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    std::unordered_set<Vector<int>> s;
    s.insert(a);
    s.insert(b);
    EXPECT_EQ(s.size(), 2U);
}

TEST(TestVector, UnorderedSetDeduplicatesEmptyArrays) {
    Vector<int> a({});
    Vector<int> b(std::vector<int>{}, Range(5, Direction::DOWNTO, 8));
    std::unordered_set<Vector<int>> s;
    s.insert(a);
    s.insert(b);
    EXPECT_EQ(s.size(), 1U);
}

TEST(TestVector, HashDistinctAcrossElementType) {
    // Element- and range-equivalent values of Vector<int> and Vector<long>
    // are distinct types with no cross-type equality; their hashes must
    // differ.
    Vector<int> a({1, 2, 3});
    Vector<long> b({1L, 2L, 3L});
    auto ha = std::hash<Vector<int>>{}(a);
    auto hb = std::hash<Vector<long>>{}(b);
    EXPECT_NE(ha, hb);
}

// -- Copy semantics ---------------------------------------------------------

TEST(TestVector, Copy) {
    Vector<int> a(std::vector<int>{1, 2, 3, 4}, Range(-2, Direction::TO, 1));
    Vector<int> b = a;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.range(), b.range());
    b[0] = 99;
    EXPECT_EQ(a[0], 3);  // independent storage
}

TEST(TestVector, Move) {
    Vector<int> a({1, 2, 3, 4});
    Vector<int> b = std::move(a);
    EXPECT_EQ(b.range().length(), 4U);
    EXPECT_EQ(b[0], 1);
}

TEST(TestVector, CopyAssignReplacesRange) {
    Vector<int> a({1, 2, 3});  // range: 0 TO 2
    Vector<int> b(std::vector<int>{10, 20, 30, 40}, Range(7, Direction::DOWNTO, 4));
    a = b;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.range(), Range(7, Direction::DOWNTO, 4));
    EXPECT_EQ(a[7], 10);
    EXPECT_EQ(a[4], 40);
}

TEST(TestVector, MoveAssignReplacesRange) {
    Vector<int> a({1, 2, 3});
    Vector<int> b(std::vector<int>{10, 20, 30, 40}, Range(7, Direction::DOWNTO, 4));
    a = std::move(b);
    EXPECT_EQ(a.range(), Range(7, Direction::DOWNTO, 4));
    EXPECT_EQ(a[7], 10);
    EXPECT_EQ(a[4], 40);
}

// -- Formatter --------------------------------------------------------------

TEST(TestVector, FormatterInt) {
    Vector<int> a({1, 2, 3});
    EXPECT_EQ(std::format("{}", a), "Vector[0 to 2]{1, 2, 3}");
}

TEST(TestVector, FormatterEmpty) {
    Vector<int> a({});
    EXPECT_EQ(std::format("{}", a), "Vector[0 to -1]{}");
}

TEST(TestVector, FormatterLogic) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l});
    EXPECT_EQ(std::format("{}", a), "LogicVector[2 downto 0]{\"01X\"}");
}

TEST(TestVector, FormatterBit) {
    Vector<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    EXPECT_EQ(std::format("{}", a), "BitVector[3 downto 0]{\"0101\"}");
}

TEST(TestVector, FormatterLogicSlice) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto s = a[Range(2, 1)];
    EXPECT_EQ(std::format("{}", s), "LogicArraySlice[2 downto 1]{\"1X\"}");
}

TEST(TestVector, FormatterLogicConstSlice) {
    Vector<Logic> const a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto s = a[Range(2, 1)];
    EXPECT_EQ(std::format("{}", s), "LogicArraySlice[2 downto 1]{\"1X\"}");
}

TEST(TestVector, FormatterBitSlice) {
    Vector<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    auto s = a[Range(2, 1)];
    EXPECT_EQ(std::format("{}", s), "BitArraySlice[2 downto 1]{\"10\"}");
}

// -- Static slice of Vector (compile-time-bounded view) -----------------

TEST(TestVectorStaticSlice, SliceHappyPath) {
    Vector<int> a({10, 20, 30, 40, 50});
    auto s = a.slice<Range{1, 3}>();
    static_assert(
        std::is_same_v<decltype(s), StaticArraySlice<Vector<int>, Range{1, 3}>>,
        "Vector::slice<R>() must return a static StaticArraySlice"
    );
    static_assert(decltype(s)::static_range == Range{1, Direction::TO, 3});
    EXPECT_EQ(s.range().length(), 3U);
    EXPECT_EQ(s[1], 20);
    EXPECT_EQ(s[3], 40);
}

TEST(TestVectorStaticSlice, SliceMutatesUnderlying) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestVectorStaticSlice, SliceAssignFromRange) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s = std::vector<int>{20, 30, 40};
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[3], 40);
}

TEST(TestVectorStaticSlice, SliceAssignFromInitializerList) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s = {7, 8, 9};
    EXPECT_EQ(a[2], 8);
}

TEST(TestVectorStaticSlice, SliceAssignWrongLength) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    EXPECT_THROW((s = std::vector<int>{1, 2, 3, 4}), std::invalid_argument);
}

TEST(TestVectorStaticSlice, SliceAssignFromStaticRangedSequence) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    Vector<int> rhs_owner({70, 80, 90});
    auto rhs = rhs_owner.slice<Range{0, 2}>();  // static slice, length 3
    s = rhs;
    EXPECT_EQ(a[1], 70);
    EXPECT_EQ(a[2], 80);
    EXPECT_EQ(a[3], 90);
}

TEST(TestVectorStaticSlice, SliceOutOfRangeRuntime) {
    Vector<int> a({1, 2, 3});
    EXPECT_THROW((void)(a.slice<Range{99, 100}>()), std::invalid_argument);
}

TEST(TestVectorStaticSlice, SliceConstReturnsConstSlice) {
    Vector<int> const a({10, 20, 30, 40});
    auto s = a.slice<Range{1, 2}>();
    static_assert(
        std::is_same_v<decltype(s), StaticArraySlice<Vector<int> const, Range{1, 2}>>
    );
    static_assert(std::is_same_v<decltype(s[1]), int const&>);
    EXPECT_EQ(s[1], 20);
    EXPECT_EQ(s[2], 30);
}

TEST(TestVectorStaticSlice, RuntimeSubSliceFlattensToDyn) {
    Vector<int> a({1, 2, 3, 4, 5, 6});
    auto s = a.slice<Range{1, 4}>();
    auto sub = s[Range{2, 3}];
    static_assert(
        std::is_same_v<decltype(sub), ArraySlice<Vector<int>>>,
        "runtime sub-slice of a static slice must flatten to ArraySlice"
    );
    EXPECT_EQ(sub[2], 3);
    EXPECT_EQ(sub[3], 4);
}

TEST(TestVectorStaticSlice, StaticSubSliceFlattensToSameOwner) {
    Vector<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s = a.slice<Range{1, 6}>();
    auto sub = s.slice<Range{2, 4}>();
    static_assert(
        std::is_same_v<decltype(sub), StaticArraySlice<Vector<int>, Range{2, 4}>>,
        "static sub-slice flattens to StaticArraySlice<owner, R>, not nested slices"
    );
    EXPECT_EQ(sub[2], 3);
    EXPECT_EQ(sub[4], 5);
}

TEST(TestVectorStaticSlice, NullSliceBoundsOutsideParentOK) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{99, Direction::TO, 50}>();
    EXPECT_EQ(s.range().length(), 0U);
    EXPECT_EQ(s.begin(), s.end());
}

// -- Compile-time dispatch -------------------------------------------------

// Single integral arg -> length-based static range starting at 0.
static_assert(Array<int, 8>::static_range == Range{0, Direction::TO, 7});
static_assert(Array<int, 0>::static_range == Range{0, Direction::TO, -1});
static_assert(Array<int, 1>::static_range == Range{0, Direction::TO, 0});

// Single Range arg -> direct passthrough.
static_assert(
    Array<int, Range{2, Direction::DOWNTO, -5}>::static_range
    == Range{2, Direction::DOWNTO, -5}
);

// Two args -> (left, right), direction inferred.
static_assert(Array<int, 1, 3>::static_range == Range{1, Direction::TO, 3});
static_assert(Array<int, 4, 0>::static_range == Range{4, Direction::DOWNTO, 0});
static_assert(Array<int, -3, 4>::static_range == Range{-3, Direction::TO, 4});

// Three args -> (left, Direction, right) verbatim.
static_assert(Array<int, 1, Direction::TO, 3>::static_range == Range{1, Direction::TO, 3});
static_assert(
    Array<int, 4, Direction::DOWNTO, 0>::static_range == Range{4, Direction::DOWNTO, 0}
);

// Different spellings of the same static range collapse to the same type:
// length, two-arg, three-arg, and Range forms all unify.
static_assert(std::is_same_v<Array<int, 8>, Array<int, Range{0, Direction::TO, 7}>>);
static_assert(std::is_same_v<Array<int, 1, 3>, Array<int, 1, Direction::TO, 3>>);
static_assert(std::is_same_v<Array<int, 4, 0>, Array<int, 4, Direction::DOWNTO, 0>>);

// Forwarding the static_range of one Array into the alias of another.
using A_1_to_4 = Array<int, 1, 4>;
static_assert(std::is_same_v<Array<int, A_1_to_4::static_range>, Array<int, 1, 4>>);

// -- Runtime: verify the static Array alias works -------------------------

TEST(TestArray, StaticLengthViaAlias) {
    Array<int, 4> a({1, 2, 3, 4});
    EXPECT_EQ(a.range(), (Range{0, Direction::TO, 3}));
    EXPECT_EQ(a[2], 3);
    a[3] = 99;
    EXPECT_EQ(a[3], 99);
}

TEST(TestArray, StaticRangeViaAlias) {
    constexpr Range R{10, Direction::TO, 13};
    Array<int, R> a({100, 200, 300, 400});
    EXPECT_EQ(a.range(), R);
    EXPECT_EQ(a[10], 100);
    EXPECT_EQ(a[13], 400);
}

TEST(TestArray, StaticTwoArgViaAlias) {
    Array<int, 2, 5> a({10, 20, 30, 40});
    EXPECT_EQ(a.range(), (Range{2, Direction::TO, 5}));
    EXPECT_EQ(a[2], 10);
    EXPECT_EQ(a[5], 40);
}

TEST(TestArray, StaticTwoArgDOWNTOViaAlias) {
    Array<int, 4, 1> a({10, 20, 30, 40});
    EXPECT_EQ(a.range(), (Range{4, Direction::DOWNTO, 1}));
    EXPECT_EQ(a[4], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestArray, StaticThreeArgViaAlias) {
    Array<int, 4, Direction::DOWNTO, 1> a({10, 20, 30, 40});
    EXPECT_EQ(a.range(), (Range{4, Direction::DOWNTO, 1}));
    EXPECT_EQ(a[4], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestArray, StaticFromForeignStaticRange) {
    using Src = Array<int, 1, 3>;
    Src src({10, 20, 30});
    Array<int, Src::static_range> dst({100, 200, 300});
    EXPECT_EQ(dst.range(), src.range());
    EXPECT_EQ(dst[1], 100);
    EXPECT_EQ(dst[3], 300);
}

// -- ArraySlice::slice<R> (runtime parent, static sub-slice) ------------

TEST(TestVectorSlice, StaticSliceFlattensToOwner) {
    Vector<int> a({10, 20, 30, 40, 50, 60});
    auto dyn = a[{1, 4}];
    auto s = dyn.slice<Range{2, 3}>();
    static_assert(
        std::is_same_v<decltype(s), StaticArraySlice<Vector<int>, Range{2, 3}>>,
        "static sub-slice of ArraySlice flattens to StaticArraySlice<owner, R>"
    );
    EXPECT_EQ(s.range().length(), 2U);
    EXPECT_EQ(s[2], 30);
    EXPECT_EQ(s[3], 40);
}

TEST(TestVectorSlice, StaticSliceMutatesUnderlying) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto dyn = a[{1, 4}];
    auto s = dyn.slice<Range{2, 3}>();
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestVectorSlice, StaticSliceOutOfRangeRuntime) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto dyn = a[{1, 3}];
    EXPECT_THROW((void)(dyn.slice<Range{99, 100}>()), std::invalid_argument);
}

TEST(TestVectorSlice, StaticSliceConstReturnsConstSlice) {
    Vector<int> const a({10, 20, 30, 40, 50});
    auto dyn = a[{1, 4}];
    auto s = dyn.slice<Range{2, 3}>();
    static_assert(
        std::is_same_v<decltype(s), StaticArraySlice<Vector<int> const, Range{2, 3}>>
    );
    static_assert(std::is_same_v<decltype(s[2]), int const&>);
    EXPECT_EQ(s[2], 30);
    EXPECT_EQ(s[3], 40);
}

// -- C++23 multi-subscript operator ----------------------------------------
//
// C++23 (P2128) introduces `arr[a, b]` as a true multi-argument subscript
// (pre-C++23 the comma is a comma-expression, so `arr[a, b]` is `arr[b]`).
// Array/Vector/StaticArraySlice/ArraySlice provide multi-subscript overloads
// that build a Range from the arguments. Without these tests, breakage in
// the C++23-gated overloads would only surface when downstream code compiles
// against C++23 -- the C++20 build can't catch it.
#if __cplusplus >= 202302L

TEST(TestCxx23MultiSubscript, VectorSliceImplicitDirection) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a[1, 4];  // implicit direction (auto-detected by Range(l, r))
    EXPECT_EQ(s.range(), Range(1, Direction::TO, 4));
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s[4], 5);
}

TEST(TestCxx23MultiSubscript, VectorSliceExplicitDirection) {
    Vector<int> a(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    auto s = a[3, Direction::DOWNTO, 1];
    EXPECT_EQ(s.range(), Range(3, Direction::DOWNTO, 1));
}

TEST(TestCxx23MultiSubscript, VectorConstSlice) {
    // Exercises the const overload of multi-subscript (regression: it was
    // missing the `const` qualifier in an earlier revision, so const objects
    // couldn't call it).
    Vector<int> const a({1, 2, 3, 4, 5});
    auto s = a[1, 3];
    static_assert(std::is_same_v<decltype(s), ArraySlice<Vector<int> const>>);
    EXPECT_EQ(s[2], 3);
}

TEST(TestCxx23MultiSubscript, StaticArraySlice) {
    Array<int, Range{0, Direction::TO, 4}> a({10, 20, 30, 40, 50});
    auto s = a[1, 3];
    EXPECT_EQ(s.range(), Range(1, Direction::TO, 3));
    EXPECT_EQ(s[1], 20);
    EXPECT_EQ(s[3], 40);
}

TEST(TestCxx23MultiSubscript, VectorSliceSubSlice) {
    Vector<int> a({1, 2, 3, 4, 5, 6});
    auto dyn = a[0, 5];
    auto sub = dyn[1, 4];
    EXPECT_EQ(sub.range(), Range(1, Direction::TO, 4));
    EXPECT_EQ(sub[1], 2);
}

TEST(TestCxx23MultiSubscript, ArraySliceSubSlice) {
    // StaticArraySliceImpl's multi-subscript was the one that referenced range_
    // (which doesn't exist on static slices) instead of R.direction.
    Array<int, Range{0, Direction::TO, 4}> a({10, 20, 30, 40, 50});
    auto stat = a.slice<Range{1, 3}>();
    auto sub = stat[1, 3];
    EXPECT_EQ(sub.range(), Range(1, Direction::TO, 3));
    EXPECT_EQ(sub[1], 20);
}

// -- Constexpr Vector (C++23 P2273 constexpr unique_ptr) -----------------
//
// COCONEXT_VECTOR_CONSTEXPR expands to `constexpr` under C++23 and to
// nothing under C++20. Under C++23 the Vector ctors / operator[] / range()
// should be evaluable in a constant-expression context. Without a test that
// actually constant-evaluates one, a regression that makes the body non-
// constexpr would silently downgrade the C++23 build without anyone noticing.

constexpr int constexpr_vector_sum() {
    Vector<int> a({10, 20, 30, 40});
    int sum = 0;
    for (auto v : a) {
        sum += v;
    }
    return sum;
}

constexpr Range constexpr_vector_range() {
    Vector<int> a(Range(5, Direction::DOWNTO, 0));
    return a.range();
}

constexpr int constexpr_vector_indexing() {
    Vector<int> a({100, 200, 300});
    return a[1];
}

static_assert(constexpr_vector_sum() == 100);
static_assert(constexpr_vector_range() == Range(5, Direction::DOWNTO, 0));
static_assert(constexpr_vector_indexing() == 200);

#endif

// -- RangedSequence element-type constraint --------------------------------

// One-arg form (default `Elem = void`) matches any element type.
static_assert(RangedSequence<Vector<int>>);
static_assert(RangedSequence<Vector<float>>);
static_assert(!RangedSequence<int>);

// Two-arg form pins the element type. Concrete types only -- concepts can't be
// passed as template arguments in C++20.
static_assert(RangedSequence<Vector<int>, int>);
static_assert(!RangedSequence<Vector<int>, float>);
static_assert(RangedSequence<Vector<float>, float>);

// StaticRangedSequence picks up the same defaulted parameter.
static_assert(StaticRangedSequence<Array<int, Range{0, Direction::TO, 3}>>);
static_assert(StaticRangedSequence<Array<int, Range{0, Direction::TO, 3}>, int>);
static_assert(!StaticRangedSequence<Array<int, Range{0, Direction::TO, 3}>, float>);
static_assert(!StaticRangedSequence<Vector<int>>);
static_assert(!StaticRangedSequence<Vector<int>, int>);

// -- index<I>() on dynamic-range types -------------------------------------
//
// On Vector and ArraySlice the range is a runtime value, so index<I>() can't
// static_assert anything -- it just delegates to operator[](I) for API
// consistency with the static-range siblings (Array::index<I>(),
// StaticArraySlice::index<I>()).

TEST(TestVector, IndexOfVectorTO) {
    Vector<int> a({10, 20, 30, 40}, Range(0, Direction::TO, 3));
    EXPECT_EQ(a.index<0>(), 10);
    EXPECT_EQ(a.index<3>(), 40);
    a.index<2>() = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestVector, IndexOfVectorOutOfRangeThrows) {
    Vector<int> a({10, 20, 30, 40}, Range(0, Direction::TO, 3));
    EXPECT_THROW((void)a.index<4>(), std::out_of_range);
    EXPECT_THROW((void)a.index<-1>(), std::out_of_range);
}

TEST(TestVector, IndexOfConstVector) {
    Vector<int> const a({10, 20, 30});
    EXPECT_EQ(a.index<1>(), 20);
}

TEST(TestVector, IndexOfDynamicSlice) {
    Vector<int> a({10, 20, 30, 40, 50});
    auto s = a[{1, 3}];  // dynamic ArraySlice over indices 1..3
    EXPECT_EQ(s.index<1>(), 20);
    EXPECT_EQ(s.index<3>(), 40);
    s.index<2>() = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestVector, IndexOfDynamicSliceOutOfRangeThrows) {
    Vector<int> a({10, 20, 30, 40, 50});
    auto s = a[{1, 3}];
    EXPECT_THROW((void)s.index<0>(), std::out_of_range);
    EXPECT_THROW((void)s.index<4>(), std::out_of_range);
}
// LCOV_EXCL_BR_STOP

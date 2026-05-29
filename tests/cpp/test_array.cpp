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

TEST(TestDynArray, ConstructFromInitializerList) {
    Vector<int> a({1, 2, 3, 4});
    EXPECT_EQ(a.range().left, 0);
    EXPECT_EQ(a.range().right, 3);
    EXPECT_EQ(a.range().direction, Direction::TO);
}

TEST(TestDynArray, ConstructFromInitializerListEmpty) {
    Vector<int> a({});
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestDynArray, ConstructFromInitializerListWithRange) {
    Vector<int> a({10, 20, 30, 40}, Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range(), Range(-2, Direction::TO, 1));
    EXPECT_EQ(a[-2], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestDynArray, ConstructFromInitializerListWithRangeLengthMismatch) {
    EXPECT_THROW(
        Vector<int> a({1, 2, 3}, Range(0, Direction::TO, 7)), std::invalid_argument
    );
}

TEST(TestDynArray, ConstructFromRange) {
    Vector<int> a(Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range(), Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range().length(), 4U);
}

TEST(TestDynArray, ConstructFromSizedRange) {
    std::vector<int> src{1, 2, 3};
    Vector<int> a(src);
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 2));
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
}

TEST(TestDynArray, ConstructFromSizedRangeWithRange) {
    std::vector<int> src{10, 20, 30, 40};
    Vector<int> a(src, Range(-2, Direction::TO, 1));
    EXPECT_EQ(a[-2], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestDynArray, ConstructFromSizedRangeLengthMismatch) {
    std::vector<int> src{1, 2, 3};
    EXPECT_THROW(Vector<int> a(src, Range(0, Direction::TO, 7)), std::invalid_argument);
}

// -- range() ----------------------------------------------------------------

TEST(TestDynArray, RangeAccessor) {
    Vector<int> a({1, 2, 3});
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 2));
}

// -- Iteration --------------------------------------------------------------

TEST(TestDynArray, ForwardIteration) {
    Vector<int> a({1, 2, 3, 4, 5});
    std::vector<int> seen(a.begin(), a.end());
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(TestDynArray, ReverseIteration) {
    Vector<int> a({1, 2, 3, 4, 5});
    std::vector<int> seen(a.rbegin(), a.rend());
    EXPECT_EQ(seen, (std::vector<int>{5, 4, 3, 2, 1}));
}

TEST(TestDynArray, IterationConst) {
    Vector<int> const a({1, 2, 3});
    int sum = std::accumulate(a.begin(), a.end(), 0);
    EXPECT_EQ(sum, 6);
}

// -- Find -------------------------------------------------------------------

TEST(TestDynArray, FindElement) {
    Vector<int> a({10, 20, 30, 40, 50});
    auto it = std::find(a.begin(), a.end(), 30);
    ASSERT_NE(it, a.end());
    EXPECT_EQ(*it, 30);
    EXPECT_EQ(std::distance(a.begin(), it), 2);
}

TEST(TestDynArray, FindElementMissing) {
    Vector<int> a({10, 20, 30});
    EXPECT_EQ(std::find(a.begin(), a.end(), 99), a.end());
}

// -- Indexing ---------------------------------------------------------------

TEST(TestDynArray, IndexingTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 40}, Range(8, Direction::TO, 11));
    EXPECT_EQ(a[8], 10);
    EXPECT_EQ(a[11], 40);
}

TEST(TestDynArray, IndexingDOWNTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 40}, Range(10, Direction::DOWNTO, 7));
    EXPECT_EQ(a[10], 10);
    EXPECT_EQ(a[7], 40);
}

TEST(TestDynArray, IndexingMutates) {
    Vector<int> a({1, 2, 3, 4});
    a[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestDynArray, IndexingOutOfRange) {
    Vector<int> a(std::vector<int>{1, 2, 3}, Range(8, Direction::TO, 10));
    EXPECT_THROW((void)a[0], std::out_of_range);
    EXPECT_THROW((void)a[100], std::out_of_range);
}

TEST(TestDynArray, IndexingConst) {
    Vector<int> const a({1, 2, 3});
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
    static_assert(std::is_same_v<decltype(a[0]), int const&>);
}

// -- Slicing ----------------------------------------------------------------

TEST(TestDynArray, SliceTO) {
    Vector<int> a({1, 2, 3, 4, 5, 6});
    auto s = a[{1, 4}];
    EXPECT_EQ(s.range(), Range(1, Direction::TO, 4));
    EXPECT_EQ(s.range().length(), 4U);
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s[4], 5);
}

TEST(TestDynArray, SliceDOWNTO) {
    Vector<int> a(std::vector<int>{10, 20, 30, 40}, Range(3, Direction::DOWNTO, 0));
    auto s = a[{2, 1}];
    EXPECT_EQ(s.range().length(), 2U);
    EXPECT_EQ(s[2], 20);
    EXPECT_EQ(s[1], 30);
}

TEST(TestDynArray, SliceMutatesUnderlying) {
    Vector<int> a({10, 20, 30, 40, 50});
    auto s = a[{1, 3}];
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestDynArray, SliceAssignFromRange) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s = std::vector<int>{20, 30, 40};
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[3], 40);
}

TEST(TestDynArray, SliceAssignFromInitializerList) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s = {7, 8, 9};
    EXPECT_EQ(a[2], 8);
}

TEST(TestDynArray, SliceAssignWrongLength) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    EXPECT_THROW((s = std::vector<int>{1, 2, 3, 4}), std::invalid_argument);
    EXPECT_THROW((s = {1, 2}), std::invalid_argument);
}

TEST(TestDynArray, SliceStartOutOfRange) {
    Vector<int> a({1, 2, 3});
    EXPECT_THROW((void)(a[{99, 100}]), std::invalid_argument);
}

TEST(TestDynArray, SliceEndOutOfRange) {
    Vector<int> a({1, 2, 3});
    EXPECT_THROW((void)(a[{0, 99}]), std::invalid_argument);
}

TEST(TestDynArray, SliceDirectionMismatch) {
    Vector<int> a(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    // start=0, end=4 walks against the array's DOWNTO direction.
    EXPECT_THROW((void)(a[{0, 4}]), std::invalid_argument);
}

// -- Null slice corner cases (subsequence validity rule) -------------------
//
// A null range (length 0) is always a valid subsequence, so the slice should
// succeed regardless of bounds or direction.

TEST(TestDynArray, SliceNullDirectionMismatchOK) {
    Vector<int> a({1, 2, 3, 4, 5});  // Range(0, TO, 4)
    // Range(3, TO, 1) has length 0; the wrong-direction-vs-owner doesn't
    // matter because there are no values to walk.
    auto s = a[{3, Direction::TO, 1}];
    EXPECT_EQ(s.range().length(), 0U);
    EXPECT_EQ(s.begin(), s.end());
}

TEST(TestDynArray, SliceNullOutOfBoundsOK) {
    Vector<int> a({1, 2, 3, 4, 5});
    // Range(99, TO, 50) has length 0; bounds outside the parent are fine.
    auto s = a[{99, Direction::TO, 50}];
    EXPECT_EQ(s.range().length(), 0U);
}

TEST(TestDynArray, SliceLengthOneDirectionAgnostic) {
    // Length-1 slice doesn't care about direction; only the single value
    // needs to exist in the parent.
    Vector<int> a({10, 20, 30, 40});  // Range(0, TO, 3)
    auto s = a[{2, Direction::DOWNTO, 2}];
    EXPECT_EQ(s.range().length(), 1U);
    EXPECT_EQ(s[2], 30);
}

TEST(TestDynArray, SliceOfSliceFlattens) {
    Vector<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    auto s2 = s1[{2, 4}];
    static_assert(
        std::is_same_v<decltype(s2), DynArraySlice<Vector<int>>>,
        "slice-of-slice must flatten"
    );
    EXPECT_EQ(s2[2], 3);
    EXPECT_EQ(s2[4], 5);
}

TEST(TestDynArray, SliceOfSliceStartOutOfRange) {
    Vector<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    // 99 is outside s1's range.
    EXPECT_THROW((void)(s1[{99, 100}]), std::invalid_argument);
}

TEST(TestDynArray, SliceOfSliceEndOutOfRange) {
    Vector<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    // 1 is in s1's range, 99 isn't.
    EXPECT_THROW((void)(s1[{1, 99}]), std::invalid_argument);
}

TEST(TestDynArray, SliceOfSliceDirectionMismatch) {
    Vector<int> a(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    auto s1 = a[{4, 1}];  // DOWNTO slice over coords 4..1
    // Asking for start=1, end=4 walks against s1's DOWNTO direction.
    EXPECT_THROW((void)(s1[{1, 4}]), std::invalid_argument);
}

// Same error paths but on const Vector / const DynArraySlice; index() and
// slice() are templates, so const and non-const callers instantiate separate
// specializations and each throw needs to be exercised independently.
TEST(TestDynArray, IndexingConstOutOfRange) {
    Vector<int> const a({1, 2, 3});
    EXPECT_THROW((void)a[100], std::out_of_range);
}

TEST(TestDynArray, SliceConstStartOutOfRange) {
    Vector<int> const a({1, 2, 3});
    EXPECT_THROW((void)(a[{99, 100}]), std::invalid_argument);
}

TEST(TestDynArray, SliceConstEndOutOfRange) {
    Vector<int> const a({1, 2, 3});
    EXPECT_THROW((void)(a[{0, 99}]), std::invalid_argument);
}

TEST(TestDynArray, SliceConstDirectionMismatch) {
    Vector<int> mut(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    Vector<int> const& a = mut;
    EXPECT_THROW((void)(a[{0, 4}]), std::invalid_argument);
}

TEST(TestDynArray, ConstSliceErrors) {
    Vector<int> const a({1, 2, 3, 4, 5});
    auto s = a[{0, 4}];
    EXPECT_THROW((void)(s[{99, 100}]), std::invalid_argument);
    EXPECT_THROW((void)(s[{0, 99}]), std::invalid_argument);
}

TEST(TestDynArray, ConstSliceDirectionMismatch) {
    Vector<int> mut(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    Vector<int> const& a = mut;
    auto s = a[{4, 0}];
    EXPECT_THROW((void)(s[{0, 4}]), std::invalid_argument);
}

TEST(TestDynArray, ConstSliceOfConstSlice) {
    Vector<int> const a({1, 2, 3, 4, 5});
    auto outer = a[{0, 4}];
    auto inner = outer[{1, 3}];
    EXPECT_EQ(inner.range().length(), 3U);
    EXPECT_EQ(inner[1], 2);
    EXPECT_EQ(inner[3], 4);
}

TEST(TestDynArray, ConstructLogicFromRange) {
    Vector<Logic> a(Range(3));
    EXPECT_EQ(a.range().length(), 3U);
}

TEST(TestDynArray, ConstSliceOverConstArray) {
    Vector<int> const a({10, 20, 30, 40});
    auto s = a[{1, 2}];
    static_assert(std::is_same_v<decltype(s), DynArraySlice<Vector<int> const>>);
    EXPECT_EQ(s[1], 20);
    static_assert(std::is_same_v<decltype(s[1]), int const&>);
}

TEST(TestDynArray, ConstSliceIteration) {
    Vector<int> const a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    int sum = std::accumulate(s.begin(), s.end(), 0);
    EXPECT_EQ(sum, 2 + 3 + 4);
}

TEST(TestDynArray, ConstSliceOverMutableArrayMutates) {
    // std::span-style const propagation: top-level const on the slice does
    // not restrict element access. The slice's own const-ness only fixes the
    // pointer/range; the underlying ArrayT determines element mutability.
    Vector<int> a({1, 2, 3, 4});
    auto const cs = a[{0, 3}];  // const DynArraySlice<Vector<int>>
    cs[0] = 99;                 // mutation through const slice
    cs = std::vector<int>{10, 20, 30, 40};
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[3], 40);
}

// -- Equality ---------------------------------------------------------------

TEST(TestDynArray, EqualityValuesAndRange) {
    EXPECT_EQ(Vector<int>({1, 2, 3, 4}), Vector<int>({1, 2, 3, 4}));
}

TEST(TestDynArray, InequalityDifferentRange) {
    // Arrays with different ranges have different indexing semantics, so they
    // are not substitutable and must not compare equal.
    Vector<int> a({1, 2, 3});
    Vector<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    EXPECT_NE(a, b);
}

TEST(TestDynArray, InequalityDifferentValues) {
    EXPECT_NE(Vector<int>({1, 2, 3}), Vector<int>({1, 2, 4}));
}

TEST(TestDynArray, InequalityDifferentLength) {
    EXPECT_NE(Vector<int>({1, 2, 3}), Vector<int>({1, 2}));
}

TEST(TestDynArray, EqualityEmptyArrays) {
    Vector<int> a({});
    Vector<int> b({});
    EXPECT_EQ(a, b);
}

// -- Hash -------------------------------------------------------------------

TEST(TestDynArray, HashEqualArraysSameRange) {
    std::hash<Vector<int>> h;
    Vector<int> a({1, 2, 3, 4});
    Vector<int> b({1, 2, 3, 4});
    EXPECT_EQ(h(a), h(b));
}

TEST(TestDynArray, HashEmptyArraysWithDifferentBounds) {
    std::hash<Vector<int>> h;
    Vector<int> a({});
    Vector<int> b(std::vector<int>{}, Range(5, Direction::DOWNTO, 8));
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
}

TEST(TestDynArray, HashSingleElementSameLeftDifferentDirection) {
    std::hash<Vector<int>> h;
    Vector<int> a({42});  // range: 0 TO 0
    Vector<int> b(std::vector<int>{42}, Range(0, Direction::DOWNTO, 0));
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
}

TEST(TestDynArray, MultiElementDifferentRangeNotEqual) {
    Vector<int> a({1, 2, 3});
    Vector<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    EXPECT_NE(a, b);
}

TEST(TestDynArray, UnorderedSetDistinguishesByRange) {
    Vector<int> a({1, 2, 3});
    Vector<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    std::unordered_set<Vector<int>> s;
    s.insert(a);
    s.insert(b);
    EXPECT_EQ(s.size(), 2U);
}

TEST(TestDynArray, UnorderedSetDeduplicatesEmptyArrays) {
    Vector<int> a({});
    Vector<int> b(std::vector<int>{}, Range(5, Direction::DOWNTO, 8));
    std::unordered_set<Vector<int>> s;
    s.insert(a);
    s.insert(b);
    EXPECT_EQ(s.size(), 1U);
}

// -- Copy semantics ---------------------------------------------------------

TEST(TestDynArray, Copy) {
    Vector<int> a(std::vector<int>{1, 2, 3, 4}, Range(-2, Direction::TO, 1));
    Vector<int> b = a;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.range(), b.range());
    b[0] = 99;
    EXPECT_EQ(a[0], 3);  // independent storage
}

TEST(TestDynArray, Move) {
    Vector<int> a({1, 2, 3, 4});
    Vector<int> b = std::move(a);
    EXPECT_EQ(b.range().length(), 4U);
    EXPECT_EQ(b[0], 1);
}

TEST(TestDynArray, CopyAssignReplacesRange) {
    Vector<int> a({1, 2, 3});  // range: 0 TO 2
    Vector<int> b(std::vector<int>{10, 20, 30, 40}, Range(7, Direction::DOWNTO, 4));
    a = b;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.range(), Range(7, Direction::DOWNTO, 4));
    EXPECT_EQ(a[7], 10);
    EXPECT_EQ(a[4], 40);
}

TEST(TestDynArray, MoveAssignReplacesRange) {
    Vector<int> a({1, 2, 3});
    Vector<int> b(std::vector<int>{10, 20, 30, 40}, Range(7, Direction::DOWNTO, 4));
    a = std::move(b);
    EXPECT_EQ(a.range(), Range(7, Direction::DOWNTO, 4));
    EXPECT_EQ(a[7], 10);
    EXPECT_EQ(a[4], 40);
}

// -- Formatter --------------------------------------------------------------

TEST(TestDynArray, FormatterInt) {
    Vector<int> a({1, 2, 3});
    EXPECT_EQ(std::format("{}", a), "[0 to 2]{1, 2, 3}");
}

TEST(TestDynArray, FormatterEmpty) {
    Vector<int> a({});
    EXPECT_EQ(std::format("{}", a), "[0 to -1]{}");
}

TEST(TestDynArray, FormatterLogic) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l});
    EXPECT_EQ(std::format("{}", a), "Logic[0 to 2]{0, 1, X}");
}

TEST(TestDynArray, FormatterBit) {
    Vector<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    EXPECT_EQ(std::format("{}", a), "Bit[0 to 3]{0, 1, 0, 1}");
}

TEST(TestDynArray, FormatterLogicSlice) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto s = a[Range(1, 2)];
    EXPECT_EQ(std::format("{}", s), "Logic[1 to 2]{1, X}");
}

TEST(TestDynArray, FormatterLogicConstSlice) {
    Vector<Logic> const a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto s = a[Range(1, 2)];
    EXPECT_EQ(std::format("{}", s), "Logic[1 to 2]{1, X}");
}

TEST(TestDynArray, FormatterBitSlice) {
    Vector<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    auto s = a[Range(1, 2)];
    EXPECT_EQ(std::format("{}", s), "Bit[1 to 2]{1, 0}");
}

// -- Static slice of Vector (compile-time-bounded view) -----------------

TEST(TestDynArrayStaticSlice, SliceHappyPath) {
    Vector<int> a({10, 20, 30, 40, 50});
    auto s = a.slice<Range{1, 3}>();
    static_assert(
        std::is_same_v<decltype(s), ArraySlice<Vector<int>, Range{1, 3}>>,
        "Vector::slice<R>() must return a static ArraySlice"
    );
    static_assert(decltype(s)::range() == Range{1, Direction::TO, 3});
    EXPECT_EQ(s.range().length(), 3U);
    EXPECT_EQ(s[1], 20);
    EXPECT_EQ(s[3], 40);
}

TEST(TestDynArrayStaticSlice, SliceMutatesUnderlying) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestDynArrayStaticSlice, SliceAssignFromRange) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s = std::vector<int>{20, 30, 40};
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[3], 40);
}

TEST(TestDynArrayStaticSlice, SliceAssignFromInitializerList) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s = {7, 8, 9};
    EXPECT_EQ(a[2], 8);
}

TEST(TestDynArrayStaticSlice, SliceAssignWrongLength) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    EXPECT_THROW((s = std::vector<int>{1, 2, 3, 4}), std::invalid_argument);
}

TEST(TestDynArrayStaticSlice, SliceAssignFromStaticRangedSequence) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    Vector<int> rhs_owner({70, 80, 90});
    auto rhs = rhs_owner.slice<Range{0, 2}>();  // static slice, length 3
    s = rhs;
    EXPECT_EQ(a[1], 70);
    EXPECT_EQ(a[2], 80);
    EXPECT_EQ(a[3], 90);
}

TEST(TestDynArrayStaticSlice, SliceOutOfRangeRuntime) {
    Vector<int> a({1, 2, 3});
    EXPECT_THROW((void)(a.slice<Range{99, 100}>()), std::invalid_argument);
}

TEST(TestDynArrayStaticSlice, SliceConstReturnsConstSlice) {
    Vector<int> const a({10, 20, 30, 40});
    auto s = a.slice<Range{1, 2}>();
    static_assert(std::is_same_v<decltype(s), ArraySlice<Vector<int> const, Range{1, 2}>>);
    static_assert(std::is_same_v<decltype(s[1]), int const&>);
    EXPECT_EQ(s[1], 20);
    EXPECT_EQ(s[2], 30);
}

TEST(TestDynArrayStaticSlice, RuntimeSubSliceFlattensToDyn) {
    Vector<int> a({1, 2, 3, 4, 5, 6});
    auto s = a.slice<Range{1, 4}>();
    auto sub = s[Range{2, 3}];
    static_assert(
        std::is_same_v<decltype(sub), DynArraySlice<Vector<int>>>,
        "runtime sub-slice of a static slice must flatten to DynArraySlice"
    );
    EXPECT_EQ(sub[2], 3);
    EXPECT_EQ(sub[3], 4);
}

TEST(TestDynArrayStaticSlice, StaticSubSliceFlattensToSameOwner) {
    Vector<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s = a.slice<Range{1, 6}>();
    auto sub = s.slice<Range{2, 4}>();
    static_assert(
        std::is_same_v<decltype(sub), ArraySlice<Vector<int>, Range{2, 4}>>,
        "static sub-slice flattens to ArraySlice<owner, R>, not nested slices"
    );
    EXPECT_EQ(sub[2], 3);
    EXPECT_EQ(sub[4], 5);
}

TEST(TestDynArrayStaticSlice, NullSliceBoundsOutsideParentOK) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{99, Direction::TO, 50}>();
    EXPECT_EQ(s.range().length(), 0U);
    EXPECT_EQ(s.begin(), s.end());
}

// -- Compile-time dispatch -------------------------------------------------

// Single integral arg -> length-based static range starting at 0.
static_assert(Array<int, 8>::range() == Range{0, Direction::TO, 7});
static_assert(Array<int, 0>::range() == Range{0, Direction::TO, -1});
static_assert(Array<int, 1>::range() == Range{0, Direction::TO, 0});

// Single Range arg -> direct passthrough.
static_assert(
    Array<int, Range{2, Direction::DOWNTO, -5}>::range() == Range{2, Direction::DOWNTO, -5}
);

// Two args -> (left, right), direction inferred.
static_assert(Array<int, 1, 3>::range() == Range{1, Direction::TO, 3});
static_assert(Array<int, 4, 0>::range() == Range{4, Direction::DOWNTO, 0});
static_assert(Array<int, -3, 4>::range() == Range{-3, Direction::TO, 4});

// Three args -> (left, Direction, right) verbatim.
static_assert(Array<int, 1, Direction::TO, 3>::range() == Range{1, Direction::TO, 3});
static_assert(
    Array<int, 4, Direction::DOWNTO, 0>::range() == Range{4, Direction::DOWNTO, 0}
);

// Different spellings of the same static range collapse to the same type:
// length, two-arg, three-arg, and Range forms all unify.
static_assert(std::is_same_v<Array<int, 8>, Array<int, Range{0, Direction::TO, 7}>>);
static_assert(std::is_same_v<Array<int, 1, 3>, Array<int, 1, Direction::TO, 3>>);
static_assert(std::is_same_v<Array<int, 4, 0>, Array<int, 4, Direction::DOWNTO, 0>>);

// Forwarding the static_range of one Array into the alias of another.
using A_1_to_4 = Array<int, 1, 4>;
static_assert(std::is_same_v<Array<int, A_1_to_4::range()>, Array<int, 1, 4>>);

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
    Array<int, Src::range()> dst({100, 200, 300});
    EXPECT_EQ(dst.range(), src.range());
    EXPECT_EQ(dst[1], 100);
    EXPECT_EQ(dst[3], 300);
}

// -- DynArraySlice::slice<R> (runtime parent, static sub-slice) ------------

TEST(TestDynArraySlice, StaticSliceFlattensToOwner) {
    Vector<int> a({10, 20, 30, 40, 50, 60});
    auto dyn = a[{1, 4}];
    auto s = dyn.slice<Range{2, 3}>();
    static_assert(
        std::is_same_v<decltype(s), ArraySlice<Vector<int>, Range{2, 3}>>,
        "static sub-slice of DynArraySlice flattens to ArraySlice<owner, R>"
    );
    EXPECT_EQ(s.range().length(), 2U);
    EXPECT_EQ(s[2], 30);
    EXPECT_EQ(s[3], 40);
}

TEST(TestDynArraySlice, StaticSliceMutatesUnderlying) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto dyn = a[{1, 4}];
    auto s = dyn.slice<Range{2, 3}>();
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestDynArraySlice, StaticSliceOutOfRangeRuntime) {
    Vector<int> a({1, 2, 3, 4, 5});
    auto dyn = a[{1, 3}];
    EXPECT_THROW((void)(dyn.slice<Range{99, 100}>()), std::invalid_argument);
}

TEST(TestDynArraySlice, StaticSliceConstReturnsConstSlice) {
    Vector<int> const a({10, 20, 30, 40, 50});
    auto dyn = a[{1, 4}];
    auto s = dyn.slice<Range{2, 3}>();
    static_assert(std::is_same_v<decltype(s), ArraySlice<Vector<int> const, Range{2, 3}>>);
    static_assert(std::is_same_v<decltype(s[2]), int const&>);
    EXPECT_EQ(s[2], 30);
    EXPECT_EQ(s[3], 40);
}

// LCOV_EXCL_BR_STOP

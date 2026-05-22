// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <format>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

using namespace coconext::types;

// -- Construction -----------------------------------------------------------

TEST(TestDynArray, ConstructFromInitializerList) {
    DynArray<int> a({1, 2, 3, 4});
    EXPECT_EQ(a.range().left, 0);
    EXPECT_EQ(a.range().right, 3);
    EXPECT_EQ(a.range().direction, Direction::TO);
}

TEST(TestDynArray, ConstructFromInitializerListEmpty) {
    DynArray<int> a({});
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestDynArray, ConstructFromInitializerListWithRange) {
    DynArray<int> a({10, 20, 30, 40}, Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range(), Range(-2, Direction::TO, 1));
    EXPECT_EQ(a[-2], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestDynArray, ConstructFromInitializerListWithRangeLengthMismatch) {
    EXPECT_THROW(
        DynArray<int> a({1, 2, 3}, Range(0, Direction::TO, 7)), std::invalid_argument
    );
}

TEST(TestDynArray, ConstructFromRange) {
    DynArray<int> a(Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range(), Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range().length(), 4U);
}

TEST(TestDynArray, ConstructFromSizedRange) {
    std::vector<int> src{1, 2, 3};
    DynArray<int> a(src);
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 2));
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
}

TEST(TestDynArray, ConstructFromSizedRangeWithRange) {
    std::vector<int> src{10, 20, 30, 40};
    DynArray<int> a(src, Range(-2, Direction::TO, 1));
    EXPECT_EQ(a[-2], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestDynArray, ConstructFromSizedRangeLengthMismatch) {
    std::vector<int> src{1, 2, 3};
    EXPECT_THROW(DynArray<int> a(src, Range(0, Direction::TO, 7)), std::invalid_argument);
}

// -- range() ----------------------------------------------------------------

TEST(TestDynArray, RangeAccessor) {
    DynArray<int> a({1, 2, 3});
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 2));
}

// -- Iteration --------------------------------------------------------------

TEST(TestDynArray, ForwardIteration) {
    DynArray<int> a({1, 2, 3, 4, 5});
    std::vector<int> seen(a.begin(), a.end());
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(TestDynArray, ReverseIteration) {
    DynArray<int> a({1, 2, 3, 4, 5});
    std::vector<int> seen(a.rbegin(), a.rend());
    EXPECT_EQ(seen, (std::vector<int>{5, 4, 3, 2, 1}));
}

TEST(TestDynArray, IterationConst) {
    DynArray<int> const a({1, 2, 3});
    int sum = std::accumulate(a.begin(), a.end(), 0);
    EXPECT_EQ(sum, 6);
}

// -- Indexing ---------------------------------------------------------------

TEST(TestDynArray, IndexingTO) {
    DynArray<int> a(std::vector<int>{10, 20, 30, 40}, Range(8, Direction::TO, 11));
    EXPECT_EQ(a[8], 10);
    EXPECT_EQ(a[11], 40);
}

TEST(TestDynArray, IndexingDOWNTO) {
    DynArray<int> a(std::vector<int>{10, 20, 30, 40}, Range(10, Direction::DOWNTO, 7));
    EXPECT_EQ(a[10], 10);
    EXPECT_EQ(a[7], 40);
}

TEST(TestDynArray, IndexingMutates) {
    DynArray<int> a({1, 2, 3, 4});
    a[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestDynArray, IndexingOutOfRange) {
    DynArray<int> a(std::vector<int>{1, 2, 3}, Range(8, Direction::TO, 10));
    EXPECT_THROW((void)a[0], std::out_of_range);
    EXPECT_THROW((void)a[100], std::out_of_range);
}

TEST(TestDynArray, IndexingConst) {
    DynArray<int> const a({1, 2, 3});
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
    static_assert(std::is_same_v<decltype(a[0]), int const&>);
}

// -- Slicing ----------------------------------------------------------------

TEST(TestDynArray, SliceTO) {
    DynArray<int> a({1, 2, 3, 4, 5, 6});
    auto s = a[{1, 4}];
    EXPECT_EQ(s.range(), Range(1, Direction::TO, 4));
    EXPECT_EQ(s.range().length(), 4U);
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s[4], 5);
}

TEST(TestDynArray, SliceDOWNTO) {
    DynArray<int> a(std::vector<int>{10, 20, 30, 40}, Range(3, Direction::DOWNTO, 0));
    auto s = a[{2, 1}];
    EXPECT_EQ(s.range().length(), 2U);
    EXPECT_EQ(s[2], 20);
    EXPECT_EQ(s[1], 30);
}

TEST(TestDynArray, SliceMutatesUnderlying) {
    DynArray<int> a({10, 20, 30, 40, 50});
    auto s = a[{1, 3}];
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestDynArray, SliceAssignFromRange) {
    DynArray<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s = std::vector<int>{20, 30, 40};
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[3], 40);
}

TEST(TestDynArray, SliceAssignFromInitializerList) {
    DynArray<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s = {7, 8, 9};
    EXPECT_EQ(a[2], 8);
}

TEST(TestDynArray, SliceAssignWrongLength) {
    DynArray<int> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    EXPECT_THROW((s = std::vector<int>{1, 2, 3, 4}), std::invalid_argument);
    EXPECT_THROW((s = {1, 2}), std::invalid_argument);
}

TEST(TestDynArray, SliceStartOutOfRange) {
    DynArray<int> a({1, 2, 3});
    EXPECT_THROW((void)(a[{99, 100}]), std::invalid_argument);
}

TEST(TestDynArray, SliceEndOutOfRange) {
    DynArray<int> a({1, 2, 3});
    EXPECT_THROW((void)(a[{0, 99}]), std::invalid_argument);
}

TEST(TestDynArray, SliceDirectionMismatch) {
    DynArray<int> a(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    // start=0, end=4 walks against the array's DOWNTO direction.
    EXPECT_THROW((void)(a[{0, 4}]), std::invalid_argument);
}

// -- Null slice corner cases (subsequence validity rule) -------------------
//
// A null range (length 0) is always a valid subsequence, so the slice should
// succeed regardless of bounds or direction.

TEST(TestDynArray, SliceNullDirectionMismatchOK) {
    DynArray<int> a({1, 2, 3, 4, 5});  // Range(0, TO, 4)
    // Range(3, TO, 1) has length 0; the wrong-direction-vs-owner doesn't
    // matter because there are no values to walk.
    auto s = a[{3, Direction::TO, 1}];
    EXPECT_EQ(s.range().length(), 0U);
    EXPECT_EQ(s.begin(), s.end());
}

TEST(TestDynArray, SliceNullOutOfBoundsOK) {
    DynArray<int> a({1, 2, 3, 4, 5});
    // Range(99, TO, 50) has length 0; bounds outside the parent are fine.
    auto s = a[{99, Direction::TO, 50}];
    EXPECT_EQ(s.range().length(), 0U);
}

TEST(TestDynArray, SliceLengthOneDirectionAgnostic) {
    // Length-1 slice doesn't care about direction; only the single value
    // needs to exist in the parent.
    DynArray<int> a({10, 20, 30, 40});  // Range(0, TO, 3)
    auto s = a[{2, Direction::DOWNTO, 2}];
    EXPECT_EQ(s.range().length(), 1U);
    EXPECT_EQ(s[2], 30);
}

TEST(TestDynArray, SliceOfSliceFlattens) {
    DynArray<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    auto s2 = s1[{2, 4}];
    static_assert(
        std::is_same_v<decltype(s2), ArraySlice<DynArray<int>>>,
        "slice-of-slice must flatten"
    );
    EXPECT_EQ(s2[2], 3);
    EXPECT_EQ(s2[4], 5);
}

TEST(TestDynArray, SliceOfSliceStartOutOfRange) {
    DynArray<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    // 99 is outside s1's range.
    EXPECT_THROW((void)(s1[{99, 100}]), std::invalid_argument);
}

TEST(TestDynArray, SliceOfSliceEndOutOfRange) {
    DynArray<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a[{1, 6}];
    // 1 is in s1's range, 99 isn't.
    EXPECT_THROW((void)(s1[{1, 99}]), std::invalid_argument);
}

TEST(TestDynArray, SliceOfSliceDirectionMismatch) {
    DynArray<int> a(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    auto s1 = a[{4, 1}];  // DOWNTO slice over coords 4..1
    // Asking for start=1, end=4 walks against s1's DOWNTO direction.
    EXPECT_THROW((void)(s1[{1, 4}]), std::invalid_argument);
}

// Same error paths but on const DynArray / const ArraySlice; index() and
// slice() are templates, so const and non-const callers instantiate separate
// specializations and each throw needs to be exercised independently.
TEST(TestDynArray, IndexingConstOutOfRange) {
    DynArray<int> const a({1, 2, 3});
    EXPECT_THROW((void)a[100], std::out_of_range);
}

TEST(TestDynArray, SliceConstStartOutOfRange) {
    DynArray<int> const a({1, 2, 3});
    EXPECT_THROW((void)(a[{99, 100}]), std::invalid_argument);
}

TEST(TestDynArray, SliceConstEndOutOfRange) {
    DynArray<int> const a({1, 2, 3});
    EXPECT_THROW((void)(a[{0, 99}]), std::invalid_argument);
}

TEST(TestDynArray, SliceConstDirectionMismatch) {
    DynArray<int> mut(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    DynArray<int> const& a = mut;
    EXPECT_THROW((void)(a[{0, 4}]), std::invalid_argument);
}

TEST(TestDynArray, ConstSliceErrors) {
    DynArray<int> const a({1, 2, 3, 4, 5});
    auto s = a[{0, 4}];
    EXPECT_THROW((void)(s[{99, 100}]), std::invalid_argument);
    EXPECT_THROW((void)(s[{0, 99}]), std::invalid_argument);
}

TEST(TestDynArray, ConstSliceDirectionMismatch) {
    DynArray<int> mut(std::vector<int>{1, 2, 3, 4, 5}, Range(4, Direction::DOWNTO, 0));
    DynArray<int> const& a = mut;
    auto s = a[{4, 0}];
    EXPECT_THROW((void)(s[{0, 4}]), std::invalid_argument);
}

TEST(TestDynArray, ConstSliceOfConstSlice) {
    DynArray<int> const a({1, 2, 3, 4, 5});
    auto outer = a[{0, 4}];
    auto inner = outer[{1, 3}];
    EXPECT_EQ(inner.range().length(), 3U);
    EXPECT_EQ(inner[1], 2);
    EXPECT_EQ(inner[3], 4);
}

TEST(TestDynArray, ConstructLogicFromRange) {
    DynArray<Logic> a(Range(3));
    EXPECT_EQ(a.range().length(), 3U);
}

TEST(TestDynArray, ConstSliceOverConstArray) {
    DynArray<int> const a({10, 20, 30, 40});
    auto s = a[{1, 2}];
    static_assert(std::is_same_v<decltype(s), ArraySlice<DynArray<int> const>>);
    EXPECT_EQ(s[1], 20);
    static_assert(std::is_same_v<decltype(s[1]), int const&>);
}

TEST(TestDynArray, ConstSliceIteration) {
    DynArray<int> const a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    int sum = std::accumulate(s.begin(), s.end(), 0);
    EXPECT_EQ(sum, 2 + 3 + 4);
}

TEST(TestDynArray, ConstSliceOverMutableArrayMutates) {
    // std::span-style const propagation: top-level const on the slice does
    // not restrict element access. The slice's own const-ness only fixes the
    // pointer/range; the underlying ArrayT determines element mutability.
    DynArray<int> a({1, 2, 3, 4});
    auto const cs = a[{0, 3}];  // const ArraySlice<DynArray<int>>
    cs[0] = 99;                 // mutation through const slice
    cs = std::vector<int>{10, 20, 30, 40};
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[3], 40);
}

// -- Equality ---------------------------------------------------------------

TEST(TestDynArray, EqualityValuesAndRange) {
    EXPECT_EQ(DynArray<int>({1, 2, 3, 4}), DynArray<int>({1, 2, 3, 4}));
}

TEST(TestDynArray, InequalityDifferentRange) {
    // Arrays with different ranges have different indexing semantics, so they
    // are not substitutable and must not compare equal.
    DynArray<int> a({1, 2, 3});
    DynArray<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    EXPECT_NE(a, b);
}

TEST(TestDynArray, InequalityDifferentValues) {
    EXPECT_NE(DynArray<int>({1, 2, 3}), DynArray<int>({1, 2, 4}));
}

TEST(TestDynArray, InequalityDifferentLength) {
    EXPECT_NE(DynArray<int>({1, 2, 3}), DynArray<int>({1, 2}));
}

TEST(TestDynArray, EqualityEmptyArrays) {
    DynArray<int> a({});
    DynArray<int> b({});
    EXPECT_EQ(a, b);
}

// -- Hash -------------------------------------------------------------------

TEST(TestDynArray, HashEqualArraysSameRange) {
    std::hash<DynArray<int>> h;
    DynArray<int> a({1, 2, 3, 4});
    DynArray<int> b({1, 2, 3, 4});
    EXPECT_EQ(h(a), h(b));
}

TEST(TestDynArray, HashEmptyArraysWithDifferentBounds) {
    std::hash<DynArray<int>> h;
    DynArray<int> a({});
    DynArray<int> b(std::vector<int>{}, Range(5, Direction::DOWNTO, 8));
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
}

TEST(TestDynArray, HashSingleElementSameLeftDifferentDirection) {
    std::hash<DynArray<int>> h;
    DynArray<int> a({42});  // range: 0 TO 0
    DynArray<int> b(std::vector<int>{42}, Range(0, Direction::DOWNTO, 0));
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
}

TEST(TestDynArray, MultiElementDifferentRangeNotEqual) {
    DynArray<int> a({1, 2, 3});
    DynArray<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    EXPECT_NE(a, b);
}

TEST(TestDynArray, UnorderedSetDistinguishesByRange) {
    DynArray<int> a({1, 2, 3});
    DynArray<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    std::unordered_set<DynArray<int>> s;
    s.insert(a);
    s.insert(b);
    EXPECT_EQ(s.size(), 2U);
}

TEST(TestDynArray, UnorderedSetDeduplicatesEmptyArrays) {
    DynArray<int> a({});
    DynArray<int> b(std::vector<int>{}, Range(5, Direction::DOWNTO, 8));
    std::unordered_set<DynArray<int>> s;
    s.insert(a);
    s.insert(b);
    EXPECT_EQ(s.size(), 1U);
}

// -- Copy semantics ---------------------------------------------------------

TEST(TestDynArray, Copy) {
    DynArray<int> a(std::vector<int>{1, 2, 3, 4}, Range(-2, Direction::TO, 1));
    DynArray<int> b = a;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.range(), b.range());
    b[0] = 99;
    EXPECT_EQ(a[0], 3);  // independent storage
}

TEST(TestDynArray, Move) {
    DynArray<int> a({1, 2, 3, 4});
    DynArray<int> b = std::move(a);
    EXPECT_EQ(b.range().length(), 4U);
    EXPECT_EQ(b[0], 1);
}

TEST(TestDynArray, CopyAssignReplacesRange) {
    DynArray<int> a({1, 2, 3});  // range: 0 TO 2
    DynArray<int> b(std::vector<int>{10, 20, 30, 40}, Range(7, Direction::DOWNTO, 4));
    a = b;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.range(), Range(7, Direction::DOWNTO, 4));
    EXPECT_EQ(a[7], 10);
    EXPECT_EQ(a[4], 40);
}

TEST(TestDynArray, MoveAssignReplacesRange) {
    DynArray<int> a({1, 2, 3});
    DynArray<int> b(std::vector<int>{10, 20, 30, 40}, Range(7, Direction::DOWNTO, 4));
    a = std::move(b);
    EXPECT_EQ(a.range(), Range(7, Direction::DOWNTO, 4));
    EXPECT_EQ(a[7], 10);
    EXPECT_EQ(a[4], 40);
}

// -- Formatter --------------------------------------------------------------

TEST(TestDynArray, FormatterInt) {
    DynArray<int> a({1, 2, 3});
    EXPECT_EQ(std::format("{}", a), "[0 to 2]{1, 2, 3}");
}

TEST(TestDynArray, FormatterEmpty) {
    DynArray<int> a({});
    EXPECT_EQ(std::format("{}", a), "[0 to -1]{}");
}

TEST(TestDynArray, FormatterLogic) {
    DynArray<Logic> a({'0'_l, '1'_l, 'X'_l});
    EXPECT_EQ(std::format("{}", a), "Logic[0 to 2]{0, 1, X}");
}

TEST(TestDynArray, FormatterBit) {
    DynArray<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    EXPECT_EQ(std::format("{}", a), "Bit[0 to 3]{0, 1, 0, 1}");
}

TEST(TestDynArray, FormatterLogicSlice) {
    DynArray<Logic> a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto s = a[Range(1, 2)];
    EXPECT_EQ(std::format("{}", s), "Logic[1 to 2]{1, X}");
}

TEST(TestDynArray, FormatterLogicConstSlice) {
    DynArray<Logic> const a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto s = a[Range(1, 2)];
    EXPECT_EQ(std::format("{}", s), "Logic[1 to 2]{1, X}");
}

TEST(TestDynArray, FormatterBitSlice) {
    DynArray<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    auto s = a[Range(1, 2)];
    EXPECT_EQ(std::format("{}", s), "Bit[1 to 2]{1, 0}");
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
// LCOV_EXCL_BR_STOP

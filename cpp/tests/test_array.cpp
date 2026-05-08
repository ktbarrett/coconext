#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

using namespace coconext::types;

// -- Construction -----------------------------------------------------------

TEST(TestArray, ConstructFromInitializerList) {
    Array<int> a({1, 2, 3, 4});
    EXPECT_EQ(a.range().left(), 0);
    EXPECT_EQ(a.range().right(), 3);
    EXPECT_EQ(a.range().direction(), Direction::TO);
}

TEST(TestArray, ConstructFromInitializerListEmpty) {
    Array<int> a({});
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestArray, ConstructFromVectorMove) {
    std::vector<int> v{1, 2, 3, 4};
    Array<int> a(std::move(v));
    EXPECT_EQ(a.range().length(), 4U);
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[3], 4);
    EXPECT_TRUE(v.empty());  // moved-from
}

TEST(TestArray, ConstructFromVectorMoveEmpty) {
    std::vector<int> v;
    Array<int> a(std::move(v));
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestArray, ConstructFromLength) {
    Array<int> a(static_cast<size_t>(5));
    EXPECT_EQ(a.range().length(), 5U);
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 4));
}

TEST(TestArray, ConstructFromLengthZero) {
    Array<int> a(static_cast<size_t>(0));
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestArray, ConstructFromLengthOverflow) {
    constexpr size_t too_big =
        static_cast<size_t>(std::numeric_limits<int32_t>::max()) + 1;
    EXPECT_THROW(Array<int> a(too_big), std::length_error);
}

TEST(TestArray, ConstructFromRange) {
    Array<int> a(Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range(), Range(-2, Direction::TO, 1));
    EXPECT_EQ(a.range().length(), 4U);
}

TEST(TestArray, ConstructFromInputRangeWithRange) {
    std::vector<int> src{10, 20, 30, 40};
    Array<int> a(src, Range(-2, Direction::TO, 1));
    EXPECT_EQ(a[-2], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestArray, ConstructFromInputRangeWithLength) {
    std::vector<int> src{1, 2, 3};
    Array<int> a(src, static_cast<size_t>(3));
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 2));
}

TEST(TestArray, ConstructFromInputRangeLengthMismatch) {
    std::vector<int> src{1, 2, 3};
    EXPECT_THROW(Array<int> a(src, Range(0, Direction::TO, 7)),
                 std::invalid_argument);
    EXPECT_THROW(Array<int> a(src, static_cast<size_t>(7)),
                 std::invalid_argument);
}

TEST(TestArray, ConstructFromIteratorPairWithRange) {
    std::vector<int> src{1, 2, 3, 4};
    Array<int> a(src.begin(), src.end(), Range(0, Direction::TO, 3));
    EXPECT_EQ(a[2], 3);
}

TEST(TestArray, ConstructFromIteratorPairWithLength) {
    std::vector<int> src{1, 2, 3, 4};
    Array<int> a(src.begin(), src.end(), static_cast<size_t>(4));
    EXPECT_EQ(a.range().length(), 4U);
}

TEST(TestArray, ConstructFromIteratorPairLengthMismatch) {
    std::vector<int> src{1, 2, 3};
    EXPECT_THROW(
        Array<int> a(src.begin(), src.end(), Range(0, Direction::TO, 7)),
        std::invalid_argument);
    EXPECT_THROW(Array<int> a(src.begin(), src.end(), static_cast<size_t>(7)),
                 std::invalid_argument);
}

// -- range() ----------------------------------------------------------------

TEST(TestArray, RangeAccessor) {
    Array<int> a({1, 2, 3});
    EXPECT_EQ(a.range(), Range(0, Direction::TO, 2));
}

// -- Iteration --------------------------------------------------------------

TEST(TestArray, ForwardIteration) {
    Array<int> a({1, 2, 3, 4, 5});
    std::vector<int> seen(a.begin(), a.end());
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(TestArray, ReverseIteration) {
    Array<int> a({1, 2, 3, 4, 5});
    std::vector<int> seen(a.rbegin(), a.rend());
    EXPECT_EQ(seen, (std::vector<int>{5, 4, 3, 2, 1}));
}

TEST(TestArray, IterationConst) {
    const Array<int> a({1, 2, 3});
    int sum = std::accumulate(a.begin(), a.end(), 0);
    EXPECT_EQ(sum, 6);
}

// -- Indexing ---------------------------------------------------------------

TEST(TestArray, IndexingTO) {
    Array<int> a(std::vector<int>{10, 20, 30, 40}, Range(8, Direction::TO, 11));
    EXPECT_EQ(a[8], 10);
    EXPECT_EQ(a[11], 40);
}

TEST(TestArray, IndexingDOWNTO) {
    Array<int> a(std::vector<int>{10, 20, 30, 40},
                 Range(10, Direction::DOWNTO, 7));
    EXPECT_EQ(a[10], 10);
    EXPECT_EQ(a[7], 40);
}

TEST(TestArray, IndexingMutates) {
    Array<int> a({1, 2, 3, 4});
    a[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestArray, IndexingOutOfRange) {
    Array<int> a(std::vector<int>{1, 2, 3}, Range(8, Direction::TO, 10));
    EXPECT_THROW((void)a[0], std::out_of_range);
    EXPECT_THROW((void)a[100], std::out_of_range);
}

TEST(TestArray, IndexingConst) {
    const Array<int> a({1, 2, 3});
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
    static_assert(std::is_same_v<decltype(a[0]), const int&>);
}

// -- Slicing ----------------------------------------------------------------

TEST(TestArray, SliceTO) {
    Array<int> a({1, 2, 3, 4, 5, 6});
    auto s = a(1, 4);
    EXPECT_EQ(s.range(), Range(1, Direction::TO, 4));
    EXPECT_EQ(s.range().length(), 4U);
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s[4], 5);
}

TEST(TestArray, SliceDOWNTO) {
    Array<int> a(std::vector<int>{10, 20, 30, 40},
                 Range(3, Direction::DOWNTO, 0));
    auto s = a(2, 1);
    EXPECT_EQ(s.range().length(), 2U);
    EXPECT_EQ(s[2], 20);
    EXPECT_EQ(s[1], 30);
}

TEST(TestArray, SliceMutatesUnderlying) {
    Array<int> a({10, 20, 30, 40, 50});
    auto s = a(1, 3);
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestArray, SliceAssignFromRange) {
    Array<int> a({1, 2, 3, 4, 5});
    auto s = a(1, 3);
    s = std::vector<int>{20, 30, 40};
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[3], 40);
}

TEST(TestArray, SliceAssignFromInitializerList) {
    Array<int> a({1, 2, 3, 4, 5});
    auto s = a(1, 3);
    s = {7, 8, 9};
    EXPECT_EQ(a[2], 8);
}

TEST(TestArray, SliceAssignWrongLength) {
    Array<int> a({1, 2, 3, 4, 5});
    auto s = a(1, 3);
    EXPECT_THROW((s = std::vector<int>{1, 2, 3, 4}), std::invalid_argument);
    EXPECT_THROW((s = {1, 2}), std::invalid_argument);
}

TEST(TestArray, SliceStartOutOfRange) {
    Array<int> a({1, 2, 3});
    EXPECT_THROW((void)a(99, 100), std::out_of_range);
}

TEST(TestArray, SliceEndOutOfRange) {
    Array<int> a({1, 2, 3});
    EXPECT_THROW((void)a(0, 99), std::out_of_range);
}

TEST(TestArray, SliceDirectionMismatch) {
    Array<int> a(std::vector<int>{1, 2, 3, 4, 5},
                 Range(4, Direction::DOWNTO, 0));
    // start=0, end=4 walks against the array's DOWNTO direction.
    EXPECT_THROW((void)a(0, 4), std::invalid_argument);
}

TEST(TestArray, SliceOfSliceFlattens) {
    Array<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a(1, 6);
    auto s2 = s1(2, 4);
    static_assert(std::is_same_v<decltype(s2), ArraySlice<Array<int>>>,
                  "slice-of-slice must flatten");
    EXPECT_EQ(s2[2], 3);
    EXPECT_EQ(s2[4], 5);
}

TEST(TestArray, SliceOfSliceStartOutOfRange) {
    Array<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a(1, 6);
    // 99 is outside s1's range.
    EXPECT_THROW((void)s1(99, 100), std::out_of_range);
}

TEST(TestArray, SliceOfSliceEndOutOfRange) {
    Array<int> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s1 = a(1, 6);
    // 1 is in s1's range, 99 isn't.
    EXPECT_THROW((void)s1(1, 99), std::out_of_range);
}

TEST(TestArray, SliceOfSliceDirectionMismatch) {
    Array<int> a(std::vector<int>{1, 2, 3, 4, 5},
                 Range(4, Direction::DOWNTO, 0));
    auto s1 = a(4, 1);  // DOWNTO slice over coords 4..1
    // Asking for start=1, end=4 walks against s1's DOWNTO direction.
    EXPECT_THROW((void)s1(1, 4), std::invalid_argument);
}

// Same error paths but on const Array / const ArraySlice; index() and slice()
// are templates, so const and non-const callers instantiate separate
// specializations and each throw needs to be exercised independently.
TEST(TestArray, IndexingConstOutOfRange) {
    const Array<int> a({1, 2, 3});
    EXPECT_THROW((void)a[100], std::out_of_range);
}

TEST(TestArray, SliceConstStartOutOfRange) {
    const Array<int> a({1, 2, 3});
    EXPECT_THROW((void)a(99, 100), std::out_of_range);
}

TEST(TestArray, SliceConstEndOutOfRange) {
    const Array<int> a({1, 2, 3});
    EXPECT_THROW((void)a(0, 99), std::out_of_range);
}

TEST(TestArray, SliceConstDirectionMismatch) {
    Array<int> mut(std::vector<int>{1, 2, 3, 4, 5},
                   Range(4, Direction::DOWNTO, 0));
    const Array<int>& a = mut;
    EXPECT_THROW((void)a(0, 4), std::invalid_argument);
}

TEST(TestArray, ConstSliceErrors) {
    const Array<int> a({1, 2, 3, 4, 5});
    auto s = a(0, 4);
    EXPECT_THROW((void)s(99, 100), std::out_of_range);
    EXPECT_THROW((void)s(0, 99), std::out_of_range);
}

TEST(TestArray, ConstSliceDirectionMismatch) {
    Array<int> mut(std::vector<int>{1, 2, 3, 4, 5},
                   Range(4, Direction::DOWNTO, 0));
    const Array<int>& a = mut;
    auto s = a(4, 0);
    EXPECT_THROW((void)s(0, 4), std::invalid_argument);
}

TEST(TestArray, ConstSliceOfConstSlice) {
    // Success path of ArraySlice<const Array<int>>::operator() — its own
    // template instantiation, separate from the non-const slice case.
    const Array<int> a({1, 2, 3, 4, 5});
    auto outer = a(0, 4);
    auto inner = outer(1, 3);
    EXPECT_EQ(inner.range().length(), 3U);
    EXPECT_EQ(inner[1], 2);
    EXPECT_EQ(inner[3], 4);
}

TEST(TestArray, ConstructLogicLengthOverflow) {
    // length_to_right_ is a static member of each Array<T> instantiation;
    // Array<Logic>'s copy needs its own overflow exercise.
    constexpr size_t too_big =
        static_cast<size_t>(std::numeric_limits<int32_t>::max()) + 1;
    EXPECT_THROW(Array<Logic> a(too_big), std::length_error);
}

TEST(TestArray, ConstructLogicFromLength) {
    // Successful Array<Logic>(size_t) — covers the data_.resize(length) line
    // inside the size_t constructor's Array<Logic> instantiation.
    Array<Logic> a(static_cast<size_t>(3));
    EXPECT_EQ(a.range().length(), 3U);
}

// Cover length_to_right_'s overflow throw via the other constructor paths
// that route through it.
TEST(TestArray, ConstructFromInputRangeWithLengthOverflow) {
    constexpr size_t too_big =
        static_cast<size_t>(std::numeric_limits<int32_t>::max()) + 1;
    std::vector<int> empty;
    EXPECT_THROW(Array<int> a(empty, too_big), std::length_error);
}

TEST(TestArray, ConstructFromIteratorPairWithLengthOverflow) {
    constexpr size_t too_big =
        static_cast<size_t>(std::numeric_limits<int32_t>::max()) + 1;
    std::vector<int> empty;
    EXPECT_THROW(Array<int> a(empty.begin(), empty.end(), too_big),
                 std::length_error);
}

TEST(TestArray, ConstSliceOverConstArray) {
    const Array<int> a({10, 20, 30, 40});
    auto s = a(1, 2);
    static_assert(std::is_same_v<decltype(s), ArraySlice<const Array<int>>>);
    EXPECT_EQ(s[1], 20);
    static_assert(std::is_same_v<decltype(s[1]), const int&>);
}

TEST(TestArray, ConstSliceIteration) {
    const Array<int> a({1, 2, 3, 4, 5});
    auto s = a(1, 3);
    int sum = std::accumulate(s.begin(), s.end(), 0);
    EXPECT_EQ(sum, 2 + 3 + 4);
}

TEST(TestArray, ConstSliceOverMutableArrayMutates) {
    // std::span-style const propagation: top-level const on the slice does
    // not restrict element access. The slice's own const-ness only fixes the
    // pointer/range; the underlying ArrayT determines element mutability.
    Array<int> a({1, 2, 3, 4});
    const auto cs = a(0, 3);  // const ArraySlice<Array<int>>
    cs[0] = 99;               // mutation through const slice
    cs = std::vector<int>{10, 20, 30, 40};
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[3], 40);
}

// -- Equality ---------------------------------------------------------------

TEST(TestArray, EqualityValuesAndRange) {
    EXPECT_EQ(Array<int>({1, 2, 3, 4}), Array<int>({1, 2, 3, 4}));
}

TEST(TestArray, InequalityDifferentRange) {
    // Arrays with different ranges have different indexing semantics, so they
    // are not substitutable and must not compare equal.
    Array<int> a({1, 2, 3});
    Array<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    EXPECT_NE(a, b);
}

TEST(TestArray, InequalityDifferentValues) {
    EXPECT_NE(Array<int>({1, 2, 3}), Array<int>({1, 2, 4}));
}

TEST(TestArray, InequalityDifferentLength) {
    EXPECT_NE(Array<int>({1, 2, 3}), Array<int>({1, 2}));
}

TEST(TestArray, EqualityEmptyArrays) {
    Array<int> a({});
    Array<int> b({});
    EXPECT_EQ(a, b);
}

// -- Hash -------------------------------------------------------------------
//
// std::hash<Array> must agree with operator==, which considers both the
// elements and the range (since arrays with different ranges have different
// indexing semantics and are not substitutable). The exception is when the
// ranges themselves compare equal under Range's own equality (length 0 always,
// length 1 with same left regardless of direction).

TEST(TestArray, HashEqualArraysSameRange) {
    std::hash<Array<int>> h;
    Array<int> a({1, 2, 3, 4});
    Array<int> b({1, 2, 3, 4});
    EXPECT_EQ(h(a), h(b));
}

TEST(TestArray, HashEmptyArraysWithDifferentBounds) {
    // All length-0 ranges are equal (Range length-0 special case), so empty
    // arrays with any range bounds compare equal and must hash equal.
    std::hash<Array<int>> h;
    Array<int> a({});
    Array<int> b(std::vector<int>{}, Range(5, Direction::DOWNTO, 8));
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
}

TEST(TestArray, HashSingleElementSameLeftDifferentDirection) {
    // Length-1 ranges with the same left compare equal regardless of
    // direction (Range length-1 special case), so the arrays compare equal
    // and must hash equal.
    std::hash<Array<int>> h;
    Array<int> a({42});  // range: 0 TO 0
    Array<int> b(std::vector<int>{42}, Range(0, Direction::DOWNTO, 0));
    EXPECT_EQ(a, b);
    EXPECT_EQ(h(a), h(b));
}

TEST(TestArray, MultiElementDifferentRangeNotEqual) {
    // For length >= 2, range direction and bounds matter for indexing.
    Array<int> a({1, 2, 3});
    Array<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    EXPECT_NE(a, b);
}

TEST(TestArray, UnorderedSetDistinguishesByRange) {
    Array<int> a({1, 2, 3});
    Array<int> b(std::vector<int>{1, 2, 3}, Range(10, Direction::DOWNTO, 8));
    std::unordered_set<Array<int>> s;
    s.insert(a);
    s.insert(b);
    EXPECT_EQ(s.size(), 2U);
}

TEST(TestArray, UnorderedSetDeduplicatesEmptyArrays) {
    Array<int> a({});
    Array<int> b(std::vector<int>{}, Range(5, Direction::DOWNTO, 8));
    std::unordered_set<Array<int>> s;
    s.insert(a);
    s.insert(b);
    EXPECT_EQ(s.size(), 1U);
}

// -- Copy semantics ---------------------------------------------------------

TEST(TestArray, Copy) {
    Array<int> a(std::vector<int>{1, 2, 3, 4}, Range(-2, Direction::TO, 1));
    Array<int> b = a;
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.range(), b.range());
    b[0] = 99;
    EXPECT_EQ(a[0], 3);  // independent storage
}

TEST(TestArray, Move) {
    Array<int> a({1, 2, 3, 4});
    Array<int> b = std::move(a);
    EXPECT_EQ(b.range().length(), 4U);
    EXPECT_EQ(b[0], 1);
}

// -- to_string --------------------------------------------------------------

TEST(TestArray, ToStringInt) {
    Array<int> a({1, 2, 3});
    EXPECT_EQ(to_string(a), "Array([1, 2, 3], Range(0, 'to', 2))");
}

TEST(TestArray, ToStringEmpty) {
    Array<int> a({});
    EXPECT_EQ(to_string(a), "Array([], Range(0, 'to', -1))");
}

TEST(TestArray, ToStringLogic) {
    // Exercises the ADL path in to_string (Logic's to_string is in
    // coconext::types, not std).
    Array<Logic> a({'0'_l, '1'_l, 'X'_l});
    auto s = to_string(a);
    EXPECT_NE(s.find("0"), std::string::npos);
    EXPECT_NE(s.find("X"), std::string::npos);
}

// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <format>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

using namespace coconext::types;

// -- Construction -----------------------------------------------------------

TEST(TestStaticArray, DefaultConstructZeroInit) {
    Array<int, Range{0, Direction::TO, 3}> a;
    EXPECT_EQ(a[0], 0);
    EXPECT_EQ(a[3], 0);
}

TEST(TestStaticArray, ConstructFromInitializerList) {
    Array<int, Range{0, Direction::TO, 3}> a({1, 2, 3, 4});
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[3], 4);
}

TEST(TestStaticArray, ConstructFromInitializerListLengthMismatch) {
    using A = Array<int, Range{0, Direction::TO, 3}>;
    EXPECT_THROW(A a({1, 2, 3}), std::invalid_argument);
    EXPECT_THROW(A a({1, 2, 3, 4, 5}), std::invalid_argument);
}

TEST(TestStaticArray, ConstructFromSizedRange) {
    std::vector<int> src = {10, 20, 30, 40};
    Array<int, Range{-2, Direction::TO, 1}> a(src);
    EXPECT_EQ(a[-2], 10);
    EXPECT_EQ(a[1], 40);
}

TEST(TestStaticArray, ConstructFromSizedRangeLengthMismatch) {
    std::vector<int> src = {1, 2, 3};
    using A = Array<int, Range{0, Direction::TO, 3}>;
    EXPECT_THROW(A a(src), std::invalid_argument);
}

TEST(TestStaticArray, CopyAndMove) {
    Array<int, Range{0, Direction::TO, 3}> a({1, 2, 3, 4});
    Array<int, Range{0, Direction::TO, 3}> b = a;
    EXPECT_EQ(a, b);
    Array<int, Range{0, Direction::TO, 3}> c = std::move(a);
    EXPECT_EQ(b, c);
}

// -- Range / iteration ------------------------------------------------------

TEST(TestStaticArray, RangeAccessor) {
    Array<int, Range{2, Direction::TO, 5}> a({1, 2, 3, 4});
    EXPECT_EQ(a.range(), (Range{2, Direction::TO, 5}));
    static_assert(decltype(a)::static_range == Range{2, Direction::TO, 5});
}

TEST(TestStaticArray, ForwardIteration) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    int sum = 0;
    for (auto v : a) {
        sum += v;
    }
    EXPECT_EQ(sum, 15);
}

TEST(TestStaticArray, ReverseIteration) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    std::vector<int> rev(a.rbegin(), a.rend());
    EXPECT_EQ(rev, (std::vector<int>{5, 4, 3, 2, 1}));
}

TEST(TestStaticArray, IterationConst) {
    Array<int, Range{0, Direction::TO, 2}> const a({1, 2, 3});
    int sum = 0;
    for (auto v : a) {
        sum += v;
    }
    EXPECT_EQ(sum, 6);
}

// -- Indexing ---------------------------------------------------------------

TEST(TestStaticArray, IndexingTO) {
    Array<int, Range{8, Direction::TO, 11}> a({10, 20, 30, 40});
    EXPECT_EQ(a[8], 10);
    EXPECT_EQ(a[11], 40);
}

TEST(TestStaticArray, IndexingDOWNTO) {
    Array<int, Range{10, Direction::DOWNTO, 7}> a({10, 20, 30, 40});
    EXPECT_EQ(a[10], 10);
    EXPECT_EQ(a[7], 40);
}

TEST(TestStaticArray, IndexingMutates) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    a[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestStaticArray, IndexingOutOfRange) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    EXPECT_THROW((void)a[-1], std::out_of_range);
    EXPECT_THROW((void)a[5], std::out_of_range);
}

TEST(TestStaticArray, IndexingDOWNTOOutOfRange) {
    Array<int, Range{4, Direction::DOWNTO, 0}> a({1, 2, 3, 4, 5});
    EXPECT_THROW((void)a[-1], std::out_of_range);
    EXPECT_THROW((void)a[5], std::out_of_range);
}

TEST(TestStaticArray, IndexingConst) {
    Array<int, Range{0, Direction::TO, 2}> const a({1, 2, 3});
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[2], 3);
    static_assert(std::is_same_v<decltype(a[0]), int const&>);
}

// -- Slicing (runtime, returns ArraySlice) ------------------------------

TEST(TestStaticArray, SliceTO) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a[{1, 4}];
    EXPECT_EQ(s.range(), (Range{1, Direction::TO, 4}));
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s[4], 5);
}

TEST(TestStaticArray, SliceDOWNTO) {
    Array<int, Range{3, Direction::DOWNTO, 0}> a({10, 20, 30, 40});
    auto s = a[{2, 1}];
    EXPECT_EQ(s.range(), (Range{2, Direction::DOWNTO, 1}));
    EXPECT_EQ(s[2], 20);
    EXPECT_EQ(s[1], 30);
}

TEST(TestStaticArray, SliceMutatesUnderlying) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestStaticArray, SliceAssignFromRange) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s = std::vector<int>{20, 30, 40};
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[3], 40);
}

TEST(TestStaticArray, SliceAssignFromInitializerList) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    s = {7, 8, 9};
    EXPECT_EQ(a[1], 7);
    EXPECT_EQ(a[3], 9);
}

TEST(TestStaticArray, SliceAssignWrongLength) {
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    auto s = a[{0, 2}];
    EXPECT_THROW((s = std::vector<int>{1, 2}), std::invalid_argument);
    EXPECT_THROW((s = {1, 2}), std::invalid_argument);
}

TEST(TestStaticArray, SliceStartOutOfRange) {
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    EXPECT_THROW((void)(a[{99, 1}]), std::invalid_argument);
}

TEST(TestStaticArray, SliceEndOutOfRange) {
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    EXPECT_THROW((void)(a[{0, 99}]), std::invalid_argument);
}

TEST(TestStaticArray, SliceDirectionMismatch) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    EXPECT_THROW((void)(a[{3, Direction::DOWNTO, 1}]), std::invalid_argument);
}

TEST(TestStaticArray, SliceNullOutOfBoundsOK) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a[{99, Direction::TO, 50}];  // length 0
    EXPECT_EQ(s.range().length(), 0u);
    EXPECT_EQ(s.begin(), s.end());
}

TEST(TestStaticArray, SliceLengthOneDirectionAgnostic) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a[{2, Direction::DOWNTO, 2}];
    EXPECT_EQ(s.range().length(), 1u);
    EXPECT_EQ(s[2], 3);
}

TEST(TestStaticArray, SliceOfSliceFlattens) {
    Array<int, Range{0, Direction::TO, 6}> a({1, 2, 3, 4, 5, 6, 7});
    auto s1 = a[{1, 6}];
    auto s2 = s1[{2, 4}];
    EXPECT_EQ(s2.range(), (Range{2, Direction::TO, 4}));
    EXPECT_EQ(s2[2], 3);
    EXPECT_EQ(s2[4], 5);
}

TEST(TestStaticArray, SliceOfSliceErrors) {
    Array<int, Range{0, Direction::TO, 6}> a({1, 2, 3, 4, 5, 6, 7});
    auto s1 = a[{1, 4}];
    EXPECT_THROW((void)(s1[{0, 2}]), std::invalid_argument);
    EXPECT_THROW((void)(s1[{1, 5}]), std::invalid_argument);
    EXPECT_THROW((void)(s1[{4, Direction::DOWNTO, 1}]), std::invalid_argument);
}

TEST(TestStaticArray, ConstSliceOverConstArray) {
    Array<int, Range{0, Direction::TO, 4}> const a({1, 2, 3, 4, 5});
    auto s = a[{1, 3}];
    static_assert(std::is_same_v<decltype(s[1]), int const&>);
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s[3], 4);
}

// -- Equality ---------------------------------------------------------------

TEST(TestStaticArray, EqualityValues) {
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    Array<int, Range{0, Direction::TO, 2}> b({1, 2, 3});
    EXPECT_EQ(a, b);
}

TEST(TestStaticArray, InequalityDifferentValues) {
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    Array<int, Range{0, Direction::TO, 2}> b({1, 2, 9});
    EXPECT_FALSE(a == b);
}

// -- Hashing ----------------------------------------------------------------

TEST(TestStaticArray, HashEqualArrays) {
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    Array<int, Range{0, Direction::TO, 2}> b({1, 2, 3});
    using H = std::hash<Array<int, Range{0, Direction::TO, 2}>>;
    EXPECT_EQ(H{}(a), H{}(b));
}

TEST(TestStaticArray, UnorderedSetByValue) {
    std::unordered_set<Array<int, Range{0, Direction::TO, 2}>> s;
    s.insert(Array<int, Range{0, Direction::TO, 2}>({1, 2, 3}));
    s.insert(Array<int, Range{0, Direction::TO, 2}>({1, 2, 3}));
    EXPECT_EQ(s.size(), 1u);
}

TEST(TestStaticArray, HashDistinctAcrossElementType) {
    // Element-equivalent values of Array<int, R> and Array<long, R> are
    // distinct types with no cross-type equality; their hashes must differ
    // so a downstream type added to the family doesn't inherit a collision.
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    Array<long, Range{0, Direction::TO, 2}> b({1L, 2L, 3L});
    auto ha = std::hash<decltype(a)>{}(a);
    auto hb = std::hash<decltype(b)>{}(b);
    EXPECT_NE(ha, hb);
}

TEST(TestStaticArray, HashDistinctAcrossRange) {
    // Different static ranges instantiate different types; their hashes
    // must differ even when the element sequence matches.
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    Array<int, Range{2, Direction::DOWNTO, 0}> b({1, 2, 3});
    auto ha = std::hash<decltype(a)>{}(a);
    auto hb = std::hash<decltype(b)>{}(b);
    EXPECT_NE(ha, hb);
}

// -- Formatter --------------------------------------------------------------

TEST(TestStaticArray, FormatterInt) {
    Array<int, Range{0, Direction::TO, 2}> a({1, 2, 3});
    EXPECT_EQ(std::format("{}", a), "Array[0 to 2]{1, 2, 3}");
}

TEST(TestStaticArray, FormatterEmpty) {
    Array<int, Range{0, Direction::TO, -1}> a;
    EXPECT_EQ(std::format("{}", a), "Array[0 to -1]{}");
}

TEST(TestStaticArray, FormatterLogic) {
    Array<Logic, Range{0, Direction::TO, 2}> a({'0'_l, '1'_l, 'X'_l});
    EXPECT_EQ(std::format("{}", a), "LogicArray[0 to 2]{\"01X\"}");
}

TEST(TestStaticArray, FormatterBit) {
    Array<Bit, Range{0, Direction::TO, 3}> a({'0'_b, '1'_b, '0'_b, '1'_b});
    EXPECT_EQ(std::format("{}", a), "BitArray[0 to 3]{\"0101\"}");
}

TEST(TestStaticArray, FormatterLogicRuntimeSlice) {
    Array<Logic, Range{0, Direction::TO, 3}> a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto s = a[Range{1, 2}];
    EXPECT_EQ(std::format("{}", s), "LogicArraySlice[1 to 2]{\"1X\"}");
}

TEST(TestStaticArray, FormatterLogicRuntimeSliceConst) {
    Array<Logic, Range{0, Direction::TO, 3}> const a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto s = a[Range{1, 2}];
    EXPECT_EQ(std::format("{}", s), "LogicArraySlice[1 to 2]{\"1X\"}");
}

TEST(TestStaticArray, FormatterBitRuntimeSlice) {
    Array<Bit, Range{0, Direction::TO, 3}> a({'0'_b, '1'_b, '0'_b, '1'_b});
    auto s = a[Range{1, 2}];
    EXPECT_EQ(std::format("{}", s), "BitArraySlice[1 to 2]{\"10\"}");
}

TEST(TestStaticArray, FormatterBitRuntimeSliceConst) {
    Array<Bit, Range{0, Direction::TO, 3}> const a({'0'_b, '1'_b, '0'_b, '1'_b});
    auto s = a[Range{1, 2}];
    EXPECT_EQ(std::format("{}", s), "BitArraySlice[1 to 2]{\"10\"}");
}

// -- Static sub-slice (compile-time-bounded) -------------------------------

TEST(TestStaticArrayStaticSlice, SliceHappyPath) {
    Array<int, Range{0, Direction::TO, 4}> a({10, 20, 30, 40, 50});
    auto s = a.slice<Range{1, 3}>();
    using AT = Array<int, Range{0, Direction::TO, 4}>;
    static_assert(
        std::is_same_v<decltype(s), StaticArraySlice<AT, Range{1, 3}>>,
        "Array::slice<R>() must return StaticArraySlice<Array, R>"
    );
    static_assert(decltype(s)::static_range == Range{1, Direction::TO, 3});
    EXPECT_EQ(s.range().length(), 3U);
    EXPECT_EQ(s[1], 20);
    EXPECT_EQ(s[3], 40);
}

TEST(TestStaticArrayStaticSlice, SliceMutatesUnderlying) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestStaticArrayStaticSlice, SliceAssignFromRange) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s = std::vector<int>{20, 30, 40};
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[3], 40);
}

TEST(TestStaticArrayStaticSlice, SliceAssignFromInitializerList) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    s = {7, 8, 9};
    EXPECT_EQ(a[2], 8);
}

TEST(TestStaticArrayStaticSlice, SliceConstReturnsConstSlice) {
    Array<int, Range{0, Direction::TO, 3}> const a({10, 20, 30, 40});
    auto s = a.slice<Range{1, 2}>();
    using AT = Array<int, Range{0, Direction::TO, 3}>;
    static_assert(std::is_same_v<decltype(s), StaticArraySlice<AT const, Range{1, 2}>>);
    static_assert(std::is_same_v<decltype(s[1]), int const&>);
    EXPECT_EQ(s[1], 20);
    EXPECT_EQ(s[2], 30);
}

TEST(TestStaticArrayStaticSlice, IndexingOutOfRange) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{1, 3}>();
    EXPECT_THROW((void)s[0], std::out_of_range);
    EXPECT_THROW((void)s[4], std::out_of_range);
}

TEST(TestStaticArrayStaticSlice, RuntimeSubSliceFlattensToDyn) {
    Array<int, Range{0, Direction::TO, 5}> a({1, 2, 3, 4, 5, 6});
    auto s = a.slice<Range{1, 4}>();
    auto sub = s[Range{2, 3}];
    using AT = Array<int, Range{0, Direction::TO, 5}>;
    static_assert(
        std::is_same_v<decltype(sub), ArraySlice<AT>>,
        "runtime sub-slice of a static slice must flatten to ArraySlice"
    );
    EXPECT_EQ(sub[2], 3);
    EXPECT_EQ(sub[3], 4);
}

TEST(TestStaticArrayStaticSlice, StaticSubSliceFlattensToSameOwner) {
    Array<int, Range{0, Direction::TO, 7}> a({1, 2, 3, 4, 5, 6, 7, 8});
    auto s = a.slice<Range{1, 6}>();
    auto sub = s.slice<Range{2, 4}>();
    using AT = Array<int, Range{0, Direction::TO, 7}>;
    static_assert(
        std::is_same_v<decltype(sub), StaticArraySlice<AT, Range{2, 4}>>,
        "static sub-slice flattens to StaticArraySlice<owner, R2>, not nested"
    );
    EXPECT_EQ(sub[2], 3);
    EXPECT_EQ(sub[4], 5);
}

TEST(TestStaticArrayStaticSlice, NullSliceBoundsOutsideParentOK) {
    Array<int, Range{0, Direction::TO, 4}> a({1, 2, 3, 4, 5});
    // Length 0; static_assert(is_subsequence(...)) accepts null ranges with
    // any bounds.
    auto s = a.slice<Range{99, Direction::TO, 50}>();
    EXPECT_EQ(s.range().length(), 0U);
    EXPECT_EQ(s.begin(), s.end());
}

// Exercises the DOWNTO branch of StaticArraySlice::begin()'s static-parent fast
// path. The TO branch is covered by the other tests in this suite.
TEST(TestStaticArrayStaticSlice, SliceDOWNTOParent) {
    Array<int, Range{10, Direction::DOWNTO, 6}> a({1, 2, 3, 4, 5});
    auto s = a.slice<Range{9, Direction::DOWNTO, 7}>();
    EXPECT_EQ(s.range().length(), 3U);
    EXPECT_EQ(s[9], 2);
    EXPECT_EQ(s[8], 3);
    EXPECT_EQ(s[7], 4);
    s[8] = 99;
    EXPECT_EQ(a[8], 99);
}

// -- ArraySlice::slice<R> over a static Array ---------------------------

TEST(TestStaticArray, DynSliceStaticSubSliceFlattens) {
    Array<int, Range{0, Direction::TO, 5}> a({10, 20, 30, 40, 50, 60});
    auto dyn = a[Range{1, 4}];
    auto s = dyn.slice<Range{2, 3}>();
    using AT = Array<int, Range{0, Direction::TO, 5}>;
    static_assert(
        std::is_same_v<decltype(s), StaticArraySlice<AT, Range{2, 3}>>,
        "static sub-slice of a ArraySlice over a static Array flattens to "
        "StaticArraySlice<owner, R>"
    );
    EXPECT_EQ(s[2], 30);
    EXPECT_EQ(s[3], 40);
    s[2] = 99;
    EXPECT_EQ(a[2], 99);
}

// -- index<I>() compile-time-checked element access ------------------------
//
// On types with a compile-time range (Array, StaticArraySlice), index<I>()
// static_asserts that I is within the range and then delegates to
// operator[](I). This catches out-of-range mistakes at the call site rather
// than at runtime, mirroring slice<R>()'s compile-time subsequence check.

TEST(TestStaticArray, IndexOfStaticArrayTO) {
    Array<int, Range{0, Direction::TO, 3}> a({10, 20, 30, 40});
    EXPECT_EQ(a.index<0>(), 10);
    EXPECT_EQ(a.index<3>(), 40);
    a.index<2>() = 99;
    EXPECT_EQ(a[2], 99);
}

TEST(TestStaticArray, IndexOfStaticArrayDOWNTO) {
    Array<int, Range{3, Direction::DOWNTO, 0}> a({10, 20, 30, 40});
    // DOWNTO: a[3] is the first element, a[0] is the last.
    EXPECT_EQ(a.index<3>(), 10);
    EXPECT_EQ(a.index<0>(), 40);
}

TEST(TestStaticArray, IndexOfConstStaticArray) {
    Array<int, Range{0, Direction::TO, 2}> const a({10, 20, 30});
    int const& r = a.index<1>();
    EXPECT_EQ(r, 20);
    static_assert(std::is_same_v<decltype(a.index<1>()), int const&>);
}

TEST(TestStaticArray, IndexOfStaticArraySlice) {
    Array<int, Range{0, Direction::TO, 5}> a({10, 20, 30, 40, 50, 60});
    auto s = a.slice<Range{2, 4}>();
    EXPECT_EQ(s.index<2>(), 30);
    EXPECT_EQ(s.index<4>(), 50);
    s.index<3>() = 99;
    EXPECT_EQ(a[3], 99);
}

// Compile-time bounds check: uncommenting any of these should produce a
// static_assert "index is out of range" failure. Kept here as a referenced
// reminder rather than as an enabled test (no C++ idiom for testing for a
// static_assert firing at compile time).
//   Array<int, Range{0, Direction::TO, 3}> a{};
//   a.index<4>();                                 // out of range high
//   a.index<-1>();                                // out of range low
//   a.slice<Range{0, 2}>().index<3>();            // out of slice range
// LCOV_EXCL_BR_STOP

// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <type_traits>
#include <vector>

using namespace coconext::types;

// -- reverse(Range) ---------------------------------------------------------

TEST(TestReverseRange, FlipsDowntoToTo) {
    constexpr Range r{7, Direction::DOWNTO, 0};
    constexpr Range rr = reverse(r);
    EXPECT_EQ(rr.left, 0);
    EXPECT_EQ(rr.right, 7);
    EXPECT_EQ(rr.direction, Direction::TO);
    EXPECT_EQ(rr.length(), 8U);
}

TEST(TestReverseRange, FlipsToToDownto) {
    constexpr Range r{-4, Direction::TO, 3};
    constexpr Range rr = reverse(r);
    EXPECT_EQ(rr.left, 3);
    EXPECT_EQ(rr.right, -4);
    EXPECT_EQ(rr.direction, Direction::DOWNTO);
    EXPECT_EQ(rr.length(), 8U);
}

TEST(TestReverseRange, ReverseIsInvolutive) {
    constexpr Range r{3, Direction::DOWNTO, -4};
    constexpr Range rrr = reverse(reverse(r));
    EXPECT_EQ(rrr.left, r.left);
    EXPECT_EQ(rrr.right, r.right);
    EXPECT_EQ(rrr.direction, r.direction);
}

TEST(TestReverseRange, LengthOne) {
    constexpr Range r{5, Direction::DOWNTO, 5};
    constexpr Range rr = reverse(r);
    EXPECT_EQ(rr.left, 5);
    EXPECT_EQ(rr.right, 5);
    EXPECT_EQ(rr.direction, Direction::TO);
    EXPECT_EQ(rr.length(), 1U);
}

TEST(TestReverseRange, ZeroLength) {
    // A zero-length range has left/right in the "wrong" order relative to
    // direction; reverse still flips both endpoints and the direction.
    constexpr Range r{0, Direction::TO, -1};
    constexpr Range rr = reverse(r);
    EXPECT_EQ(rr.left, -1);
    EXPECT_EQ(rr.right, 0);
    EXPECT_EQ(rr.direction, Direction::DOWNTO);
    EXPECT_EQ(rr.length(), 0U);
}

// -- reverse(Array) ---------------------------------------------------------

TEST(TestReverseArray, DowntoToTo) {
    Array<int, Range{3, Direction::DOWNTO, 0}> a{10, 20, 30, 40};
    auto r = reverse(a);
    static_assert(
        std::is_same_v<decltype(r), detail::Array<int, Range{0, Direction::TO, 3}>>
    );
    EXPECT_EQ(r.range(), (Range{0, Direction::TO, 3}));
    EXPECT_EQ(r[0], 40);
    EXPECT_EQ(r[1], 30);
    EXPECT_EQ(r[2], 20);
    EXPECT_EQ(r[3], 10);
}

TEST(TestReverseArray, ToToDownto) {
    Array<int, Range{0, Direction::TO, 3}> a{1, 2, 3, 4};
    auto r = reverse(a);
    static_assert(
        std::is_same_v<decltype(r), detail::Array<int, Range{3, Direction::DOWNTO, 0}>>
    );
    // Reverse preserves value-at-HDL-position: both ranges cover positions
    // {0,1,2,3}, and a[i] == r[i] for each i. What changes is iteration
    // order (and underlying storage layout).
    EXPECT_EQ(r[0], 1);
    EXPECT_EQ(r[1], 2);
    EXPECT_EQ(r[2], 3);
    EXPECT_EQ(r[3], 4);
    std::vector<int> const it(r.begin(), r.end());
    EXPECT_EQ(it, (std::vector<int>{4, 3, 2, 1}));
}

TEST(TestReverseArray, SingleElement) {
    Array<int, Range{5, Direction::DOWNTO, 5}> a{42};
    auto r = reverse(a);
    EXPECT_EQ(r.range().direction, Direction::TO);
    EXPECT_EQ(r[5], 42);
}

TEST(TestReverseArray, Involutive) {
    Array<int, Range{7, Direction::DOWNTO, 0}> a{1, 2, 3, 4, 5, 6, 7, 8};
    auto rr = reverse(reverse(a));
    static_assert(std::is_same_v<decltype(rr), decltype(a)>);
    EXPECT_EQ(rr, a);
}

// -- reverse(Vector) --------------------------------------------------------

TEST(TestReverseVector, DowntoToTo) {
    Vector<int> v({10, 20, 30, 40}, Range{3, Direction::DOWNTO, 0});
    auto r = reverse(v);
    EXPECT_EQ(r.range(), (Range{0, Direction::TO, 3}));
    EXPECT_EQ(r[0], 40);
    EXPECT_EQ(r[3], 10);
}

TEST(TestReverseVector, ToToDownto) {
    Vector<int> v({1, 2, 3, 4}, Range{0, Direction::TO, 3});
    auto r = reverse(v);
    EXPECT_EQ(r.range(), (Range{3, Direction::DOWNTO, 0}));
    // Value-at-HDL-position invariant; iteration order flipped.
    EXPECT_EQ(r[0], 1);
    EXPECT_EQ(r[3], 4);
    std::vector<int> const it(r.begin(), r.end());
    EXPECT_EQ(it, (std::vector<int>{4, 3, 2, 1}));
}

TEST(TestReverseVector, Involutive) {
    Vector<int> v({1, 2, 3, 4, 5}, Range{4, Direction::DOWNTO, 0});
    auto rr = reverse(reverse(v));
    EXPECT_EQ(rr, v);
}

TEST(TestReverseVector, Empty) {
    Vector<int> v(Range{0, Direction::TO, -1});
    auto r = reverse(v);
    EXPECT_EQ(r.range().length(), 0U);
    EXPECT_EQ(r.range().direction, Direction::DOWNTO);
}

// -- reverse(ArraySlice) ----------------------------------------------------

TEST(TestReverseArraySlice, ReturnsVector) {
    Array<int, Range{7, Direction::DOWNTO, 0}> a{1, 2, 3, 4, 5, 6, 7, 8};
    auto s = a[Range{5, Direction::DOWNTO, 2}];  // elements at HDL idx 5..2: {3,4,5,6}
    auto r = reverse(s);
    static_assert(std::is_same_v<decltype(r), Vector<int>>);
    EXPECT_EQ(r.range(), (Range{2, Direction::TO, 5}));
    EXPECT_EQ(r[2], 6);
    EXPECT_EQ(r[3], 5);
    EXPECT_EQ(r[4], 4);
    EXPECT_EQ(r[5], 3);
}

TEST(TestReverseArraySlice, FromVectorSlice) {
    Vector<int> v({10, 20, 30, 40}, Range{0, Direction::TO, 3});
    auto s = v[Range{1, Direction::TO, 2}];
    auto r = reverse(s);
    EXPECT_EQ(r.range(), (Range{2, Direction::DOWNTO, 1}));
    EXPECT_EQ(r[2], 30);
    EXPECT_EQ(r[1], 20);
}

// -- reverse(StaticArraySlice) ----------------------------------------------

TEST(TestReverseStaticArraySlice, PreservesStaticRange) {
    Array<int, Range{7, Direction::DOWNTO, 0}> a{1, 2, 3, 4, 5, 6, 7, 8};
    auto s = a.slice<Range{5, Direction::DOWNTO, 2}>();
    auto r = reverse(s);
    static_assert(
        std::is_same_v<decltype(r), detail::Array<int, Range{2, Direction::TO, 5}>>
    );
    EXPECT_EQ(r[2], 6);
    EXPECT_EQ(r[5], 3);
}

// -- Cross-type: reverse on LogicArray and BitArray (aliases for Array) ----

TEST(TestReverseArray, BitArray) {
    BitArray<4> a{'1'_b, '0'_b, '1'_b, '0'_b};
    auto r = reverse(a);
    // BitArray<4> is detail::Array<Bit, {3 DOWNTO 0}>; reverse -> {0 TO 3}.
    EXPECT_EQ(r.range().direction, Direction::TO);
    EXPECT_EQ(r[0], '0'_b);
    EXPECT_EQ(r[1], '1'_b);
    EXPECT_EQ(r[2], '0'_b);
    EXPECT_EQ(r[3], '1'_b);
}

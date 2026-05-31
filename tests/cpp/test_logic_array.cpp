// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <algorithm>
#include <coconext/types.hpp>
#include <stdexcept>
#include <vector>

using namespace coconext::types;

// -- Find -------------------------------------------------------------------

TEST(TestLogicArray, FindLogicElement) {
    auto a = "01XZ"_l;
    auto it = std::find(a.begin(), a.end(), 'X'_l);
    ASSERT_NE(it, a.end());
    EXPECT_EQ(*it, 'X'_l);
}

// -- Bitwise array operations: AND ------------------------------------------

TEST(TestLogicArray, AndLogic) {
    DynArray<Logic> a({'1'_l, '0'_l, 'X'_l, '1'_l});
    DynArray<Logic> b({'1'_l, '1'_l, '1'_l, '0'_l});
    auto c = a & b;
    static_assert(std::is_same_v<decltype(c), DynArray<Logic>>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '0'_l);
}

// -- Bitwise array operations: OR -------------------------------------------

TEST(TestLogicArray, OrLogic) {
    DynArray<Logic> a({'1'_l, '0'_l, 'X'_l, '0'_l});
    DynArray<Logic> b({'0'_l, '0'_l, '1'_l, '0'_l});
    auto c = a | b;
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

// -- Bitwise array operations: XOR ------------------------------------------

TEST(TestLogicArray, XorLogic) {
    DynArray<Logic> a({'1'_l, '0'_l, '1'_l, '0'_l});
    DynArray<Logic> b({'1'_l, '1'_l, '0'_l, '0'_l});
    auto c = a ^ b;
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

// -- Bitwise array operations: NOT ------------------------------------------

TEST(TestLogicArray, NotLogic) {
    DynArray<Logic> a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto c = ~a;
    static_assert(std::is_same_v<decltype(c), DynArray<Logic>>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

TEST(TestLogicArray, NotBit) {
    DynArray<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    auto c = ~a;
    static_assert(std::is_same_v<decltype(c), DynArray<Bit>>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_b);
    EXPECT_EQ(c[2], '0'_b);
    EXPECT_EQ(c[1], '1'_b);
    EXPECT_EQ(c[0], '0'_b);
}

// -- Bitwise operations: edge cases -----------------------------------------

TEST(TestLogicArray, BitwiseLengthMismatch) {
    DynArray<Logic> a({'0'_l, '1'_l});
    DynArray<Logic> b({'0'_l, '1'_l, 'X'_l});
    EXPECT_THROW((void)(a & b), std::invalid_argument);
    EXPECT_THROW((void)(a | b), std::invalid_argument);
    EXPECT_THROW((void)(a ^ b), std::invalid_argument);
}

TEST(TestLogicArray, BitwiseEmpty) {
    DynArray<Logic> a({});
    DynArray<Logic> b({});
    auto c = a & b;
    EXPECT_EQ(c.range().length(), 0U);
}

TEST(TestLogicArray, BitwiseResultRange) {
    DynArray<Logic> a({'0'_l, '1'_l, '1'_l}, Range(5, Direction::DOWNTO, 3));
    DynArray<Logic> b({'1'_l, '1'_l, '0'_l});
    auto c = a & b;
    EXPECT_EQ(c.range(), Range(2, Direction::DOWNTO, 0));
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestLogicArray, BitwiseOnSlice) {
    DynArray<Logic> a({'0'_l, '1'_l, '1'_l, '0'_l});
    DynArray<Logic> b({'1'_l, '0'_l});
    auto s = a[{1, 2}];
    auto c = s & b;
    static_assert(std::is_same_v<decltype(c), DynArray<Logic>>);
    EXPECT_EQ(c.range(), Range(1, Direction::DOWNTO, 0));
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestLogicArray, BitwiseOnStaticArraySlice) {
    Array<Logic, 4> a({'0'_l, '1'_l, '1'_l, '0'_l});
    Array<Logic, 4> b({'1'_l, '0'_l, '1'_l, '1'_l});
    auto sa = a[{1, 2}];
    auto sb = b[{1, 2}];
    auto c = sa & sb;
    static_assert(std::is_same_v<decltype(c), DynArray<Logic>>);
    EXPECT_EQ(c.range(), Range(1, Direction::DOWNTO, 0));
    EXPECT_EQ(c[1], '0'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestLogicArray, MixedLogicBitAnd) {
    DynArray<Logic> a({'0'_l, '1'_l, 'X'_l});
    DynArray<Bit> b({'1'_b, '1'_b, '1'_b});
    auto c = a & b;
    static_assert(std::is_same_v<decltype(c), DynArray<Logic>>);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

TEST(TestLogicArray, MixedLogicBitOr) {
    DynArray<Logic> a({'0'_l, '1'_l, 'X'_l});
    DynArray<Bit> b({'0'_b, '0'_b, '1'_b});
    auto c = a | b;
    static_assert(std::is_same_v<decltype(c), DynArray<Logic>>);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestLogicArray, MixedLogicBitXor) {
    DynArray<Logic> a({'0'_l, '1'_l, 'X'_l});
    DynArray<Bit> b({'1'_b, '1'_b, '0'_b});
    auto c = a ^ b;
    static_assert(std::is_same_v<decltype(c), DynArray<Logic>>);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '0'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

// -- to_logic_array from string ---------------------------------------------

TEST(TestLogicArray, ToLogicArray) {
    auto a = to_logic_array("01XZ");
    EXPECT_EQ(a.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(a[3], '0'_l);
    EXPECT_EQ(a[2], '1'_l);
    EXPECT_EQ(a[1], 'X'_l);
    EXPECT_EQ(a[0], 'Z'_l);
}

TEST(TestLogicArray, ToLogicArrayCaseInsensitive) {
    auto a = to_logic_array("01xzulwh-");
    EXPECT_EQ(a[8], '0'_l);
    EXPECT_EQ(a[7], '1'_l);
    EXPECT_EQ(a[6], 'X'_l);
    EXPECT_EQ(a[5], 'Z'_l);
    EXPECT_EQ(a[4], 'U'_l);
    EXPECT_EQ(a[3], 'L'_l);
    EXPECT_EQ(a[2], 'W'_l);
    EXPECT_EQ(a[1], 'H'_l);
    EXPECT_EQ(a[0], '-'_l);
}

TEST(TestLogicArray, ToLogicArrayUnderscore) {
    auto a = to_logic_array("01_XZ");
    EXPECT_EQ(a.range().length(), 4U);
    EXPECT_EQ(a[3], '0'_l);
    EXPECT_EQ(a[0], 'Z'_l);
}

TEST(TestLogicArray, ToLogicArrayEmpty) {
    auto a = to_logic_array("");
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestLogicArray, ToLogicArrayInvalidChar) {
    EXPECT_THROW(to_logic_array("0!1"), std::invalid_argument);
}

// -- to_string on arrays ----------------------------------------------------

TEST(TestLogicArray, ToStringLogic) {
    auto a = to_logic_array("01XZULWH-");
    EXPECT_EQ(to_string(a), "01XZULWH-");
}

TEST(TestLogicArray, ToStringEmpty) {
    auto a = to_logic_array("");
    EXPECT_EQ(to_string(a), "");
}

// -- is_resolvable on arrays ------------------------------------------------

TEST(TestLogicArray, IsResolvableTrue) {
    auto a = to_logic_array("01LH");
    EXPECT_TRUE(a.is_resolvable());
}

TEST(TestLogicArray, IsResolvableFalse) {
    EXPECT_FALSE(to_logic_array("01X0").is_resolvable());
    EXPECT_FALSE(to_logic_array("Z").is_resolvable());
    EXPECT_FALSE(to_logic_array("U").is_resolvable());
    EXPECT_FALSE(to_logic_array("W").is_resolvable());
    EXPECT_FALSE(to_logic_array("-").is_resolvable());
}

TEST(TestLogicArray, IsResolvableEmpty) {
    auto a = to_logic_array("");
    EXPECT_TRUE(a.is_resolvable());
}

// -- resolve on arrays ------------------------------------------------------

TEST(TestLogicArray, ResolveZeros) {
    auto a = to_logic_array("01XZULWH-");
    auto b = a.resolve(ResolveMethod::ZEROS);
    EXPECT_EQ(to_string(b), "010000010");
}

TEST(TestLogicArray, ResolveOnes) {
    auto a = to_logic_array("01XZULWH-");
    auto b = a.resolve(ResolveMethod::ONES);
    EXPECT_EQ(to_string(b), "011110111");
}

TEST(TestLogicArray, ResolveWeak) {
    auto a = to_logic_array("01XZULWH-");
    auto b = a.resolve(ResolveMethod::WEAK);
    EXPECT_EQ(to_string(b), "01XZU0X1-");
}

TEST(TestLogicArray, ResolveError) {
    auto a = to_logic_array("01X");
    EXPECT_THROW(a.resolve(ResolveMethod::ERROR), std::invalid_argument);
}

TEST(TestLogicArray, ResolveErrorPass) {
    auto a = to_logic_array("01");
    auto b = a.resolve(ResolveMethod::ERROR);
    EXPECT_EQ(to_string(b), "01");
}

TEST(TestLogicArray, ResolveStaticReturnsStaticArray) {
    auto a = "01XZ"_l;  // static LogicArray<Range{3, DOWNTO, 0}>
    auto b = a.resolve(ResolveMethod::ZEROS);
    // Static-bound input -> static-bound output of matching range. This is the
    // payoff of memberizing resolve on the specializations: the result type
    // preserves Self's static range when available.
    static_assert(std::is_same_v<decltype(b), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(to_string(b), "0100");
}

// -- Slice resolvability ---------------------------------------------------
//
// The constrained partial specs of ArraySlice and DynArraySlice inherit the
// LogicArrayMixin too, so slices of LogicArray/BitArray/DynLogicArray/etc
// have is_resolvable() and resolve(method) members. Sub-slicing preserves
// the mixin via outer-name resolution in the slice impl.

TEST(TestLogicArray, DynSliceIsResolvable) {
    // to_logic_array parses MSB-first into a DOWNTO range, so "01X" has
    // a[2]='0', a[1]='1', a[0]='X'.
    auto a = to_logic_array("01X");
    auto s_full = a[{2, 0}];
    EXPECT_FALSE(s_full.is_resolvable());  // covers the X at a[0]
    auto s_excl_x = a[{2, 1}];
    EXPECT_TRUE(s_excl_x.is_resolvable());  // covers '0' and '1' only
}

TEST(TestLogicArray, DynSliceResolveReturnsDynArray) {
    auto a = to_logic_array("01XZ");
    auto s = a[{3, 0}];
    auto r = s.resolve(ResolveMethod::ZEROS);
    static_assert(std::is_same_v<decltype(r), DynArray<Logic>>);
    EXPECT_EQ(to_string(r), "0100");
}

TEST(TestLogicArray, StaticSliceResolveReturnsStaticArray) {
    auto a = "01XZ"_l;  // LogicArray<Range{3, DOWNTO, 0}>
    auto s = a.slice<Range{2, Direction::DOWNTO, 1}>();
    auto r = s.resolve(ResolveMethod::ZEROS);
    static_assert(std::is_same_v<decltype(r), LogicArray<Range{2, Direction::DOWNTO, 1}>>);
    EXPECT_EQ(to_string(r), "10");  // X->0, 1->1; slice was {X, 1} in storage order
}

TEST(TestLogicArray, StaticSliceIsResolvable) {
    // Exercises ArraySlice<LogicArray<R>, R2>::is_resolvable() -- the constrained
    // partial spec from logic_array.hpp. Without this test a regression that drops
    // the mixin from the static slice spec would only be caught when user code
    // calls .is_resolvable() on a sliced LogicArray and fails to compile.
    auto a = "01XZ"_l;  // a[3]='0', a[2]='1', a[1]='X', a[0]='Z'
    auto s_with_x = a.slice<Range{2, Direction::DOWNTO, 1}>();
    EXPECT_FALSE(s_with_x.is_resolvable());  // contains '1' and 'X'
    auto s_clean = a.slice<Range{3, Direction::DOWNTO, 2}>();
    EXPECT_TRUE(s_clean.is_resolvable());  // '0' and '1'
}

TEST(TestLogicArray, ConstOwnerDynSliceHasMixin) {
    // Exercises DynArraySlice<const DynArray<Logic>> -- the constrained partial
    // spec must trigger for const-qualified Logic/Bit owners too. Without this
    // test a regression where range_value_t<const DynArray<Logic>> doesn't reduce
    // to Logic would leave const slices on the non-Logic primary shell and lose
    // the mixin. The check is structural (the call has to compile and return a
    // bool), not the value -- a const slice over a resolvable Logic array.
    DynArray<Logic> const a = to_logic_array("01LH");
    auto s = a[{3, 0}];
    static_assert(
        std::same_as<decltype(s), DynArraySlice<DynArray<Logic> const>>,
        "const Logic owner must produce DynArraySlice<const DynArray<Logic>>"
    );
    EXPECT_TRUE(s.is_resolvable());  // all elements are 0/1/L/H
    auto r = s.resolve(ResolveMethod::WEAK);
    EXPECT_EQ(to_string(r), "0101");  // L->0, H->1
}

TEST(TestLogicArray, SubSlicePreservesMixin) {
    auto a = to_logic_array("01XZ");
    auto s = a[{3, 0}];
    auto sub = s[{2, 1}];  // sub-slice via DynArraySliceImpl::operator[]
    // The sub-slice still has is_resolvable() -- the impl returns
    // DynArraySlice<ArrayT> by outer name, which resolves to the constrained
    // partial spec when the element type is Logic.
    EXPECT_FALSE(sub.is_resolvable());
}

// -- String-literal UDL ----------------------------------------------------

TEST(TestLogicArray, UdlLogic) {
    auto a = "01XZ"_l;
    static_assert(std::is_same_v<decltype(a), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(a.range(), (Range{3, Direction::DOWNTO, 0}));
    EXPECT_EQ(a[3], '0'_l);
    EXPECT_EQ(a[2], '1'_l);
    EXPECT_EQ(a[1], 'X'_l);
    EXPECT_EQ(a[0], 'Z'_l);
}

TEST(TestLogicArray, UdlLogicUnderscore) {
    auto a = "01_XZ"_l;
    static_assert(std::is_same_v<decltype(a), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(a[3], '0'_l);
    EXPECT_EQ(a[2], '1'_l);
    EXPECT_EQ(a[1], 'X'_l);
    EXPECT_EQ(a[0], 'Z'_l);
}

TEST(TestLogicArray, UdlLogicEmpty) {
    // Empty UDL yields a zero-length static LogicArray. The Range{-1, DOWNTO, 0}
    // construction inside the UDL is a degenerate range (length 0); verify it
    // type-checks and produces an iterable empty array.
    auto a = ""_l;
    static_assert(std::is_same_v<decltype(a), LogicArray<Range{-1, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(a.range().length(), 0U);
    EXPECT_EQ(a.begin(), a.end());
}

TEST(TestLogicArray, UdlLogicAllUnderscores) {
    // All-underscore literal yields a zero-length array just like the empty
    // literal -- the underscores are stripped, count_non_underscore returns 0.
    auto a = "____"_l;
    static_assert(std::is_same_v<decltype(a), LogicArray<Range{-1, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestLogicArray, FormatterStatic) {
    // The LogicType-constrained std::formatter spec in logic_array.hpp must
    // pick up static LogicArray, not just DynLogicArray (the latter is covered
    // in test_array.cpp). Verifies the partial spec matches at the static
    // owner type and produces the "Logic[range]{...}" form.
    auto a = "01XZ"_l;  // LogicArray<Range{3, DOWNTO, 0}>
    EXPECT_EQ(std::format("{}", a), "Logic[3 downto 0]{0, 1, X, Z}");
}

TEST(TestBitArray, FormatterStatic) {
    auto a = "0101"_b;  // BitArray<Range{3, DOWNTO, 0}>
    EXPECT_EQ(std::format("{}", a), "Bit[3 downto 0]{0, 1, 0, 1}");
}

TEST(TestLogicArray, UdlBitwiseOps) {
    auto a = "0101"_l & "1100"_l;
    static_assert(std::is_same_v<decltype(a), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(a[3], '0'_l);
    EXPECT_EQ(a[0], '0'_l);

    auto b = "0101"_l ^ "1100"_l;
    static_assert(std::is_same_v<decltype(b), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(b[3], '1'_l);
    EXPECT_EQ(b[0], '1'_l);

    auto c = "0101"_l | "1100"_l;
    static_assert(std::is_same_v<decltype(c), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(c[3], '1'_l);

    auto d = ~"0101"_l;
    static_assert(std::is_same_v<decltype(d), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(d[3], '1'_l);
}

TEST(TestLogicArray, BitwiseOpsMixedYieldsDynamic) {
    // One static operand and one dynamic operand -> DynArray result.
    auto lhs = "0101"_l;  // static LogicArray<4>
    DynLogicArray rhs({'1'_l, '1'_l, '0'_l, '0'_l});
    auto a = lhs & rhs;
    static_assert(std::is_same_v<decltype(a), DynLogicArray>);
    EXPECT_EQ(a.range(), (Range{3, Direction::DOWNTO, 0}));
    EXPECT_EQ(a[3], '0'_l);
    EXPECT_EQ(a[0], '0'_l);

    auto b = ~rhs;
    static_assert(std::is_same_v<decltype(b), DynLogicArray>);
    EXPECT_EQ(b[3], '0'_l);
    EXPECT_EQ(b[0], '1'_l);
}

// -- BitArray alias --------------------------------------------------------

static_assert(std::is_same_v<
              BitArray<Range{3, Direction::DOWNTO, 0}>,
              Array<Bit, Range{3, Direction::DOWNTO, 0}>>);
static_assert(std::is_same_v<DynBitArray, DynArray<Bit>>);

// -- Bit-only bitwise operations -------------------------------------------

TEST(TestBitArray, AndBit) {
    DynBitArray a({'1'_b, '0'_b, '1'_b, '0'_b});
    DynBitArray b({'1'_b, '1'_b, '0'_b, '0'_b});
    auto c = a & b;
    static_assert(std::is_same_v<decltype(c), DynBitArray>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_b);
    EXPECT_EQ(c[2], '0'_b);
    EXPECT_EQ(c[1], '0'_b);
    EXPECT_EQ(c[0], '0'_b);
}

TEST(TestBitArray, OrBit) {
    DynBitArray a({'1'_b, '0'_b, '0'_b, '0'_b});
    DynBitArray b({'0'_b, '0'_b, '1'_b, '0'_b});
    auto c = a | b;
    static_assert(std::is_same_v<decltype(c), DynBitArray>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_b);
    EXPECT_EQ(c[2], '0'_b);
    EXPECT_EQ(c[1], '1'_b);
    EXPECT_EQ(c[0], '0'_b);
}

TEST(TestBitArray, XorBit) {
    DynBitArray a({'1'_b, '0'_b, '1'_b, '0'_b});
    DynBitArray b({'1'_b, '1'_b, '0'_b, '0'_b});
    auto c = a ^ b;
    static_assert(std::is_same_v<decltype(c), DynBitArray>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '0'_b);
    EXPECT_EQ(c[2], '1'_b);
    EXPECT_EQ(c[1], '1'_b);
    EXPECT_EQ(c[0], '0'_b);
}

// -- Cross-type promotion (Logic wins) -------------------------------------

TEST(TestBitArray, StaticLogicBitAndPromotesToLogic) {
    auto l = "0101"_l;  // static LogicArray<4>
    auto b = "1100"_b;  // static BitArray<4>
    auto c = l & b;
    static_assert(std::is_same_v<decltype(c), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '0'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, StaticLogicBitOrPromotesToLogic) {
    auto l = "0100"_l;
    auto b = "1010"_b;
    auto c = l | b;
    static_assert(std::is_same_v<decltype(c), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, StaticLogicBitXorPromotesToLogic) {
    auto l = "0011"_l;
    auto b = "0101"_b;
    auto c = l ^ b;
    static_assert(std::is_same_v<decltype(c), LogicArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, StaticLogicXAndBitPromotesToLogic) {
    // Logic side carries 'X' -- result must remain Logic to preserve it.
    auto l = "X1"_l;
    auto b = "11"_b;
    auto c = l & b;
    static_assert(std::is_same_v<decltype(c), LogicArray<Range{1, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestBitArray, StaticBitBitStaysBit) {
    auto a = "0101"_b;  // static BitArray<4>
    auto b = "1100"_b;
    auto c = a & b;
    static_assert(std::is_same_v<decltype(c), BitArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(c[3], '0'_b);
    EXPECT_EQ(c[2], '1'_b);
    EXPECT_EQ(c[1], '0'_b);
    EXPECT_EQ(c[0], '0'_b);

    auto d = ~a;
    static_assert(std::is_same_v<decltype(d), BitArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(d[3], '1'_b);
    EXPECT_EQ(d[2], '0'_b);
    EXPECT_EQ(d[1], '1'_b);
    EXPECT_EQ(d[0], '0'_b);
}

TEST(TestBitArray, DynLogicBitAndPromotesToLogic) {
    DynLogicArray l({'0'_l, '1'_l, 'X'_l, '0'_l});
    DynBitArray b({'1'_b, '1'_b, '1'_b, '0'_b});
    auto c = l & b;
    static_assert(std::is_same_v<decltype(c), DynLogicArray>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, DynBitLogicOrPromotesToLogic) {
    // BitArray on LHS, LogicArray on RHS -- promotion still happens.
    DynBitArray b({'0'_b, '0'_b, '1'_b, '0'_b});
    DynLogicArray l({'1'_l, '0'_l, '0'_l, 'X'_l});
    auto c = b | l;
    static_assert(std::is_same_v<decltype(c), DynLogicArray>);
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

TEST(TestBitArray, StaticDynMixedYieldsDynLogic) {
    // Static LogicArray + dynamic BitArray -- dynamic operand forces DynArray,
    // and Logic wins for the element type.
    auto l = "0101"_l;
    DynBitArray b({'1'_b, '1'_b, '0'_b, '0'_b});
    auto c = l & b;
    static_assert(std::is_same_v<decltype(c), DynLogicArray>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, DynBitStaticLogicAndPromotesToDynLogic) {
    DynBitArray b({'1'_b, '1'_b, '0'_b, '0'_b});
    auto l = "0101"_l;
    auto c = b & l;
    static_assert(std::is_same_v<decltype(c), DynLogicArray>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '0'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, StaticBitDynLogicOrPromotesToDynLogic) {
    auto b = "0010"_b;
    DynLogicArray l({'1'_l, '0'_l, '0'_l, 'X'_l});
    auto c = b | l;
    static_assert(std::is_same_v<decltype(c), DynLogicArray>);
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

// -- to_bit_array from string ----------------------------------------------

TEST(TestBitArray, ToBitArray) {
    auto a = to_bit_array("0110");
    EXPECT_EQ(a.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(a[3], '0'_b);
    EXPECT_EQ(a[2], '1'_b);
    EXPECT_EQ(a[1], '1'_b);
    EXPECT_EQ(a[0], '0'_b);
}

TEST(TestBitArray, ToBitArrayUnderscore) {
    auto a = to_bit_array("01_10");
    EXPECT_EQ(a.range().length(), 4U);
    EXPECT_EQ(a[3], '0'_b);
    EXPECT_EQ(a[0], '0'_b);
}

TEST(TestBitArray, ToBitArrayEmpty) {
    auto a = to_bit_array("");
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestBitArray, ToBitArrayInvalidChar) {
    EXPECT_THROW(to_bit_array("01X0"), std::invalid_argument);
    EXPECT_THROW(to_bit_array("2"), std::invalid_argument);
}

// -- to_bit_array from range of Logic --------------------------------------

TEST(TestBitArray, ToBitArrayFromLogicRange) {
    auto a = to_logic_array("01LH");
    auto b = to_bit_array(a);
    static_assert(std::is_same_v<decltype(b), DynBitArray>);
    EXPECT_EQ(b.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(b[3], '0'_b);
    EXPECT_EQ(b[2], '1'_b);
    EXPECT_EQ(b[1], '0'_b);
    EXPECT_EQ(b[0], '1'_b);
}

TEST(TestBitArray, ToBitArrayFromLogicRangeNonResolvable) {
    EXPECT_THROW(to_bit_array(to_logic_array("X")), std::invalid_argument);
    EXPECT_THROW(to_bit_array(to_logic_array("Z")), std::invalid_argument);
    EXPECT_THROW(to_bit_array(to_logic_array("U")), std::invalid_argument);
    EXPECT_THROW(to_bit_array(to_logic_array("W")), std::invalid_argument);
    EXPECT_THROW(to_bit_array(to_logic_array("-")), std::invalid_argument);
}

TEST(TestBitArray, ToBitArrayFromStdVector) {
    // Exercises the sized_range overload with a non-coconext range. std::vector
    // is the canonical non-coconext sized range; this verifies the constraint
    // accepts it and the single-pass fused walk yields the same result as the
    // coconext-array form above.
    std::vector<Logic> const v{'0'_l, '1'_l, 'L'_l, 'H'_l};
    auto b = to_bit_array(v);
    static_assert(std::is_same_v<decltype(b), DynBitArray>);
    EXPECT_EQ(b.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(b[3], '0'_b);
    EXPECT_EQ(b[2], '1'_b);
    EXPECT_EQ(b[1], '0'_b);  // L -> 0
    EXPECT_EQ(b[0], '1'_b);  // H -> 1
}

TEST(TestBitArray, ToBitArrayFromStdVectorNonResolvable) {
    std::vector<Logic> const v{'0'_l, '1'_l, 'X'_l};
    EXPECT_THROW(to_bit_array(v), std::invalid_argument);
}

TEST(TestBitArray, ToBitArrayFromStaticLogicArray) {
    auto a = "01LH"_l;
    auto b = to_bit_array(a);
    static_assert(std::is_same_v<decltype(b), DynBitArray>);
    EXPECT_EQ(to_string(b), "0101");
}

// -- to_string on BitArray -------------------------------------------------

TEST(TestBitArray, ToStringBit) {
    auto a = to_bit_array("0110");
    EXPECT_EQ(to_string(a), "0110");
}

TEST(TestBitArray, ToStringStaticBit) {
    auto a = "0101"_b;
    EXPECT_EQ(to_string(a), "0101");
}

// -- is_resolvable on BitArray ---------------------------------------------

TEST(TestBitArray, IsResolvableAlwaysTrue) {
    auto a = to_bit_array("0110");
    EXPECT_TRUE(a.is_resolvable());
    auto empty = to_bit_array("");
    EXPECT_TRUE(empty.is_resolvable());
}

// -- resolve on BitArray ---------------------------------------------------

TEST(TestBitArray, ResolveBitIsIdentity) {
    auto a = to_bit_array("0110");
    static_assert(std::is_same_v<decltype(a.resolve(ResolveMethod::ZEROS)), DynBitArray>);
    for (auto method :
         {ResolveMethod::ZEROS,
          ResolveMethod::ONES,
          ResolveMethod::WEAK,
          ResolveMethod::ERROR,
          ResolveMethod::RANDOM})
    {
        EXPECT_EQ(to_string(a.resolve(method)), "0110");
    }
}

// -- String-literal UDL ----------------------------------------------------

TEST(TestBitArray, UdlBit) {
    auto a = "0101"_b;
    static_assert(std::is_same_v<decltype(a), BitArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(a.range(), (Range{3, Direction::DOWNTO, 0}));
    EXPECT_EQ(a[3], '0'_b);
    EXPECT_EQ(a[2], '1'_b);
    EXPECT_EQ(a[1], '0'_b);
    EXPECT_EQ(a[0], '1'_b);
}

TEST(TestBitArray, UdlBitConstexpr) {
    constexpr auto a = "0101"_b;
    static_assert(a[3] == '0'_b);
    static_assert(a[2] == '1'_b);
    static_assert(a[1] == '0'_b);
    static_assert(a[0] == '1'_b);
}

TEST(TestBitArray, UdlBitUnderscore) {
    auto a = "01_10"_b;
    static_assert(std::is_same_v<decltype(a), BitArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(a[3], '0'_b);
    EXPECT_EQ(a[2], '1'_b);
    EXPECT_EQ(a[1], '1'_b);
    EXPECT_EQ(a[0], '0'_b);
}

// Note: "<bad>"_b is a compile-time error (throw in constant evaluation), so
// the UDL invalid-character path cannot be exercised by a runtime test. The
// runtime invalid-character path is covered by ToBitArrayInvalidChar above.

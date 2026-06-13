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
    Vector<Logic> a({'1'_l, '0'_l, 'X'_l, '1'_l});
    Vector<Logic> b({'1'_l, '1'_l, '1'_l, '0'_l});
    auto c = a & b;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '0'_l);
}

// -- Bitwise array operations: OR -------------------------------------------

TEST(TestLogicArray, OrLogic) {
    Vector<Logic> a({'1'_l, '0'_l, 'X'_l, '0'_l});
    Vector<Logic> b({'0'_l, '0'_l, '1'_l, '0'_l});
    auto c = a | b;
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

// -- Bitwise array operations: XOR ------------------------------------------

TEST(TestLogicArray, XorLogic) {
    Vector<Logic> a({'1'_l, '0'_l, '1'_l, '0'_l});
    Vector<Logic> b({'1'_l, '1'_l, '0'_l, '0'_l});
    auto c = a ^ b;
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

// -- Bitwise array operations: NOT ------------------------------------------

TEST(TestLogicArray, NotLogic) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    auto c = ~a;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

TEST(TestLogicArray, NotBit) {
    Vector<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    auto c = ~a;
    static_assert(std::is_same_v<decltype(c), Vector<Bit>>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_b);
    EXPECT_EQ(c[2], '0'_b);
    EXPECT_EQ(c[1], '1'_b);
    EXPECT_EQ(c[0], '0'_b);
}

// -- Bitwise operations: edge cases -----------------------------------------

TEST(TestLogicArray, BitwiseLengthMismatch) {
    Vector<Logic> a({'0'_l, '1'_l});
    Vector<Logic> b({'0'_l, '1'_l, 'X'_l});
    EXPECT_THROW((void)(a & b), std::invalid_argument);
    EXPECT_THROW((void)(a | b), std::invalid_argument);
    EXPECT_THROW((void)(a ^ b), std::invalid_argument);
}

TEST(TestLogicArray, BitwiseEmpty) {
    Vector<Logic> a({});
    Vector<Logic> b({});
    auto c = a & b;
    EXPECT_EQ(c.range().length(), 0U);
}

TEST(TestLogicArray, BitwiseResultRange) {
    Vector<Logic> a({'0'_l, '1'_l, '1'_l}, Range(5, Direction::DOWNTO, 3));
    Vector<Logic> b({'1'_l, '1'_l, '0'_l});
    auto c = a & b;
    EXPECT_EQ(c.range(), Range(2, Direction::DOWNTO, 0));
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestLogicArray, BitwiseOnSlice) {
    Vector<Logic> a({'0'_l, '1'_l, '1'_l, '0'_l});
    Vector<Logic> b({'1'_l, '0'_l});
    auto s = a[{2, 1}];
    auto c = s & b;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
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
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c.range(), Range(1, Direction::DOWNTO, 0));
    EXPECT_EQ(c[1], '0'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestLogicArray, MixedLogicBitAnd) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l});
    Vector<Bit> b({'1'_b, '1'_b, '1'_b});
    auto c = a & b;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

TEST(TestLogicArray, MixedLogicBitOr) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l});
    Vector<Bit> b({'0'_b, '0'_b, '1'_b});
    auto c = a | b;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestLogicArray, MixedLogicBitXor) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l});
    Vector<Bit> b({'1'_b, '1'_b, '0'_b});
    auto c = a ^ b;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '0'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

// -- Bitwise scalar+array broadcasts ----------------------------------------

TEST(TestLogicArray, AndScalarLHSVector) {
    Vector<Logic> a({'1'_l, '0'_l, 'X'_l, '1'_l});
    auto c = '1'_l & a;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestLogicArray, AndScalarRHSVector) {
    Vector<Logic> a({'1'_l, '0'_l, 'X'_l, '1'_l});
    auto c = a & '0'_l;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '0'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestLogicArray, OrScalarLHSVector) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, '0'_l});
    auto c = '0'_l | a;
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestLogicArray, OrScalarRHSVector) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, '0'_l});
    auto c = a | '1'_l;
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestLogicArray, XorScalarLHSVector) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, '0'_l});
    auto c = '1'_l ^ a;
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestLogicArray, XorScalarRHSVector) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, '0'_l});
    auto c = a ^ '0'_l;
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestLogicArray, ScalarBitwiseStaticReturnsStatic) {
    Array<Logic, 4> a({'0'_l, '1'_l, 'X'_l, '1'_l});
    auto c = a & '1'_l;
    static_assert(
        std::is_same_v<decltype(c), Array<Logic, Range{3, Direction::DOWNTO, 0}>>
    );
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '1'_l);
}

TEST(TestLogicArray, ScalarBitwiseMixedBitScalarLogicArray) {
    // Bit scalar combined with a Logic array: result element type follows the
    // op's deduced return (Logic, since Bit & Logic -> Logic).
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l});
    auto c = '1'_b & a;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

TEST(TestLogicArray, ScalarBitwiseOnSlice) {
    Vector<Logic> a({'0'_l, '1'_l, '1'_l, '0'_l});
    auto s = a[{2, 1}];
    auto c = s | '1'_l;
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(c.range(), Range(1, Direction::DOWNTO, 0));
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], '1'_l);
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

TEST(TestLogicArray, ResolveWeakAcceptsResolvable) {
    // WEAK passes 0/1/L/H -> 0/1/0/1 and throws on the rest. The input must
    // contain only resolvable-under-WEAK values for the call to succeed.
    auto a = to_logic_array("01LH");
    auto b = a.resolve(ResolveMethod::WEAK);
    EXPECT_EQ(to_string(b), "0101");
}

TEST(TestLogicArray, ResolveWeakThrowsOnMetavalue) {
    // Even a single non-resolvable value makes the whole array throw.
    auto a = to_logic_array("01X");
    EXPECT_THROW(a.resolve(ResolveMethod::WEAK), std::invalid_argument);
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

TEST(TestLogicArray, ResolveErrorThrowsOnWeakStrengths) {
    // ERROR is the strict tier -- L and H aren't accepted even though they're
    // representable as 0/1 (use WEAK for that).
    EXPECT_THROW(to_logic_array("0L").resolve(ResolveMethod::ERROR), std::invalid_argument);
    EXPECT_THROW(to_logic_array("1H").resolve(ResolveMethod::ERROR), std::invalid_argument);
}

TEST(TestLogicArray, ResolveStaticReturnsStaticArray) {
    auto a = "01XZ"_l;  // static LogicArray<Range{3, DOWNTO, 0}>
    auto b = a.resolve(ResolveMethod::ZEROS);
    // Static-bound input -> static-bound output of matching range. This is the
    // payoff of memberizing resolve on the specializations: the result type
    // preserves Self's static range when available, and resolve always returns
    // a Bit-valued container.
    static_assert(std::is_same_v<decltype(b), BitArray<Range{3, Direction::DOWNTO, 0}>>);
    EXPECT_EQ(to_string(b), "0100");
}

// -- Slice resolvability ---------------------------------------------------
//
// The constrained partial specs of StaticArraySlice and ArraySlice inherit the
// LogicArrayMixin too, so slices of LogicArray/BitArray/LogicVector/etc
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

TEST(TestLogicArray, DynSliceResolveReturnsVector) {
    auto a = to_logic_array("01XZ");
    auto s = a[{3, 0}];
    auto r = s.resolve(ResolveMethod::ZEROS);
    static_assert(std::is_same_v<decltype(r), BitVector>);
    EXPECT_EQ(to_string(r), "0100");
}

TEST(TestLogicArray, StaticSliceResolveReturnsStaticArray) {
    auto a = "01XZ"_l;  // LogicArray<Range{3, DOWNTO, 0}>
    auto s = a.slice<Range{2, Direction::DOWNTO, 1}>();
    auto r = s.resolve(ResolveMethod::ZEROS);
    static_assert(std::is_same_v<decltype(r), BitArray<Range{2, Direction::DOWNTO, 1}>>);
    EXPECT_EQ(to_string(r), "10");  // X->0, 1->1; slice was {X, 1} in storage order
}

TEST(TestLogicArray, StaticSliceIsResolvable) {
    // Exercises StaticArraySlice<LogicArray<R>, R2>::is_resolvable() -- the constrained
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
    // Exercises ArraySlice<const Vector<Logic>> -- the constrained partial
    // spec must trigger for const-qualified Logic/Bit owners too. Without this
    // test a regression where range_value_t<const Vector<Logic>> doesn't reduce
    // to Logic would leave const slices on the non-Logic primary shell and lose
    // the mixin. The check is structural (the call has to compile and return a
    // bool), not the value -- a const slice over a resolvable Logic array.
    Vector<Logic> const a = to_logic_array("01LH");
    auto s = a[{3, 0}];
    static_assert(
        std::same_as<decltype(s), ArraySlice<Vector<Logic> const>>,
        "const Logic owner must produce ArraySlice<const Vector<Logic>>"
    );
    EXPECT_TRUE(s.is_resolvable());  // all elements are 0/1/L/H
    auto r = s.resolve(ResolveMethod::WEAK);
    EXPECT_EQ(to_string(r), "0101");  // L->0, H->1
}

TEST(TestLogicArray, SubSlicePreservesMixin) {
    auto a = to_logic_array("01XZ");
    auto s = a[{3, 0}];
    auto sub = s[{2, 1}];  // sub-slice via ArraySliceImpl::operator[]
    // The sub-slice still has is_resolvable() -- the impl returns
    // ArraySlice<ArrayT> by outer name, which resolves to the constrained
    // partial spec when the element type is Logic.
    EXPECT_FALSE(sub.is_resolvable());
}

// -- index / rindex on Logic/Bit arrays -----------------------------------

TEST(TestLogicArray, IndexInheritedOnLogicVector) {
    auto a = "10X10"_l;  // DOWNTO {4..0}: a[4]=1, a[3]=0, a[2]=X, a[1]=1, a[0]=0
    auto first_one = a.index('1'_l);
    auto last_one = a.rindex('1'_l);
    ASSERT_TRUE(first_one.has_value() && last_one.has_value());
    EXPECT_EQ(*first_one, 4);  // first '1' in iteration -> highest HDL coord
    EXPECT_EQ(*last_one, 1);
    EXPECT_FALSE(a.index('U'_l).has_value());
}

TEST(TestBitArray, IndexInheritedOnStaticBitArray) {
    BitArray<4> a({'1'_b, '0'_b, '1'_b, '0'_b});
    EXPECT_EQ(*a.index('1'_b), 3);   // DOWNTO {3..0}, first '1' is a[3]
    EXPECT_EQ(*a.rindex('1'_b), 1);  // last '1' is a[1]
}

// -- Reductions: and_reduce / or_reduce / xor_reduce -----------------------

TEST(TestLogicArray, AndReduceAllOnes) {
    auto a = "1111"_l;
    EXPECT_EQ(a.and_reduce(), '1'_l);
}

TEST(TestLogicArray, AndReduceWithZero) {
    auto a = "1101"_l;
    EXPECT_EQ(a.and_reduce(), '0'_l);
}

TEST(TestLogicArray, AndReduceWithX) {
    auto a = "11X1"_l;
    EXPECT_EQ(a.and_reduce(), 'X'_l);
}

TEST(TestLogicArray, AndReduceEmpty) {
    Vector<Logic> a({});
    EXPECT_EQ(a.and_reduce(), '1'_l);  // identity for AND
}

TEST(TestLogicArray, OrReduceAllZeros) {
    auto a = "0000"_l;
    EXPECT_EQ(a.or_reduce(), '0'_l);
}

TEST(TestLogicArray, OrReduceWithOne) {
    auto a = "0010"_l;
    EXPECT_EQ(a.or_reduce(), '1'_l);
}

TEST(TestLogicArray, OrReduceWithX) {
    auto a = "00X0"_l;
    EXPECT_EQ(a.or_reduce(), 'X'_l);
}

TEST(TestLogicArray, OrReduceEmpty) {
    Vector<Logic> a({});
    EXPECT_EQ(a.or_reduce(), '0'_l);  // identity for OR
}

TEST(TestLogicArray, XorReduceEvenOnes) {
    auto a = "1100"_l;
    EXPECT_EQ(a.xor_reduce(), '0'_l);
}

TEST(TestLogicArray, XorReduceOddOnes) {
    auto a = "1110"_l;
    EXPECT_EQ(a.xor_reduce(), '1'_l);
}

TEST(TestLogicArray, XorReduceWithX) {
    auto a = "1X1"_l;
    EXPECT_EQ(a.xor_reduce(), 'X'_l);
}

TEST(TestLogicArray, XorReduceEmpty) {
    Vector<Logic> a({});
    EXPECT_EQ(a.xor_reduce(), '0'_l);  // identity for XOR
}

TEST(TestBitArray, ReductionsReturnBit) {
    auto a = "1010"_b;
    static_assert(std::is_same_v<decltype(a.and_reduce()), Bit>);
    EXPECT_EQ(a.and_reduce(), '0'_b);
    EXPECT_EQ(a.or_reduce(), '1'_b);
    EXPECT_EQ(a.xor_reduce(), '0'_b);
}

TEST(TestLogicArray, ReductionsOnVector) {
    Vector<Logic> a({'1'_l, '0'_l, '1'_l});
    EXPECT_EQ(a.and_reduce(), '0'_l);
    EXPECT_EQ(a.or_reduce(), '1'_l);
    EXPECT_EQ(a.xor_reduce(), '0'_l);
}

TEST(TestLogicArray, ReductionsOnSlice) {
    auto a = "1110"_l;
    auto s = a[{3, 1}];  // "111"
    EXPECT_EQ(s.and_reduce(), '1'_l);
    EXPECT_EQ(s.or_reduce(), '1'_l);
    EXPECT_EQ(s.xor_reduce(), '1'_l);
}

// -- Compound bitwise assignment -------------------------------------------

TEST(TestLogicArray, CompoundAndArrayArrayDynLogic) {
    Vector<Logic> a({'1'_l, '0'_l, 'X'_l, '1'_l});
    Vector<Logic> b({'1'_l, '1'_l, '1'_l, '0'_l});
    a &= b;
    EXPECT_EQ(to_string(a), "10X0");
}

TEST(TestLogicArray, CompoundOrArrayArrayDynLogic) {
    Vector<Logic> a({'1'_l, '0'_l, 'X'_l, '0'_l});
    Vector<Logic> b({'0'_l, '0'_l, '1'_l, '0'_l});
    a |= b;
    // X | 1 = 1 per Logic truth table.
    EXPECT_EQ(to_string(a), "1010");
}

TEST(TestLogicArray, CompoundXorArrayArrayDynLogic) {
    Vector<Logic> a({'1'_l, '0'_l, '1'_l, '0'_l});
    Vector<Logic> b({'1'_l, '1'_l, '0'_l, '0'_l});
    a ^= b;
    EXPECT_EQ(to_string(a), "0110");
}

TEST(TestLogicArray, CompoundLengthMismatchThrows) {
    Vector<Logic> a({'0'_l, '1'_l});
    Vector<Logic> b({'0'_l, '1'_l, 'X'_l});
    EXPECT_THROW(a &= b, std::invalid_argument);
}

TEST(TestLogicArray, CompoundAndScalarRHS) {
    Vector<Logic> a({'1'_l, '0'_l, 'X'_l, '1'_l});
    a &= '0'_l;
    EXPECT_EQ(to_string(a), "0000");
}

TEST(TestLogicArray, CompoundOrScalarRHS) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, '0'_l});
    a |= '1'_l;
    EXPECT_EQ(to_string(a), "1111");
}

TEST(TestLogicArray, CompoundXorScalarRHS) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, '0'_l});
    a ^= '1'_l;
    EXPECT_EQ(to_string(a), "10X1");
}

TEST(TestLogicArray, CompoundLogicArrayWithBitArray) {
    // LogicArray &= BitArray works because Bit -> Logic implicitly.
    Vector<Logic> a({'1'_l, '0'_l, 'X'_l});
    Vector<Bit> b({'1'_b, '1'_b, '1'_b});
    a &= b;
    EXPECT_EQ(to_string(a), "10X");
}

TEST(TestBitArray, CompoundBitArrayWithBitArray) {
    Vector<Bit> a({'1'_b, '0'_b, '1'_b, '1'_b});
    Vector<Bit> b({'1'_b, '1'_b, '0'_b, '1'_b});
    a &= b;
    EXPECT_EQ(to_string(a), "1001");
}

TEST(TestLogicArray, CompoundOnSlice) {
    Vector<Logic> a({'1'_l, '1'_l, '1'_l, '1'_l});
    a[{2, 1}] &= '0'_l;
    EXPECT_EQ(to_string(a), "1001");
}

TEST(TestLogicArray, CompoundOnStaticArray) {
    LogicArray<4> a({'1'_l, '0'_l, '1'_l, '1'_l});
    a |= '0'_l;
    EXPECT_EQ(to_string(a), "1011");
}

TEST(TestLogicArray, CompoundStaticArrayWithStaticArray) {
    // Both sides have static ranges -- length check is a static_assert; a
    // mismatch would fail compile rather than throw.
    LogicArray<4> a({'1'_l, '0'_l, '1'_l, '1'_l});
    LogicArray<4> b({'1'_l, '1'_l, '0'_l, '1'_l});
    a &= b;
    EXPECT_EQ(to_string(a), "1001");
}

TEST(TestLogicArray, CompoundEmptyArrays) {
    Vector<Logic> a({});
    Vector<Logic> b({});
    a &= b;  // no-op
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestLogicArray, CompoundReturnsReference) {
    // Confirm the compound op returns its LHS so chained writes / use as an
    // expression work.
    Vector<Logic> a({'1'_l, '1'_l});
    auto& ref = (a &= '0'_l);
    EXPECT_EQ(&ref, &a);
    EXPECT_EQ(to_string(a), "00");
}

TEST(TestLogicArray, InplaceNotDynLogic) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l, 'Z'_l});
    inplace_not(a);
    EXPECT_EQ(to_string(a), "10XX");  // ~Z = X
}

TEST(TestBitArray, InplaceNotDynBit) {
    Vector<Bit> a({'0'_b, '1'_b, '0'_b, '1'_b});
    inplace_not(a);
    EXPECT_EQ(to_string(a), "1010");
}

TEST(TestLogicArray, InplaceNotStaticArray) {
    LogicArray<3> a({'0'_l, '1'_l, 'X'_l});
    inplace_not(a);
    EXPECT_EQ(to_string(a), "10X");
}

TEST(TestLogicArray, InplaceNotOnSlice) {
    Vector<Logic> a({'0'_l, '0'_l, '0'_l, '0'_l});
    inplace_not(a[{2, 1}]);
    EXPECT_EQ(to_string(a), "0110");
}

TEST(TestLogicArray, InplaceNotEmpty) {
    Vector<Logic> a({});
    inplace_not(a);
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestLogicArray, InplaceNotReturnsReference) {
    Vector<Logic> a({'1'_l});
    auto& ref = inplace_not(a);
    EXPECT_EQ(&ref, &a);
}

// -- Concatenation ---------------------------------------------------------

TEST(TestLogicArray, ConcatTwoArrays) {
    auto a = "0011"_l;  // {3 DOWNTO 0}: bits 0,0,1,1
    auto b = "1100"_l;  // {3 DOWNTO 0}: bits 1,1,0,0
    auto c = concat(a, b);
    // a first → high 4 bits; b second → low 4 bits. Iteration order MSB-first.
    EXPECT_EQ(to_string(c), "00111100");
    static_assert(
        std::is_same_v<decltype(c), detail::Array<Logic, Range{7, Direction::DOWNTO, 0}>>
    );
}

TEST(TestLogicArray, ConcatScalarLeftArrayRight) {
    auto c = concat('1'_l, "00"_l);
    EXPECT_EQ(to_string(c), "100");
    EXPECT_EQ(c.range(), (Range{2, Direction::DOWNTO, 0}));
}

TEST(TestLogicArray, ConcatArrayLeftScalarRight) {
    auto c = concat("11"_l, '0'_l);
    EXPECT_EQ(to_string(c), "110");
}

TEST(TestLogicArray, ConcatThreeArgs) {
    auto c = concat('1'_l, "010"_l, '0'_l);
    EXPECT_EQ(to_string(c), "10100");
}

TEST(TestLogicArray, ConcatMixedBitLogicReturnsLogic) {
    auto a = "01"_b;
    auto b = "X1"_l;
    auto c = concat(a, b);
    // Bit converts implicitly to Logic; common type is Logic.
    static_assert(std::same_as<std::ranges::range_value_t<decltype(c)>, Logic>);
    EXPECT_EQ(to_string(c), "01X1");
}

TEST(TestLogicArray, ConcatAllStaticReturnsStatic) {
    LogicArray<3> a({'1'_l, '0'_l, '1'_l});
    LogicArray<2> b({'0'_l, '1'_l});
    auto c = concat(a, b);
    static_assert(
        std::is_same_v<decltype(c), detail::Array<Logic, Range{4, Direction::DOWNTO, 0}>>
    );
    EXPECT_EQ(to_string(c), "10101");
}

TEST(TestLogicArray, ConcatDynamicReturnsDynamic) {
    Vector<Logic> a({'1'_l, '0'_l, '1'_l});
    LogicArray<2> b({'0'_l, '1'_l});
    auto c = concat(a, b);
    static_assert(std::is_same_v<decltype(c), Vector<Logic>>);
    EXPECT_EQ(to_string(c), "10101");
    EXPECT_EQ(c.range(), (Range{4, Direction::DOWNTO, 0}));
}

TEST(TestLogicArray, ConcatSingleScalar) {
    auto c = concat('1'_l);
    static_assert(
        std::is_same_v<decltype(c), detail::Array<Logic, Range{0, Direction::DOWNTO, 0}>>
    );
    EXPECT_EQ(to_string(c), "1");
}

TEST(TestLogicArray, ConcatSingleArrayNormalizesToDownto) {
    // Single-arg concat with a TO array effectively re-normalizes to DOWNTO.
    // a is TO {0..2}: a[0]='1', a[1]='0', a[2]='X'.
    Array<Logic, Range{0, Direction::TO, 2}> a({'1'_l, '0'_l, 'X'_l});
    auto c = concat(a);
    // Iteration order through a is begin to end = '1', '0', 'X'.
    // Result is {2 DOWNTO 0}: c[2]='1' (first written), c[0]='X' (last).
    EXPECT_EQ(c.range(), (Range{2, Direction::DOWNTO, 0}));
    EXPECT_EQ(to_string(c), "10X");
}

TEST(TestLogicArray, ConcatEmptyArrays) {
    Vector<Logic> a({});
    Vector<Logic> b({});
    auto c = concat(a, b);
    EXPECT_EQ(c.range().length(), 0U);
}

TEST(TestLogicArray, ConcatEmptyAndNonEmpty) {
    Vector<Logic> empty({});
    auto c = concat(empty, "1"_l);
    EXPECT_EQ(to_string(c), "1");
}

TEST(TestLogicArray, ConcatOnSlice) {
    auto a = "1100"_l;
    auto s = a[{3, 2}];  // "11"
    auto c = concat(s, '0'_l);
    EXPECT_EQ(to_string(c), "110");
}

TEST(TestBitArray, ConcatBitArrays) {
    auto a = "10"_b;
    auto b = "01"_b;
    auto c = concat(a, b);
    static_assert(std::same_as<std::ranges::range_value_t<decltype(c)>, Bit>);
    EXPECT_EQ(to_string(c), "1001");
}

// -- DOWNTO defaults for length-only construction --------------------------

TEST(TestLogicArray, VectorLengthCtorDefaultsToDowntoLogic) {
    Vector<Logic> a(5);
    EXPECT_EQ(a.range(), (Range{4, Direction::DOWNTO, 0}));
}

TEST(TestBitArray, VectorLengthCtorDefaultsToDowntoBit) {
    Vector<Bit> a(5);
    EXPECT_EQ(a.range(), (Range{4, Direction::DOWNTO, 0}));
}

TEST(TestLogicArray, VectorInitListDefaultsToDowntoLogic) {
    Vector<Logic> a({'0'_l, '1'_l, 'X'_l});
    EXPECT_EQ(a.range(), (Range{2, Direction::DOWNTO, 0}));
    EXPECT_EQ(a[2], '0'_l);  // HDL coord 2 == first element (high bit)
    EXPECT_EQ(a[0], 'X'_l);  // HDL coord 0 == last element (low bit)
}

TEST(TestLogicArray, VectorSizedRangeDefaultsToDowntoLogic) {
    std::vector<Logic> v{'0'_l, '1'_l, 'X'_l};
    Vector<Logic> a(v);
    EXPECT_EQ(a.range(), (Range{2, Direction::DOWNTO, 0}));
}

TEST(TestLogicArray, VectorEmptyInitListDowntoLogic) {
    Vector<Logic> a({});
    EXPECT_EQ(a.range().length(), 0U);
}

TEST(TestLogicArray, LogicArrayAliasLengthDefaultsToDownto) {
    // LogicArray<N>/BitArray<N> aliases use DOWNTO for length-only forms.
    static_assert(
        std::is_same_v<LogicArray<4>, detail::Array<Logic, Range{3, Direction::DOWNTO, 0}>>
    );
    static_assert(
        std::is_same_v<BitArray<8>, detail::Array<Bit, Range{7, Direction::DOWNTO, 0}>>
    );
    LogicArray<4> a{};
    EXPECT_EQ(a.range(), (Range{3, Direction::DOWNTO, 0}));
}

TEST(TestLogicArray, LogicArrayAliasExplicitRangePreserved) {
    // Explicit Range NTTP is passed through unchanged.
    static_assert(std::is_same_v<
                  LogicArray<Range{0, Direction::TO, 7}>,
                  detail::Array<Logic, Range{0, Direction::TO, 7}>>);
}

TEST(TestLogicArray, LogicArrayAliasTwoArgFormDirection) {
    // 2-arg form auto-detects direction, with one tweak: L == R picks DOWNTO
    // (rather than TO as the generic auto-direction would) to match the 1-arg
    // length-only convention.
    static_assert(std::is_same_v<
                  LogicArray<7, 0>,
                  detail::Array<Logic, Range{7, Direction::DOWNTO, 0}>>);
    static_assert(std::is_same_v<
                  LogicArray<3, 3>,
                  detail::Array<Logic, Range{3, Direction::DOWNTO, 3}>>);
    // L < R keeps the generic TO auto-direction.
    static_assert(
        std::is_same_v<LogicArray<0, 7>, detail::Array<Logic, Range{0, Direction::TO, 7}>>
    );
    static_assert(
        std::is_same_v<BitArray<15, 0>, detail::Array<Bit, Range{15, Direction::DOWNTO, 0}>>
    );
}

TEST(TestLogicArray, LogicArrayAliasThreeArgFormRespectsDirection) {
    // 3-arg (L, D, H) form lets the user override the default DOWNTO.
    static_assert(std::is_same_v<
                  LogicArray<0, Direction::TO, 7>,
                  detail::Array<Logic, Range{0, Direction::TO, 7}>>);
    static_assert(std::is_same_v<
                  BitArray<7, Direction::DOWNTO, 0>,
                  detail::Array<Bit, Range{7, Direction::DOWNTO, 0}>>);
}

TEST(TestLogicArray, GenericArraySugarStillTo) {
    // `Array<Logic, N>` (generic sugar, not LogicArray) keeps generic TO
    // defaults -- users wanting HDL conventions should prefer LogicArray.
    Array<Logic, 4> a{};
    EXPECT_EQ(a.range(), (Range{0, Direction::TO, 3}));
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
    // pick up static LogicArray, not just LogicVector (the latter is covered
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
    // One static operand and one dynamic operand -> Vector result.
    auto lhs = "0101"_l;  // static LogicArray<4>
    LogicVector rhs({'1'_l, '1'_l, '0'_l, '0'_l});
    auto a = lhs & rhs;
    static_assert(std::is_same_v<decltype(a), LogicVector>);
    EXPECT_EQ(a.range(), (Range{3, Direction::DOWNTO, 0}));
    EXPECT_EQ(a[3], '0'_l);
    EXPECT_EQ(a[0], '0'_l);

    auto b = ~rhs;
    static_assert(std::is_same_v<decltype(b), LogicVector>);
    EXPECT_EQ(b[3], '0'_l);
    EXPECT_EQ(b[0], '1'_l);
}

// -- BitArray alias --------------------------------------------------------

static_assert(std::is_same_v<
              BitArray<Range{3, Direction::DOWNTO, 0}>,
              Array<Bit, Range{3, Direction::DOWNTO, 0}>>);
static_assert(std::is_same_v<BitVector, Vector<Bit>>);

// -- Bit-only bitwise operations -------------------------------------------

TEST(TestBitArray, AndBit) {
    BitVector a({'1'_b, '0'_b, '1'_b, '0'_b});
    BitVector b({'1'_b, '1'_b, '0'_b, '0'_b});
    auto c = a & b;
    static_assert(std::is_same_v<decltype(c), BitVector>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_b);
    EXPECT_EQ(c[2], '0'_b);
    EXPECT_EQ(c[1], '0'_b);
    EXPECT_EQ(c[0], '0'_b);
}

TEST(TestBitArray, OrBit) {
    BitVector a({'1'_b, '0'_b, '0'_b, '0'_b});
    BitVector b({'0'_b, '0'_b, '1'_b, '0'_b});
    auto c = a | b;
    static_assert(std::is_same_v<decltype(c), BitVector>);
    EXPECT_EQ(c.range(), Range(3, Direction::DOWNTO, 0));
    EXPECT_EQ(c[3], '1'_b);
    EXPECT_EQ(c[2], '0'_b);
    EXPECT_EQ(c[1], '1'_b);
    EXPECT_EQ(c[0], '0'_b);
}

TEST(TestBitArray, XorBit) {
    BitVector a({'1'_b, '0'_b, '1'_b, '0'_b});
    BitVector b({'1'_b, '1'_b, '0'_b, '0'_b});
    auto c = a ^ b;
    static_assert(std::is_same_v<decltype(c), BitVector>);
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
    LogicVector l({'0'_l, '1'_l, 'X'_l, '0'_l});
    BitVector b({'1'_b, '1'_b, '1'_b, '0'_b});
    auto c = l & b;
    static_assert(std::is_same_v<decltype(c), LogicVector>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], 'X'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, DynBitLogicOrPromotesToLogic) {
    // BitArray on LHS, LogicArray on RHS -- promotion still happens.
    BitVector b({'0'_b, '0'_b, '1'_b, '0'_b});
    LogicVector l({'1'_l, '0'_l, '0'_l, 'X'_l});
    auto c = b | l;
    static_assert(std::is_same_v<decltype(c), LogicVector>);
    EXPECT_EQ(c[3], '1'_l);
    EXPECT_EQ(c[2], '0'_l);
    EXPECT_EQ(c[1], '1'_l);
    EXPECT_EQ(c[0], 'X'_l);
}

TEST(TestBitArray, StaticDynMixedYieldsDynLogic) {
    // Static LogicArray + dynamic BitArray -- dynamic operand forces Vector,
    // and Logic wins for the element type.
    auto l = "0101"_l;
    BitVector b({'1'_b, '1'_b, '0'_b, '0'_b});
    auto c = l & b;
    static_assert(std::is_same_v<decltype(c), LogicVector>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, DynBitStaticLogicAndPromotesToDynLogic) {
    BitVector b({'1'_b, '1'_b, '0'_b, '0'_b});
    auto l = "0101"_l;
    auto c = b & l;
    static_assert(std::is_same_v<decltype(c), LogicVector>);
    EXPECT_EQ(c[3], '0'_l);
    EXPECT_EQ(c[2], '1'_l);
    EXPECT_EQ(c[1], '0'_l);
    EXPECT_EQ(c[0], '0'_l);
}

TEST(TestBitArray, StaticBitDynLogicOrPromotesToDynLogic) {
    auto b = "0010"_b;
    LogicVector l({'1'_l, '0'_l, '0'_l, 'X'_l});
    auto c = b | l;
    static_assert(std::is_same_v<decltype(c), LogicVector>);
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
    static_assert(std::is_same_v<decltype(b), BitVector>);
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
    static_assert(std::is_same_v<decltype(b), BitVector>);
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
    static_assert(std::is_same_v<decltype(b), BitVector>);
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
    static_assert(std::is_same_v<decltype(a.resolve(ResolveMethod::ZEROS)), BitVector>);
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

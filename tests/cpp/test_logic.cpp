// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <format>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace coconext::types;

// Test Logic conversions from various types
TEST(TestLogic, LogicConversions) {
    EXPECT_EQ('U'_l, 'u'_l);
    EXPECT_EQ(Logic('U'_l), 'U'_l);
    EXPECT_EQ(Logic('U'), 'U'_l);
    EXPECT_EQ(Logic('u'), 'U'_l);
    EXPECT_EQ(Logic("U"), 'U'_l);
    EXPECT_EQ(Logic("u"), 'U'_l);

    EXPECT_EQ('X'_l, 'x'_l);
    EXPECT_EQ(Logic('X'_l), 'X'_l);
    EXPECT_EQ(Logic('X'), 'X'_l);
    EXPECT_EQ(Logic('x'), 'X'_l);
    EXPECT_EQ(Logic("X"), 'X'_l);
    EXPECT_EQ(Logic("x"), 'X'_l);

    EXPECT_EQ('0'_l, Logic('0'));
    EXPECT_EQ('0'_l, Logic("0"));
    EXPECT_EQ('0'_l, Logic(0));
    EXPECT_EQ('0'_l, Logic(false));
    EXPECT_EQ(Logic('0'_l), '0'_l);

    EXPECT_EQ('1'_l, Logic('1'));
    EXPECT_EQ('1'_l, Logic("1"));
    EXPECT_EQ('1'_l, Logic(1));
    EXPECT_EQ('1'_l, Logic(true));
    EXPECT_EQ(Logic('1'_l), '1'_l);

    EXPECT_EQ('Z'_l, 'z'_l);
    EXPECT_EQ(Logic('Z'_l), 'Z'_l);
    EXPECT_EQ(Logic('Z'), 'Z'_l);
    EXPECT_EQ(Logic('z'), 'Z'_l);
    EXPECT_EQ(Logic("Z"), 'Z'_l);
    EXPECT_EQ(Logic("z"), 'Z'_l);

    EXPECT_EQ('W'_l, 'w'_l);
    EXPECT_EQ(Logic('W'_l), 'W'_l);
    EXPECT_EQ(Logic('W'), 'W'_l);
    EXPECT_EQ(Logic('w'), 'W'_l);
    EXPECT_EQ(Logic("W"), 'W'_l);
    EXPECT_EQ(Logic("w"), 'W'_l);

    EXPECT_EQ('L'_l, 'l'_l);
    EXPECT_EQ(Logic('L'_l), 'L'_l);
    EXPECT_EQ(Logic('L'), 'L'_l);
    EXPECT_EQ(Logic('l'), 'L'_l);
    EXPECT_EQ(Logic("L"), 'L'_l);
    EXPECT_EQ(Logic("l"), 'L'_l);

    EXPECT_EQ('H'_l, 'h'_l);
    EXPECT_EQ(Logic('H'_l), 'H'_l);
    EXPECT_EQ(Logic('H'), 'H'_l);
    EXPECT_EQ(Logic('h'), 'H'_l);
    EXPECT_EQ(Logic("H"), 'H'_l);
    EXPECT_EQ(Logic("h"), 'H'_l);

    EXPECT_EQ('-'_l, Logic::DC);
    EXPECT_EQ(Logic('-'_l), '-'_l);
    EXPECT_EQ(Logic('-'), '-'_l);
    EXPECT_EQ(Logic("-"), '-'_l);

    // Test invalid conversions
    EXPECT_THROW(Logic('j'), std::invalid_argument);
    EXPECT_THROW(Logic(2), std::invalid_argument);
    EXPECT_THROW(Logic("lol"), std::invalid_argument);
}

TEST(TestBit, BitConversions) {
    EXPECT_EQ('0'_b, Bit('0'));
    EXPECT_EQ('1'_b, Bit('1'));
    EXPECT_EQ('0'_b, Bit("0"));
    EXPECT_EQ('1'_b, Bit("1"));
    EXPECT_EQ('0'_b, Bit(0));
    EXPECT_EQ('1'_b, Bit(1));
    EXPECT_EQ('0'_b, Bit(false));
    EXPECT_EQ('1'_b, Bit(true));

    // Test invalid conversions
    EXPECT_THROW(Bit('2'), std::invalid_argument);
    EXPECT_THROW(Bit("10"), std::invalid_argument);
    EXPECT_THROW(Bit(2), std::invalid_argument);
}

// Test Logic bool conversions
TEST(TestLogic, LogicBoolConversions) {
    // Convertible to true
    EXPECT_EQ('1'_l.resolve(ResolveMethod::WEAK).has_value(), true);
    EXPECT_EQ('H'_l.resolve(ResolveMethod::WEAK).has_value(), true);

    // Convertible to false
    EXPECT_EQ('0'_l.resolve(ResolveMethod::WEAK).has_value(), true);
    EXPECT_EQ('L'_l.resolve(ResolveMethod::WEAK).has_value(), true);

    // Non-convertible values
    EXPECT_EQ('X'_l.resolve(ResolveMethod::WEAK).has_value(), false);
    EXPECT_EQ('Z'_l.resolve(ResolveMethod::WEAK).has_value(), false);
    EXPECT_EQ('U'_l.resolve(ResolveMethod::WEAK).has_value(), false);
    EXPECT_EQ('W'_l.resolve(ResolveMethod::WEAK).has_value(), false);
    EXPECT_EQ('-'_l.resolve(ResolveMethod::WEAK).has_value(), false);
}

TEST(TestBit, BitBoolConversions) {
    EXPECT_EQ('0'_b.resolve(ResolveMethod::WEAK).has_value(), true);
    EXPECT_EQ('1'_b.resolve(ResolveMethod::WEAK).has_value(), true);
}

TEST(TestLogic, LogicResolvabilityUnderEachMethod) {
    // ERROR: only 0/1 are resolvable.
    EXPECT_TRUE('0'_l.resolve(ResolveMethod::ERROR).has_value());
    EXPECT_TRUE('1'_l.resolve(ResolveMethod::ERROR).has_value());
    EXPECT_FALSE('L'_l.resolve(ResolveMethod::ERROR).has_value());
    EXPECT_FALSE('H'_l.resolve(ResolveMethod::ERROR).has_value());
    EXPECT_FALSE('X'_l.resolve(ResolveMethod::ERROR).has_value());
    EXPECT_FALSE('Z'_l.resolve(ResolveMethod::ERROR).has_value());
    EXPECT_FALSE('U'_l.resolve(ResolveMethod::ERROR).has_value());
    EXPECT_FALSE('W'_l.resolve(ResolveMethod::ERROR).has_value());
    EXPECT_FALSE('-'_l.resolve(ResolveMethod::ERROR).has_value());

    // WEAK: 0/1/L/H pass; metavalues (incl. W) do not.
    EXPECT_TRUE('0'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_TRUE('1'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_TRUE('L'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_TRUE('H'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('X'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('Z'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('U'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('W'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('-'_l.resolve(ResolveMethod::WEAK).has_value());

    // ZEROS / ONES / RANDOM: always resolvable (no value can throw).
    for (auto m : {ResolveMethod::ZEROS, ResolveMethod::ONES, ResolveMethod::RANDOM}) {
        EXPECT_TRUE('0'_l.resolve(m).has_value());
        EXPECT_TRUE('X'_l.resolve(m).has_value());
        EXPECT_TRUE('U'_l.resolve(m).has_value());
        EXPECT_TRUE('-'_l.resolve(m).has_value());
    }
}

// Test Logic string conversions
TEST(TestLogic, LogicStringConversions) {
    EXPECT_EQ(to_string('0'_l), "0");
    EXPECT_EQ(to_string('1'_l), "1");
    EXPECT_EQ(to_string('X'_l), "X");
    EXPECT_EQ(to_string('Z'_l), "Z");
}

TEST(TestBit, BitStringConversions) {
    EXPECT_EQ(to_string('0'_b), "0");
    EXPECT_EQ(to_string('1'_b), "1");
}

TEST(TestLogic, LogicFormatter) {
    EXPECT_EQ(std::format("{}", '0'_l), "Logic{0}");
    EXPECT_EQ(std::format("{}", '1'_l), "Logic{1}");
    EXPECT_EQ(std::format("{}", 'X'_l), "Logic{X}");
    EXPECT_EQ(std::format("{}", 'Z'_l), "Logic{Z}");
}

TEST(TestBit, BitFormatter) {
    EXPECT_EQ(std::format("{}", '0'_b), "Bit{0}");
    EXPECT_EQ(std::format("{}", '1'_b), "Bit{1}");
}

TEST(TestLogic, LogicCharConversions) {
    EXPECT_EQ(char('0'_l), '0');
    EXPECT_EQ(char('1'_l), '1');
    EXPECT_EQ(char('X'_l), 'X');
    EXPECT_EQ(char('Z'_l), 'Z');
}

TEST(TestBit, BitCharConversions) {
    EXPECT_EQ(char('0'_b), '0');
    EXPECT_EQ(char('1'_b), '1');
}

// Test Logic int conversions
TEST(TestLogic, LogicIntConversions) {
    EXPECT_EQ(int('0'_l), 0);
    EXPECT_EQ(int('1'_l), 1);
    EXPECT_EQ(int('L'_l), 0);
    EXPECT_EQ(int('H'_l), 1);

    // Non-convertible values should throw. Casting to (void) inside
    // EXPECT_THROW silences clang's unused-value warning without changing
    // when the operator conversion runs.
    EXPECT_THROW((void)int('X'_l), std::invalid_argument);
    EXPECT_THROW((void)int('Z'_l), std::invalid_argument);
    EXPECT_THROW((void)int('U'_l), std::invalid_argument);
    EXPECT_THROW((void)int('W'_l), std::invalid_argument);
    EXPECT_THROW((void)int('-'_l), std::invalid_argument);
}

TEST(TestBit, BitIntConversions) {
    EXPECT_EQ(int('0'_b), 0);
    EXPECT_EQ(int('1'_b), 1);
}

// Test Logic AND operator
TEST(TestLogic, LogicAnd) {
    EXPECT_EQ(('0'_l & 'Z'_l), '0'_l);
    EXPECT_EQ(('1'_l & '1'_l), '1'_l);
    EXPECT_EQ(('X'_l & 'Z'_l), 'X'_l);
}

TEST(TestBit, BitAnd) {
    EXPECT_EQ(('0'_b & '0'_b), '0'_b);
    EXPECT_EQ(('1'_b & '1'_b), '1'_b);
    EXPECT_EQ(('0'_b & '1'_b), '0'_b);
}

// Test Logic OR operator
TEST(TestLogic, LogicOr) {
    EXPECT_EQ(('1'_l | 'Z'_l), '1'_l);
    EXPECT_EQ(('0'_l | '0'_l), '0'_l);
    EXPECT_EQ(('X'_l | 'Z'_l), 'X'_l);
}

TEST(TestBit, BitOr) {
    EXPECT_EQ(('0'_b | '0'_b), '0'_b);
    EXPECT_EQ(('1'_b | '1'_b), '1'_b);
    EXPECT_EQ(('0'_b | '1'_b), '1'_b);
}

// Test Logic XOR operator
TEST(TestLogic, LogicXor) {
    EXPECT_EQ(('1'_l ^ '1'_l), '0'_l);
    EXPECT_EQ(('1'_l ^ 'X'_l), 'X'_l);
    EXPECT_EQ(('1'_l ^ '0'_l), '1'_l);
}

TEST(TestBit, BitXor) {
    EXPECT_EQ(('1'_b ^ '1'_b), '0'_b);
    EXPECT_EQ(('1'_b ^ '0'_b), '1'_b);
    EXPECT_EQ(('0'_b ^ '0'_b), '0'_b);
}

// Test Logic NOT operator
TEST(TestLogic, LogicInvert) {
    EXPECT_EQ(~'0'_l, '1'_l);
    EXPECT_EQ(~'1'_l, '0'_l);
    EXPECT_EQ(~'X'_l, 'X'_l);
    EXPECT_EQ(~'Z'_l, 'X'_l);
}

TEST(TestBit, BitInvert) {
    EXPECT_EQ(~'0'_b, '1'_b);
    EXPECT_EQ(~'1'_b, '0'_b);
}

// Test Logic resolve with different methods
TEST(TestLogic, LogicResolve) {
    // Test WEAK resolution: 0/1 pass through; L/H map to 0/1; everything else
    // returns nullopt (the "not resolvable under WEAK" tier matches L/H plus
    // 0/1). optional<Bit> compares equal to Bit when engaged.
    EXPECT_EQ('0'_l.resolve(ResolveMethod::WEAK), '0'_b);
    EXPECT_EQ('1'_l.resolve(ResolveMethod::WEAK), '1'_b);
    EXPECT_EQ('L'_l.resolve(ResolveMethod::WEAK), '0'_b);
    EXPECT_EQ('H'_l.resolve(ResolveMethod::WEAK), '1'_b);
    EXPECT_EQ('U'_l.resolve(ResolveMethod::WEAK), std::nullopt);
    EXPECT_EQ('X'_l.resolve(ResolveMethod::WEAK), std::nullopt);
    EXPECT_EQ('Z'_l.resolve(ResolveMethod::WEAK), std::nullopt);
    EXPECT_EQ('W'_l.resolve(ResolveMethod::WEAK), std::nullopt);
    EXPECT_EQ('-'_l.resolve(ResolveMethod::WEAK), std::nullopt);

    // Test ZEROS resolution
    EXPECT_EQ('U'_l.resolve(ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ('X'_l.resolve(ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ('0'_l.resolve(ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ('1'_l.resolve(ResolveMethod::ZEROS), '1'_b);
    EXPECT_EQ('Z'_l.resolve(ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ('W'_l.resolve(ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ('L'_l.resolve(ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ('H'_l.resolve(ResolveMethod::ZEROS), '1'_b);
    EXPECT_EQ('-'_l.resolve(ResolveMethod::ZEROS), '0'_b);

    // Test ONES resolution
    EXPECT_EQ('U'_l.resolve(ResolveMethod::ONES), '1'_b);
    EXPECT_EQ('X'_l.resolve(ResolveMethod::ONES), '1'_b);
    EXPECT_EQ('0'_l.resolve(ResolveMethod::ONES), '0'_b);
    EXPECT_EQ('1'_l.resolve(ResolveMethod::ONES), '1'_b);
    EXPECT_EQ('Z'_l.resolve(ResolveMethod::ONES), '1'_b);
    EXPECT_EQ('W'_l.resolve(ResolveMethod::ONES), '1'_b);
    EXPECT_EQ('L'_l.resolve(ResolveMethod::ONES), '0'_b);
    EXPECT_EQ('H'_l.resolve(ResolveMethod::ONES), '1'_b);
    EXPECT_EQ('-'_l.resolve(ResolveMethod::ONES), '1'_b);

    // Test RANDOM resolution
    auto resolved_u = 'U'_l.resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_u == '0'_b || resolved_u == '1'_b);

    auto resolved_x = 'X'_l.resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_x == '0'_b || resolved_x == '1'_b);

    EXPECT_EQ('0'_l.resolve(ResolveMethod::RANDOM), '0'_b);
    EXPECT_EQ('1'_l.resolve(ResolveMethod::RANDOM), '1'_b);

    auto resolved_z = 'Z'_l.resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_z == '0'_b || resolved_z == '1'_b);

    auto resolved_w = 'W'_l.resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_w == '0'_b || resolved_w == '1'_b);

    EXPECT_EQ('L'_l.resolve(ResolveMethod::RANDOM), '0'_b);
    EXPECT_EQ('H'_l.resolve(ResolveMethod::RANDOM), '1'_b);

    auto resolved_dc = '-'_l.resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_dc == '0'_b || resolved_dc == '1'_b);

    // Test ERROR resolution
    EXPECT_EQ('0'_l.resolve(ResolveMethod::ERROR), '0'_b);
    EXPECT_EQ('1'_l.resolve(ResolveMethod::ERROR), '1'_b);
    EXPECT_EQ('U'_l.resolve(ResolveMethod::ERROR), std::nullopt);
    EXPECT_EQ('X'_l.resolve(ResolveMethod::ERROR), std::nullopt);
    EXPECT_EQ('Z'_l.resolve(ResolveMethod::ERROR), std::nullopt);
    EXPECT_EQ('W'_l.resolve(ResolveMethod::ERROR), std::nullopt);
    EXPECT_EQ('L'_l.resolve(ResolveMethod::ERROR), std::nullopt);
    EXPECT_EQ('H'_l.resolve(ResolveMethod::ERROR), std::nullopt);
    EXPECT_EQ('-'_l.resolve(ResolveMethod::ERROR), std::nullopt);

    // Out-of-range ResolveMethod hits the outer fall-through arm and returns
    // nullopt (the unified API doesn't throw on bad methods either).
    EXPECT_EQ('0'_l.resolve(static_cast<ResolveMethod>(99)), std::nullopt);
}

TEST(TestBit, BitResolve) {
    // Every Bit value is resolvable, so resolve() always returns an engaged
    // optional equal to the input -- including ERROR (which is nullopt on
    // Logic for non-binary values) and out-of-range method values.
    EXPECT_EQ('0'_b.resolve(ResolveMethod::ERROR), '0'_b);
    EXPECT_EQ('1'_b.resolve(ResolveMethod::ERROR), '1'_b);

    EXPECT_EQ('0'_b.resolve(ResolveMethod::WEAK), '0'_b);
    EXPECT_EQ('1'_b.resolve(ResolveMethod::WEAK), '1'_b);

    EXPECT_EQ('0'_b.resolve(ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ('1'_b.resolve(ResolveMethod::ZEROS), '1'_b);

    EXPECT_EQ('0'_b.resolve(ResolveMethod::ONES), '0'_b);
    EXPECT_EQ('1'_b.resolve(ResolveMethod::ONES), '1'_b);

    EXPECT_EQ('0'_b.resolve(ResolveMethod::RANDOM), '0'_b);
    EXPECT_EQ('1'_b.resolve(ResolveMethod::RANDOM), '1'_b);

    // Out-of-range method values are silently accepted (Bit::resolve is
    // noexcept and ignores its argument).
    EXPECT_EQ('0'_b.resolve(static_cast<ResolveMethod>(99)), '0'_b);
    EXPECT_EQ('1'_b.resolve(static_cast<ResolveMethod>(99)), '1'_b);
}

// No-arg resolve() defaults to WEAK -- mirroring the array-form shortcut.
TEST(TestLogic, LogicResolveNoArgDefaultsToWeak) {
    EXPECT_EQ('0'_l.resolve(), '0'_b);
    EXPECT_EQ('1'_l.resolve(), '1'_b);
    EXPECT_EQ('L'_l.resolve(), '0'_b);
    EXPECT_EQ('H'_l.resolve(), '1'_b);
    EXPECT_EQ('X'_l.resolve(), std::nullopt);
    EXPECT_EQ('U'_l.resolve(), std::nullopt);
    EXPECT_EQ('Z'_l.resolve(), std::nullopt);
    EXPECT_EQ('W'_l.resolve(), std::nullopt);
    EXPECT_EQ('-'_l.resolve(), std::nullopt);
}

TEST(TestBit, BitResolveNoArgAlwaysEngaged) {
    EXPECT_EQ('0'_b.resolve(), '0'_b);
    EXPECT_EQ('1'_b.resolve(), '1'_b);
}

// Stores values in std::vector to force runtime evaluation of conversions.
// These can be evaluated at compile time and reduce observed coverage.
TEST(TestLogic, RuntimeIntAndBitConversions) {
    std::vector<int> const ints{0, 1, 2};
    EXPECT_EQ(Logic(ints[0]), '0'_l);
    EXPECT_EQ(Logic(ints[1]), '1'_l);
    EXPECT_THROW((void)Logic(ints[2]), std::invalid_argument);
    EXPECT_EQ(Bit(ints[0]), '0'_b);
    EXPECT_EQ(Bit(ints[1]), '1'_b);
    EXPECT_THROW((void)Bit(ints[2]), std::invalid_argument);

    std::vector<Bit> const bits{'0'_b, '1'_b};
    EXPECT_EQ(Logic(bits[0]), '0'_l);
    EXPECT_EQ(Logic(bits[1]), '1'_l);
    EXPECT_EQ(bits[0].resolve(ResolveMethod::WEAK), '0'_b);
    EXPECT_EQ(bits[1].resolve(ResolveMethod::ZEROS), '1'_b);
}

// Test Logic resolvability via resolve(...).has_value()
TEST(TestLogic, LogicIsResolvable) {
    EXPECT_TRUE('0'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_TRUE('1'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_TRUE('L'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_TRUE('H'_l.resolve(ResolveMethod::WEAK).has_value());

    EXPECT_FALSE('U'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('X'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('Z'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('W'_l.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_FALSE('-'_l.resolve(ResolveMethod::WEAK).has_value());
}

TEST(TestBit, BitIsResolvable) {
    EXPECT_TRUE('0'_b.resolve(ResolveMethod::WEAK).has_value());
    EXPECT_TRUE('1'_b.resolve(ResolveMethod::WEAK).has_value());
}

TEST(TestLogic, LogicIsHashable) {
    std::hash<Logic> h;
    EXPECT_EQ(h('0'_l), h('0'_l));
    EXPECT_EQ(h('1'_l), h('1'_l));
    EXPECT_EQ(h('U'_l), h('U'_l));
    EXPECT_EQ(h('X'_l), h('X'_l));
    EXPECT_EQ(h('Z'_l), h('Z'_l));
    EXPECT_EQ(h('W'_l), h('W'_l));
    EXPECT_EQ(h('L'_l), h('L'_l));
    EXPECT_EQ(h('H'_l), h('H'_l));
    EXPECT_EQ(h('-'_l), h('-'_l));
    std::unordered_set<Logic> const same{'0'_l, '0'_l};
    EXPECT_EQ(same.size(), 1U);

    std::unordered_set<Logic> const different{
        '0'_l, '1'_l, 'X'_l, 'Z'_l, 'W'_l, 'L'_l, 'H'_l, '-'_l, 'U'_l
    };
    EXPECT_EQ(different.size(), 9U);
}

TEST(TestBit, BitIsHashable) {
    std::hash<Bit> h;
    EXPECT_EQ(h('0'_b), h('0'_b));
    EXPECT_EQ(h('1'_b), h('1'_b));
    std::unordered_set<Bit> const same{'0'_b, '0'_b};
    EXPECT_EQ(same.size(), 1U);

    std::unordered_set<Bit> const different{'0'_b, '1'_b};
    EXPECT_EQ(different.size(), 2U);
}

// -- Compound bitwise assignment -------------------------------------------

TEST(TestLogic, LogicCompoundAssignAnd) {
    Logic v = '1'_l;
    v &= '0'_l;
    EXPECT_EQ(v, '0'_l);
    v = 'X'_l;
    v &= '1'_l;
    EXPECT_EQ(v, 'X'_l);
}

TEST(TestLogic, LogicCompoundAssignOr) {
    Logic v = '0'_l;
    v |= '1'_l;
    EXPECT_EQ(v, '1'_l);
}

TEST(TestLogic, LogicCompoundAssignXor) {
    Logic v = '1'_l;
    v ^= '1'_l;
    EXPECT_EQ(v, '0'_l);
}

TEST(TestLogic, LogicCompoundAssignAcceptsBit) {
    // Bit implicitly converts to Logic, so `Logic &= Bit` resolves to the
    // Logic overload.
    Logic v = '1'_l;
    v &= '0'_b;
    EXPECT_EQ(v, '0'_l);
}

TEST(TestBit, BitCompoundAssignAnd) {
    Bit v = '1'_b;
    v &= '0'_b;
    EXPECT_EQ(v, '0'_b);
}

TEST(TestBit, BitCompoundAssignOr) {
    Bit v = '0'_b;
    v |= '1'_b;
    EXPECT_EQ(v, '1'_b);
}

TEST(TestBit, BitCompoundAssignXor) {
    Bit v = '1'_b;
    v ^= '1'_b;
    EXPECT_EQ(v, '0'_b);
}

TEST(TestLogic, InplaceNotLogic) {
    Logic v = '1'_l;
    inplace_not(v);
    EXPECT_EQ(v, '0'_l);
    v = 'X'_l;
    inplace_not(v);
    EXPECT_EQ(v, 'X'_l);  // ~X = X
}

TEST(TestBit, InplaceNotBit) {
    Bit v = '1'_b;
    inplace_not(v);
    EXPECT_EQ(v, '0'_b);
}

TEST(TestLogic, InplaceNotReturnsReference) {
    Logic v = '1'_l;
    auto& ref = inplace_not(v);
    EXPECT_EQ(&ref, &v);
}

// -- Bit contextual conversion to bool -------------------------------------

TEST(TestBit, BitImplicitToBool) {
    // `operator bool` is explicit, but contextual conversion in `if`/`while`/
    // logical operators still accepts it.
    if (!('1'_b)) {
        FAIL() << "Bit('1') should convert to true";
    }
    if ('0'_b) {
        FAIL() << "Bit('0') should convert to false";
    }
}

// -- Logic contextual conversion to bool -----------------------------------

TEST(TestLogic, LogicContextualBool) {
    if (!static_cast<bool>('1'_l)) {
        FAIL() << "Logic('1') should convert to true";
    }
    if (static_cast<bool>('0'_l)) {
        FAIL() << "Logic('0') should convert to false";
    }
    if (!static_cast<bool>('H'_l)) {
        FAIL() << "Logic('H') should convert to true";
    }
    if (static_cast<bool>('L'_l)) {
        FAIL() << "Logic('L') should convert to false";
    }
}

TEST(TestLogic, LogicBoolConversionThrowsOnMetavalue) {
    // operator bool throws for values outside {0, 1, L, H}.
    std::vector<Logic> const bad{'X'_l, 'Z'_l, 'U'_l, 'W'_l, '-'_l};
    for (auto const& v : bad) {
        EXPECT_THROW((void)static_cast<bool>(v), std::out_of_range);
    }
}

// -- Bit(Logic) explicit constructor ---------------------------------------

TEST(TestBit, BitFromLogic) {
    EXPECT_EQ(Bit('0'_l), '0'_b);
    EXPECT_EQ(Bit('1'_l), '1'_b);
    EXPECT_EQ(Bit('L'_l), '0'_b);
    EXPECT_EQ(Bit('H'_l), '1'_b);
}

TEST(TestBit, BitFromLogicThrowsOnMetavalue) {
    std::vector<Logic> const bad{'X'_l, 'Z'_l, 'U'_l, 'W'_l, '-'_l};
    for (auto const& v : bad) {
        EXPECT_THROW((void)Bit(v), std::invalid_argument);
    }
}

// -- Templated egress conversions ------------------------------------------
//
// The int/char egress operators are templates on Integer/Character; existing
// tests only exercise them at `int` / `char`. Confirm they also instantiate
// (and produce the right result) for other integer widths and character
// types.

TEST(TestLogic, LogicEgressToOtherIntTypes) {
    EXPECT_EQ(static_cast<int64_t>('1'_l), int64_t{1});
    EXPECT_EQ(static_cast<uint8_t>('L'_l), uint8_t{0});
    EXPECT_EQ(static_cast<unsigned long>('H'_l), 1UL);
}

TEST(TestBit, BitEgressToOtherIntTypes) {
    EXPECT_EQ(static_cast<int64_t>('1'_b), int64_t{1});
    EXPECT_EQ(static_cast<uint8_t>('0'_b), uint8_t{0});
    EXPECT_EQ(static_cast<unsigned long>('1'_b), 1UL);
}

TEST(TestLogic, LogicEgressToWchar) {
    EXPECT_EQ(static_cast<wchar_t>('X'_l), L'X');
    EXPECT_EQ(static_cast<char32_t>('-'_l), U'-');
}

TEST(TestBit, BitEgressToWchar) {
    EXPECT_EQ(static_cast<wchar_t>('1'_b), L'1');
    EXPECT_EQ(static_cast<char32_t>('0'_b), U'0');
}

// -- Cross-type equality is deleted ---------------------------------------

TEST(TestLogic, CrossTypeEqualityDeleted) {
    // Logic == Bit and Bit == Logic are deleted overloads; only the
    // same-type == operators are viable. Compile-time check.
    static_assert(std::equality_comparable<Logic>);
    static_assert(std::equality_comparable<Bit>);
    static_assert(!std::equality_comparable_with<Logic, Bit>);
}

// -- consteval UDLs reject bad values at compile time ----------------------

TEST(TestLogic, UdlConsteval) {
    // `_l` and `_b` are consteval, so evaluation happens at compile time. Any
    // non-constant argument or invalid char is a hard compile error rather
    // than a runtime throw. This test just documents the successful cases;
    // negative cases live in the language's static checking.
    constexpr auto l = 'X'_l;
    constexpr auto b = '1'_b;
    static_assert(l == Logic::X);
    static_assert(b == Bit::_1);
}

// LCOV_EXCL_BR_STOP

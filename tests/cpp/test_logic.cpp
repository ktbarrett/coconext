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
    EXPECT_EQ(to_logic('U'), 'U'_l);
    EXPECT_EQ(to_logic('u'), 'U'_l);
    EXPECT_EQ(to_logic("U"), 'U'_l);
    EXPECT_EQ(to_logic("u"), 'U'_l);

    EXPECT_EQ('X'_l, 'x'_l);
    EXPECT_EQ(Logic('X'_l), 'X'_l);
    EXPECT_EQ(to_logic('X'), 'X'_l);
    EXPECT_EQ(to_logic('x'), 'X'_l);
    EXPECT_EQ(to_logic("X"), 'X'_l);
    EXPECT_EQ(to_logic("x"), 'X'_l);

    EXPECT_EQ('0'_l, to_logic('0'));
    EXPECT_EQ('0'_l, to_logic("0"));
    EXPECT_EQ('0'_l, to_logic(0));
    EXPECT_EQ('0'_l, to_logic(false));
    EXPECT_EQ(Logic('0'_l), '0'_l);

    EXPECT_EQ('1'_l, to_logic('1'));
    EXPECT_EQ('1'_l, to_logic("1"));
    EXPECT_EQ('1'_l, to_logic(1));
    EXPECT_EQ('1'_l, to_logic(true));
    EXPECT_EQ(Logic('1'_l), '1'_l);

    EXPECT_EQ('Z'_l, 'z'_l);
    EXPECT_EQ(Logic('Z'_l), 'Z'_l);
    EXPECT_EQ(to_logic('Z'), 'Z'_l);
    EXPECT_EQ(to_logic('z'), 'Z'_l);
    EXPECT_EQ(to_logic("Z"), 'Z'_l);
    EXPECT_EQ(to_logic("z"), 'Z'_l);

    EXPECT_EQ('W'_l, 'w'_l);
    EXPECT_EQ(Logic('W'_l), 'W'_l);
    EXPECT_EQ(to_logic('W'), 'W'_l);
    EXPECT_EQ(to_logic('w'), 'W'_l);
    EXPECT_EQ(to_logic("W"), 'W'_l);
    EXPECT_EQ(to_logic("w"), 'W'_l);

    EXPECT_EQ('L'_l, 'l'_l);
    EXPECT_EQ(Logic('L'_l), 'L'_l);
    EXPECT_EQ(to_logic('L'), 'L'_l);
    EXPECT_EQ(to_logic('l'), 'L'_l);
    EXPECT_EQ(to_logic("L"), 'L'_l);
    EXPECT_EQ(to_logic("l"), 'L'_l);

    EXPECT_EQ('H'_l, 'h'_l);
    EXPECT_EQ(Logic('H'_l), 'H'_l);
    EXPECT_EQ(to_logic('H'), 'H'_l);
    EXPECT_EQ(to_logic('h'), 'H'_l);
    EXPECT_EQ(to_logic("H"), 'H'_l);
    EXPECT_EQ(to_logic("h"), 'H'_l);

    EXPECT_EQ('-'_l, Logic::DC);
    EXPECT_EQ(Logic('-'_l), '-'_l);
    EXPECT_EQ(to_logic('-'), '-'_l);
    EXPECT_EQ(to_logic("-"), '-'_l);

    // Test invalid conversions
    EXPECT_THROW(to_logic('j'), std::invalid_argument);
    EXPECT_THROW(to_logic(2), std::invalid_argument);
    EXPECT_THROW(to_logic("lol"), std::invalid_argument);
}

TEST(TestBit, BitConversions) {
    EXPECT_EQ('0'_b, to_bit('0'));
    EXPECT_EQ('1'_b, to_bit('1'));
    EXPECT_EQ('0'_b, to_bit("0"));
    EXPECT_EQ('1'_b, to_bit("1"));
    EXPECT_EQ('0'_b, to_bit(0));
    EXPECT_EQ('1'_b, to_bit(1));
    EXPECT_EQ('0'_b, to_bit(false));
    EXPECT_EQ('1'_b, to_bit(true));

    // Test invalid conversions
    EXPECT_THROW(to_bit('2'), std::invalid_argument);
    EXPECT_THROW(to_bit("10"), std::invalid_argument);
    EXPECT_THROW(to_bit(2), std::invalid_argument);
}

// Test Logic bool conversions
TEST(TestLogic, LogicBoolConversions) {
    // Convertible to true
    EXPECT_EQ(('1'_l).is_resolvable(), true);
    EXPECT_EQ(('H'_l).is_resolvable(), true);

    // Convertible to false
    EXPECT_EQ(('0'_l).is_resolvable(), true);
    EXPECT_EQ(('L'_l).is_resolvable(), true);

    // Non-convertible values
    EXPECT_EQ(('X'_l).is_resolvable(), false);
    EXPECT_EQ(('Z'_l).is_resolvable(), false);
    EXPECT_EQ(('U'_l).is_resolvable(), false);
    EXPECT_EQ(('W'_l).is_resolvable(), false);
    EXPECT_EQ(('-'_l).is_resolvable(), false);
}

TEST(TestBit, BitBoolConversions) {
    EXPECT_EQ(('0'_b).is_resolvable(), true);
    EXPECT_EQ(('1'_b).is_resolvable(), true);
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
    EXPECT_EQ(to_char('0'_l), '0');
    EXPECT_EQ(to_char('1'_l), '1');
    EXPECT_EQ(to_char('X'_l), 'X');
    EXPECT_EQ(to_char('Z'_l), 'Z');
}

TEST(TestBit, BitCharConversions) {
    EXPECT_EQ(to_char('0'_b), '0');
    EXPECT_EQ(to_char('1'_b), '1');
}

// Test Logic int conversions
TEST(TestLogic, LogicIntConversions) {
    EXPECT_EQ(to_int('0'_l), 0);
    EXPECT_EQ(to_int('1'_l), 1);
    EXPECT_EQ(to_int('L'_l), 0);
    EXPECT_EQ(to_int('H'_l), 1);

    // Non-convertible values should throw
    EXPECT_THROW(to_int('X'_l), std::invalid_argument);
    EXPECT_THROW(to_int('Z'_l), std::invalid_argument);
    EXPECT_THROW(to_int('U'_l), std::invalid_argument);
    EXPECT_THROW(to_int('W'_l), std::invalid_argument);
    EXPECT_THROW(to_int('-'_l), std::invalid_argument);
}

TEST(TestBit, BitIntConversions) {
    EXPECT_EQ(to_int('0'_b), 0);
    EXPECT_EQ(to_int('1'_b), 1);
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
    // Test WEAK resolution
    EXPECT_EQ(('U'_l).resolve(ResolveMethod::WEAK), 'U'_l);
    EXPECT_EQ(('X'_l).resolve(ResolveMethod::WEAK), 'X'_l);
    EXPECT_EQ(('0'_l).resolve(ResolveMethod::WEAK), '0'_l);
    EXPECT_EQ(('1'_l).resolve(ResolveMethod::WEAK), '1'_l);
    EXPECT_EQ(('Z'_l).resolve(ResolveMethod::WEAK), 'Z'_l);
    EXPECT_EQ(('W'_l).resolve(ResolveMethod::WEAK), 'X'_l);
    EXPECT_EQ(('L'_l).resolve(ResolveMethod::WEAK), '0'_l);
    EXPECT_EQ(('H'_l).resolve(ResolveMethod::WEAK), '1'_l);
    EXPECT_EQ(('-'_l).resolve(ResolveMethod::WEAK), '-'_l);

    // Test ZEROS resolution
    EXPECT_EQ(('U'_l).resolve(ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(('X'_l).resolve(ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(('0'_l).resolve(ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(('1'_l).resolve(ResolveMethod::ZEROS), '1'_l);
    EXPECT_EQ(('Z'_l).resolve(ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(('W'_l).resolve(ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(('L'_l).resolve(ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(('H'_l).resolve(ResolveMethod::ZEROS), '1'_l);
    EXPECT_EQ(('-'_l).resolve(ResolveMethod::ZEROS), '0'_l);

    // Test ONES resolution
    EXPECT_EQ(('U'_l).resolve(ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(('X'_l).resolve(ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(('0'_l).resolve(ResolveMethod::ONES), '0'_l);
    EXPECT_EQ(('1'_l).resolve(ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(('Z'_l).resolve(ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(('W'_l).resolve(ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(('L'_l).resolve(ResolveMethod::ONES), '0'_l);
    EXPECT_EQ(('H'_l).resolve(ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(('-'_l).resolve(ResolveMethod::ONES), '1'_l);

    // Test RANDOM resolution
    auto resolved_u = ('U'_l).resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_u == '0'_l || resolved_u == '1'_l);

    auto resolved_x = ('X'_l).resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_x == '0'_l || resolved_x == '1'_l);

    EXPECT_EQ(('0'_l).resolve(ResolveMethod::RANDOM), '0'_l);
    EXPECT_EQ(('1'_l).resolve(ResolveMethod::RANDOM), '1'_l);

    auto resolved_z = ('Z'_l).resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_z == '0'_l || resolved_z == '1'_l);

    auto resolved_w = ('W'_l).resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_w == '0'_l || resolved_w == '1'_l);

    EXPECT_EQ(('L'_l).resolve(ResolveMethod::RANDOM), '0'_l);
    EXPECT_EQ(('H'_l).resolve(ResolveMethod::RANDOM), '1'_l);

    auto resolved_dc = ('-'_l).resolve(ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_dc == '0'_l || resolved_dc == '1'_l);

    // Test ERROR resolution
    EXPECT_EQ(('0'_l).resolve(ResolveMethod::ERROR), '0'_l);
    EXPECT_EQ(('1'_l).resolve(ResolveMethod::ERROR), '1'_l);
    EXPECT_THROW((void)('U'_l).resolve(ResolveMethod::ERROR), std::invalid_argument);
    EXPECT_THROW((void)('X'_l).resolve(ResolveMethod::ERROR), std::invalid_argument);
    EXPECT_THROW((void)('Z'_l).resolve(ResolveMethod::ERROR), std::invalid_argument);
    EXPECT_THROW((void)('W'_l).resolve(ResolveMethod::ERROR), std::invalid_argument);
    EXPECT_THROW((void)('L'_l).resolve(ResolveMethod::ERROR), std::invalid_argument);
    EXPECT_THROW((void)('H'_l).resolve(ResolveMethod::ERROR), std::invalid_argument);
    EXPECT_THROW((void)('-'_l).resolve(ResolveMethod::ERROR), std::invalid_argument);

    // Out-of-range ResolveMethod hits the outer `default:` arm.
    EXPECT_THROW(
        (void)('0'_l).resolve(static_cast<ResolveMethod>(99)), std::invalid_argument
    );
}

TEST(TestBit, BitResolve) {
    EXPECT_EQ(('0'_b).resolve(ResolveMethod::WEAK), '0'_b);
    EXPECT_EQ(('1'_b).resolve(ResolveMethod::WEAK), '1'_b);

    EXPECT_EQ(('0'_b).resolve(ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ(('1'_b).resolve(ResolveMethod::ZEROS), '1'_b);

    EXPECT_EQ(('0'_b).resolve(ResolveMethod::ONES), '0'_b);
    EXPECT_EQ(('1'_b).resolve(ResolveMethod::ONES), '1'_b);

    EXPECT_EQ(('0'_b).resolve(ResolveMethod::RANDOM), '0'_b);
    EXPECT_EQ(('1'_b).resolve(ResolveMethod::RANDOM), '1'_b);
}

// Stores values in std::vector to force runtime evaluation of conversions.
// These can be evaluated at compile time and reduce observed coverage.
TEST(TestLogic, RuntimeIntAndBitConversions) {
    std::vector<int> const ints{0, 1, 2};
    EXPECT_EQ(to_logic(ints[0]), '0'_l);
    EXPECT_EQ(to_logic(ints[1]), '1'_l);
    EXPECT_THROW((void)to_logic(ints[2]), std::invalid_argument);
    EXPECT_EQ(to_bit(ints[0]), '0'_b);
    EXPECT_EQ(to_bit(ints[1]), '1'_b);
    EXPECT_THROW((void)to_bit(ints[2]), std::invalid_argument);

    std::vector<Bit> const bits{'0'_b, '1'_b};
    EXPECT_EQ(to_logic(bits[0]), '0'_l);
    EXPECT_EQ(to_logic(bits[1]), '1'_l);
    EXPECT_EQ((bits[0]).resolve(ResolveMethod::WEAK), '0'_b);
    EXPECT_EQ((bits[1]).resolve(ResolveMethod::ZEROS), '1'_b);
}

// Test Logic is_resolvable
TEST(TestLogic, LogicIsResolvable) {
    EXPECT_TRUE(('0'_l).is_resolvable());
    EXPECT_TRUE(('1'_l).is_resolvable());
    EXPECT_TRUE(('L'_l).is_resolvable());
    EXPECT_TRUE(('H'_l).is_resolvable());

    EXPECT_FALSE(('U'_l).is_resolvable());
    EXPECT_FALSE(('X'_l).is_resolvable());
    EXPECT_FALSE(('Z'_l).is_resolvable());
    EXPECT_FALSE(('W'_l).is_resolvable());
    EXPECT_FALSE(('-'_l).is_resolvable());
}

TEST(TestBit, BitIsResolvable) {
    EXPECT_TRUE(('0'_b).is_resolvable());
    EXPECT_TRUE(('1'_b).is_resolvable());
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
// LCOV_EXCL_BR_STOP

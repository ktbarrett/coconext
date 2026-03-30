#include <gtest/gtest.h>

#include <coconext/types.hpp>
#include <stdexcept>

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
    EXPECT_EQ(is_01('1'_l), true);
    EXPECT_EQ(is_01('H'_l), true);

    // Convertible to false
    EXPECT_EQ(is_01('0'_l), true);
    EXPECT_EQ(is_01('L'_l), true);

    // Non-convertible values
    EXPECT_EQ(is_01('X'_l), false);
    EXPECT_EQ(is_01('Z'_l), false);
    EXPECT_EQ(is_01('U'_l), false);
    EXPECT_EQ(is_01('W'_l), false);
    EXPECT_EQ(is_01('-'_l), false);
}

TEST(TestBit, BitBoolConversions) {
    EXPECT_EQ(is_01('0'_b), true);
    EXPECT_EQ(is_01('1'_b), true);
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
    EXPECT_EQ(resolve('U'_l, ResolveMethod::WEAK), 'U'_l);
    EXPECT_EQ(resolve('X'_l, ResolveMethod::WEAK), 'X'_l);
    EXPECT_EQ(resolve('0'_l, ResolveMethod::WEAK), '0'_l);
    EXPECT_EQ(resolve('1'_l, ResolveMethod::WEAK), '1'_l);
    EXPECT_EQ(resolve('Z'_l, ResolveMethod::WEAK), 'Z'_l);
    EXPECT_EQ(resolve('W'_l, ResolveMethod::WEAK), 'X'_l);
    EXPECT_EQ(resolve('L'_l, ResolveMethod::WEAK), '0'_l);
    EXPECT_EQ(resolve('H'_l, ResolveMethod::WEAK), '1'_l);
    EXPECT_EQ(resolve('-'_l, ResolveMethod::WEAK), '-'_l);

    // Test ZEROS resolution
    EXPECT_EQ(resolve('U'_l, ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(resolve('X'_l, ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(resolve('0'_l, ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(resolve('1'_l, ResolveMethod::ZEROS), '1'_l);
    EXPECT_EQ(resolve('Z'_l, ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(resolve('W'_l, ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(resolve('L'_l, ResolveMethod::ZEROS), '0'_l);
    EXPECT_EQ(resolve('H'_l, ResolveMethod::ZEROS), '1'_l);
    EXPECT_EQ(resolve('-'_l, ResolveMethod::ZEROS), '0'_l);

    // Test ONES resolution
    EXPECT_EQ(resolve('U'_l, ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(resolve('X'_l, ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(resolve('0'_l, ResolveMethod::ONES), '0'_l);
    EXPECT_EQ(resolve('1'_l, ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(resolve('Z'_l, ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(resolve('W'_l, ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(resolve('L'_l, ResolveMethod::ONES), '0'_l);
    EXPECT_EQ(resolve('H'_l, ResolveMethod::ONES), '1'_l);
    EXPECT_EQ(resolve('-'_l, ResolveMethod::ONES), '1'_l);

    // Test RANDOM resolution
    auto resolved_u = resolve('U'_l, ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_u == '0'_l || resolved_u == '1'_l);

    auto resolved_x = resolve('X'_l, ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_x == '0'_l || resolved_x == '1'_l);

    EXPECT_EQ(resolve('0'_l, ResolveMethod::RANDOM), '0'_l);
    EXPECT_EQ(resolve('1'_l, ResolveMethod::RANDOM), '1'_l);

    auto resolved_z = resolve('Z'_l, ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_z == '0'_l || resolved_z == '1'_l);

    auto resolved_w = resolve('W'_l, ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_w == '0'_l || resolved_w == '1'_l);

    EXPECT_EQ(resolve('L'_l, ResolveMethod::RANDOM), '0'_l);
    EXPECT_EQ(resolve('H'_l, ResolveMethod::RANDOM), '1'_l);

    auto resolved_dc = resolve('-'_l, ResolveMethod::RANDOM);
    EXPECT_TRUE(resolved_dc == '0'_l || resolved_dc == '1'_l);
}

TEST(TestBit, BitResolve) {
    EXPECT_EQ(resolve('0'_b, ResolveMethod::WEAK), '0'_b);
    EXPECT_EQ(resolve('1'_b, ResolveMethod::WEAK), '1'_b);

    EXPECT_EQ(resolve('0'_b, ResolveMethod::ZEROS), '0'_b);
    EXPECT_EQ(resolve('1'_b, ResolveMethod::ZEROS), '1'_b);

    EXPECT_EQ(resolve('0'_b, ResolveMethod::ONES), '0'_b);
    EXPECT_EQ(resolve('1'_b, ResolveMethod::ONES), '1'_b);

    EXPECT_EQ(resolve('0'_b, ResolveMethod::RANDOM), '0'_b);
    EXPECT_EQ(resolve('1'_b, ResolveMethod::RANDOM), '1'_b);
}

// Test Logic is_resolvable (checked via is_01)
TEST(TestLogic, LogicIsResolvable) {
    EXPECT_TRUE(is_01(Logic::_0));
    EXPECT_TRUE(is_01(Logic::_1));
    EXPECT_TRUE(is_01('L'_l));
    EXPECT_TRUE(is_01('H'_l));

    EXPECT_FALSE(is_01('U'_l));
    EXPECT_FALSE(is_01('X'_l));
    EXPECT_FALSE(is_01('Z'_l));
    EXPECT_FALSE(is_01('W'_l));
    EXPECT_FALSE(is_01('-'_l));
}

TEST(TestBit, BitIsResolvable) {
    EXPECT_TRUE(is_01('0'_b));
    EXPECT_TRUE(is_01('1'_b));
}

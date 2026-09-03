// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/types/dyn_fixed.hpp>
#include <string>

using coconext::types::as;
using coconext::types::BitVector;
using coconext::types::Direction;
using coconext::types::Range;
using coconext::types::detail::DynSfixed;
using coconext::types::detail::DynUfixed;

TEST(DynFixed, SignedConstructionAndShape) {
    Range range{3, Direction::DOWNTO, -4};
    auto value = as<DynSfixed>(BitVector("10101111", range));
    DynSfixed deferred = as(BitVector("10101111", range));

    EXPECT_EQ(value.range(), range);
    EXPECT_EQ(deferred, value);
    EXPECT_EQ(value.size(), 8);
    EXPECT_DOUBLE_EQ(static_cast<double>(value), -5.0625);
    EXPECT_EQ(value.raw_binary(), "10101111");
    EXPECT_TRUE(value.index(3));
    EXPECT_TRUE(value.index(-4));

    value.set_index(3, false);
    value.set_index(-4, false);
    EXPECT_DOUBLE_EQ(static_cast<double>(value), 2.875);
}

TEST(DynFixed, SignedArithmetic) {
    auto a = as<DynSfixed>(BitVector("101100110000", Range{5, Direction::DOWNTO, -6}));
    auto b = as<DynSfixed>(
        BitVector("111111110101000000000", Range{10, Direction::DOWNTO, -10})
    );

    auto sum = a + b;
    EXPECT_EQ(sum.range(), (Range{11, Direction::DOWNTO, -10}));
    EXPECT_DOUBLE_EQ(static_cast<double>(sum), -24.75);

    auto difference = a - b;
    EXPECT_EQ(difference.range(), (Range{11, Direction::DOWNTO, -10}));
    EXPECT_DOUBLE_EQ(static_cast<double>(difference), -13.75);

    auto product = a * b;
    EXPECT_EQ(product.range(), (Range{16, Direction::DOWNTO, -16}));
    EXPECT_DOUBLE_EQ(static_cast<double>(product), 105.875);

    auto quotient = as<DynSfixed>(BitVector("110", Range{2, Direction::DOWNTO, 0}))
                  / as<DynSfixed>(BitVector("011", Range{2, Direction::DOWNTO, 0}));
    EXPECT_EQ(quotient.range(), (Range{3, Direction::DOWNTO, -2}));
    EXPECT_DOUBLE_EQ(static_cast<double>(quotient), -0.75);

    auto remainder = as<DynSfixed>(BitVector("1011", Range{3, Direction::DOWNTO, 0}))
                   % as<DynSfixed>(BitVector("011", Range{2, Direction::DOWNTO, 0}));
    EXPECT_DOUBLE_EQ(static_cast<double>(remainder), -2.0);
}

TEST(DynFixed, SignedUnaryShiftAndCompound) {
    Range fractional{3, Direction::DOWNTO, -4};
    auto value = as<DynSfixed>(BitVector("11110000", fractional));

    EXPECT_DOUBLE_EQ(static_cast<double>(value << 1), -2.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(value >> 1), -0.5);
    EXPECT_DOUBLE_EQ(static_cast<double>(-value), 1.0);
    EXPECT_EQ((-value).range(), (Range{4, Direction::DOWNTO, -4}));
    EXPECT_DOUBLE_EQ(static_cast<double>(value.abs()), 1.0);

    auto wrapping = as<DynSfixed>(BitVector("0111", Range{3, Direction::DOWNTO, 0}));
    wrapping += as<DynSfixed>(BitVector("01", Range{1, Direction::DOWNTO, 0}));
    EXPECT_DOUBLE_EQ(static_cast<double>(wrapping), -8.0);

    auto fractional_compound =
        as<DynSfixed>(BitVector("101011", Range{3, Direction::DOWNTO, -2}));
    fractional_compound *= as<DynSfixed>(BitVector("011", Range{1, Direction::DOWNTO, -1}));
    EXPECT_DOUBLE_EQ(static_cast<double>(fractional_compound), -7.75);
}

TEST(DynFixed, UnsignedArithmetic) {
    Range range{3, Direction::DOWNTO, -4};
    auto value = as<DynUfixed>(BitVector("01010001", range));
    EXPECT_DOUBLE_EQ(static_cast<double>(value), 5.0625);
    EXPECT_EQ(value.raw_binary(), "01010001");

    auto sum = value + as<DynUfixed>(BitVector("01010", Range{2, Direction::DOWNTO, -2}));
    EXPECT_EQ(sum.range(), (Range{4, Direction::DOWNTO, -4}));
    EXPECT_DOUBLE_EQ(static_cast<double>(sum), 7.5625);

    auto difference = as<DynUfixed>(BitVector("0101", Range{3, Direction::DOWNTO, 0}))
                    - as<DynUfixed>(BitVector("111", Range{2, Direction::DOWNTO, 0}));
    EXPECT_DOUBLE_EQ(static_cast<double>(difference), -2.0);

    auto product = as<DynUfixed>(BitVector("010101", Range{3, Direction::DOWNTO, -2}))
                 * as<DynUfixed>(BitVector("0101", Range{2, Direction::DOWNTO, -1}));
    EXPECT_DOUBLE_EQ(static_cast<double>(product), 13.125);

    auto quotient = as<DynUfixed>(BitVector("10", Range{1, Direction::DOWNTO, 0}))
                  / as<DynUfixed>(BitVector("11", Range{1, Direction::DOWNTO, 0}));
    EXPECT_EQ(quotient.range(), (Range{1, Direction::DOWNTO, -2}));
    EXPECT_DOUBLE_EQ(static_cast<double>(quotient), 0.75);
}

TEST(DynFixed, WideStorageAndValidation) {
    Range wide_range{100, Direction::DOWNTO, -50};
    std::string wide_bits = std::string(98, '1') + "0101" + std::string(49, '0');
    auto wide = as<DynSfixed>(BitVector(wide_bits, wide_range));
    EXPECT_EQ(wide.size(), 151);
    EXPECT_DOUBLE_EQ(static_cast<double>(wide), -5.5);

    EXPECT_THROW(
        as<DynSfixed>(BitVector("0000", Range{0, Direction::TO, 3})), std::invalid_argument
    );
}

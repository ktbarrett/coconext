// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <algorithm>
#include <coconext/types/dyn_sfixed.hpp>
#include <coconext/types/sfixed.hpp>
#include <coconext/types/ufixed.hpp>
#include <format>
#include <limits>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>

using coconext::types::as;
using coconext::types::BitVector;
using coconext::types::Direction;
using coconext::types::DynSfixed;
using coconext::types::DynUfixed;
using coconext::types::overflow_mode;
using coconext::types::Range;
using coconext::types::round_mode;
using coconext::types::detail::DynSigned;
using coconext::types::detail::DynUnsigned;
using namespace coconext::types;

template <typename LHS, typename RHS>
concept DynamicUnsignedArithmetic =
    std::same_as<
        decltype(std::declval<LHS const&>() + std::declval<RHS const&>()),
        DynUfixed>
    && std::same_as<
        decltype(std::declval<LHS const&>() - std::declval<RHS const&>()),
        DynSfixed>
    && std::same_as<
        decltype(std::declval<LHS const&>() * std::declval<RHS const&>()),
        DynUfixed>
    && std::same_as<
        decltype(std::declval<LHS const&>() / std::declval<RHS const&>()),
        DynUfixed>
    && std::same_as<
        decltype(std::declval<LHS const&>() % std::declval<RHS const&>()),
        DynUfixed>;

template <typename LHS, typename RHS>
concept DynamicSignedArithmetic =
    std::same_as<
        decltype(std::declval<LHS const&>() + std::declval<RHS const&>()),
        DynSfixed>
    && std::same_as<
        decltype(std::declval<LHS const&>() - std::declval<RHS const&>()),
        DynSfixed>
    && std::same_as<
        decltype(std::declval<LHS const&>() * std::declval<RHS const&>()),
        DynSfixed>
    && std::same_as<
        decltype(std::declval<LHS const&>() / std::declval<RHS const&>()),
        DynSfixed>
    && std::same_as<
        decltype(std::declval<LHS const&>() % std::declval<RHS const&>()),
        DynSfixed>;

template <typename Target, typename Source>
concept ExplicitAsAvailable =
    requires(Source&& source) { as<Target>(std::forward<Source>(source)); };

template <typename Source>
concept HasBitVectorConversionOperator =
    requires(Source const& source) { source.operator BitVector(); };

template <typename Source>
concept HasDynamicIntegerConversions = requires(Source const& source) {
    source.to_signed(8);
    source.to_unsigned(8);
};

TEST(DynFixed, PublicTypesAndCrossKindConstruction) {
    static_assert(std::same_as<DynUfixed, coconext::types::detail::DynUfixed>);
    static_assert(std::same_as<DynSfixed, coconext::types::detail::DynSfixed>);
    static_assert(!std::constructible_from<DynUfixed, BitVector const&>);
    static_assert(!std::constructible_from<DynUfixed, BitVector&&>);
    static_assert(!std::constructible_from<DynSfixed, BitVector const&>);
    static_assert(!std::constructible_from<DynSfixed, BitVector&&>);
    static_assert(!std::constructible_from<DynUfixed, Ufixed<3, 0> const&>);
    static_assert(!std::constructible_from<DynSfixed, Sfixed<3, 0> const&>);
    static_assert(!std::constructible_from<DynUfixed, DynSfixed&&>);
    static_assert(!std::constructible_from<DynSfixed, DynUfixed&&>);
    static_assert(!HasBitVectorConversionOperator<DynUfixed>);
    static_assert(!HasBitVectorConversionOperator<DynSfixed>);
    static_assert(!HasDynamicIntegerConversions<DynUfixed>);
    static_assert(!HasDynamicIntegerConversions<DynSfixed>);
    static_assert(!std::constructible_from<BitVector, DynUfixed const&>);
    static_assert(!std::constructible_from<BitVector, DynSfixed const&>);

    Range const range{3, Direction::DOWNTO, 0};
    DynUfixed unsigned_value(range, 5);
    DynSfixed signed_value(range, -5);
    EXPECT_EQ(static_cast<int>(DynSfixed(range, unsigned_value)), 5);
    EXPECT_EQ(static_cast<int>(DynUfixed(range, DynSfixed(range, 5))), 5);
    EXPECT_THROW(DynUfixed(range, signed_value), std::out_of_range);
}

TEST(DynFixed, SignedConstructionAndShape) {
    Range range{3, Direction::DOWNTO, -4};
    auto value = as<DynSfixed>(BitVector("10101111", range));
    DynSfixed deferred = as(BitVector("10101111", range));

    EXPECT_EQ(value.range(), range);
    EXPECT_EQ(deferred, value);
    EXPECT_EQ(value.size(), 8);
    EXPECT_DOUBLE_EQ(static_cast<double>(value), -5.0625);
    EXPECT_EQ(value.raw_binary(), "10101111");
    EXPECT_EQ(value[3], Bit::_1);
    EXPECT_EQ(value[-4], Bit::_1);

    value[3] = Bit::_0;
    value[-4] = Bit::_0;
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

    auto to_value = as<DynSfixed>(BitVector("0000", Range{0, Direction::TO, 3}));
    EXPECT_EQ(to_value.range(), (Range{0, Direction::TO, 3}));
    EXPECT_THROW(static_cast<void>(static_cast<double>(to_value)), std::invalid_argument);
}

TEST(DynFixed, ConstructionConversionAndResizeParity) {
    Range const fractional{3, Direction::DOWNTO, -4};
    DynUfixed u(fractional, 5.0625);
    DynSfixed s(fractional, -5.0625);

    EXPECT_EQ(static_cast<int>(u), 5);
    EXPECT_EQ(static_cast<int>(s), -5);
    EXPECT_EQ(static_cast<unsigned>(u), 5U);
#if defined(__SIZEOF_INT128__)
    EXPECT_TRUE(static_cast<__uint128_t>(u) == __uint128_t{5});
    EXPECT_TRUE(static_cast<__int128_t>(s) == __int128_t{-5});
#endif
    EXPECT_THROW(static_cast<void>(static_cast<unsigned>(s)), std::out_of_range);
    EXPECT_THROW(
        static_cast<void>(
            static_cast<signed char>(DynUfixed({11, Direction::DOWNTO, 0}, 300))
        ),
        std::out_of_range
    );

    DynUnsigned dyn_unsigned(8, 5);
    DynSigned dyn_signed(8, -5);
    EXPECT_DOUBLE_EQ(static_cast<double>(DynUfixed(fractional, dyn_unsigned)), 5.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(DynSfixed(fractional, dyn_signed)), -5.0);
    EXPECT_THROW(DynUfixed(fractional, dyn_signed), std::out_of_range);

    coconext::types::Ufixed<3, -4> static_u(5.0625);
    EXPECT_DOUBLE_EQ(
        static_cast<double>(DynSfixed({4, Direction::DOWNTO, -4}, static_u)), 5.0625
    );

    Range const integer{10, Direction::DOWNTO, 0};
    DynUfixed positive({10, Direction::DOWNTO, -10}, 9.5);
    EXPECT_EQ(
        static_cast<int>(
            resize(positive, integer, overflow_mode::saturate, round_mode::truncate)
        ),
        9
    );
    EXPECT_EQ(
        static_cast<int>(
            resize(positive, integer, overflow_mode::saturate, round_mode::round_to_even)
        ),
        10
    );

    DynSfixed negative({10, Direction::DOWNTO, -10}, -9.5);
    EXPECT_EQ(
        static_cast<int>(
            resize(negative, integer, overflow_mode::saturate, round_mode::truncate)
        ),
        -10
    );
    EXPECT_EQ(
        static_cast<int>(
            resize(negative, integer, overflow_mode::saturate, round_mode::round_to_zero)
        ),
        -9
    );

    DynUfixed too_large({7, Direction::DOWNTO, 0}, 200);
    EXPECT_EQ(static_cast<int>(resize(too_large, Range{3, Direction::DOWNTO, 0})), 15);
    EXPECT_EQ(
        static_cast<int>(resize(
            too_large,
            {3, Direction::DOWNTO, 0},
            overflow_mode::wrap,
            round_mode::round_to_even
        )),
        8
    );

    EXPECT_THROW(DynUfixed(fractional, -1), std::out_of_range);
    EXPECT_THROW(DynSfixed({3, Direction::DOWNTO, 0}, 8), std::out_of_range);
    EXPECT_THROW(
        DynUfixed(fractional, std::numeric_limits<double>::quiet_NaN()), std::domain_error
    );
    EXPECT_DOUBLE_EQ(
        static_cast<double>(DynSfixed(fractional, std::numeric_limits<double>::infinity())),
        7.9375
    );
}

TEST(DynFixed, StaticConversionResizeAndReinterpretation) {
    using IntegerUfixed = Ufixed<3, 0>;
    using IntegerSfixed = Sfixed<3, 0>;
    using FractionalUfixed = Ufixed<3, -2>;
    using FractionalSfixed = Sfixed<3, -2>;

    static_assert(std::constructible_from<FractionalUfixed, DynUfixed const&>);
    static_assert(std::constructible_from<FractionalUfixed, DynSfixed const&>);
    static_assert(std::constructible_from<FractionalSfixed, DynUfixed const&>);
    static_assert(std::constructible_from<FractionalSfixed, DynSfixed const&>);
    static_assert(!std::convertible_to<DynUfixed const&, FractionalUfixed>);
    static_assert(!std::convertible_to<DynSfixed const&, FractionalSfixed>);
    static_assert(
        std::
            same_as<decltype(resize<3, 0>(std::declval<DynUfixed const&>())), IntegerUfixed>
    );
    static_assert(
        std::
            same_as<decltype(resize<3, 0>(std::declval<DynSfixed const&>())), IntegerSfixed>
    );

    Range const fractional{3, Direction::DOWNTO, -2};
    DynUfixed dynamic_unsigned(fractional, 5.25);
    DynSfixed dynamic_positive(fractional, 5.25);
    DynSfixed dynamic_negative(fractional, -2.5);
    EXPECT_DOUBLE_EQ(static_cast<double>(FractionalUfixed(dynamic_unsigned)), 5.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(FractionalUfixed(dynamic_positive)), 5.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(FractionalSfixed(dynamic_unsigned)), 5.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(FractionalSfixed(dynamic_negative)), -2.5);

    EXPECT_THROW(static_cast<void>(IntegerUfixed(dynamic_unsigned)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(IntegerSfixed(dynamic_negative)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(IntegerUfixed(dynamic_negative)), std::out_of_range);
    EXPECT_THROW(
        IntegerSfixed(DynUfixed({3, Direction::DOWNTO, 0}, 15)), std::out_of_range
    );

    DynUfixed unsigned_half({7, Direction::DOWNTO, -1}, 9.5);
    EXPECT_EQ(
        static_cast<int>(
            resize<3, 0>(unsigned_half, overflow_mode::saturate, round_mode::truncate)
        ),
        9
    );
    EXPECT_EQ(
        static_cast<int>(
            resize<3, 0>(unsigned_half, overflow_mode::saturate, round_mode::round_to_even)
        ),
        10
    );

    DynSfixed signed_half({7, Direction::DOWNTO, -1}, -9.5);
    EXPECT_EQ(
        static_cast<int>(
            resize<4, 0>(signed_half, overflow_mode::saturate, round_mode::truncate)
        ),
        -10
    );
    EXPECT_EQ(
        static_cast<int>(
            resize<4, 0>(signed_half, overflow_mode::saturate, round_mode::round_to_zero)
        ),
        -9
    );

    DynUfixed too_large({7, Direction::DOWNTO, 0}, 200);
    EXPECT_EQ(static_cast<int>(resize<3, 0>(too_large)), 15);
    EXPECT_EQ(
        static_cast<int>(
            resize<3, 0>(too_large, overflow_mode::wrap, round_mode::round_to_even)
        ),
        8
    );

    static_assert(ExplicitAsAvailable<IntegerUfixed, DynSfixed>);
    static_assert(ExplicitAsAvailable<IntegerSfixed, DynUfixed>);
    static_assert(!ExplicitAsAvailable<IntegerUfixed, DynSfixed&>);
    static_assert(!ExplicitAsAvailable<IntegerSfixed, DynUfixed const>);

    EXPECT_EQ(
        static_cast<int>(as<IntegerUfixed>(
            as<DynSfixed>(BitVector("1111", Range{3, Direction::DOWNTO, 0}))
        )),
        15
    );
    EXPECT_EQ(
        static_cast<int>(as<IntegerSfixed>(
            as<DynUfixed>(BitVector("1111", Range{3, Direction::DOWNTO, 0}))
        )),
        -1
    );
    EXPECT_THROW(
        static_cast<void>(as<IntegerUfixed>(DynUfixed({4, Direction::DOWNTO, 0}, 1))),
        std::invalid_argument
    );
}

TEST(DynFixed, DivisionFamilyAndRounding) {
    DynUfixed two({1, Direction::DOWNTO, 0}, 2);
    DynUfixed three({1, Direction::DOWNTO, 0}, 3);

    EXPECT_DOUBLE_EQ(static_cast<double>(two / three), 0.75);
    EXPECT_DOUBLE_EQ(
        static_cast<double>(divide(two, three, round_mode::round_to_even, 1)), 0.5
    );
    EXPECT_DOUBLE_EQ(
        static_cast<double>(divide(two, three, round_mode::round_to_zero, 3)), 0.5
    );

    DynUfixed five({2, Direction::DOWNTO, 0}, 5);
    auto [unsigned_quotient, unsigned_remainder] = divrem(five, three);
    EXPECT_EQ(unsigned_quotient, divide(five, three));
    EXPECT_EQ(static_cast<unsigned>(unsigned_remainder), 2U);
    EXPECT_EQ(rem(five, three), unsigned_remainder);
    EXPECT_EQ(mod(five, three), unsigned_remainder);

    DynSfixed negative_two({2, Direction::DOWNTO, 0}, -2);
    DynSfixed signed_three({2, Direction::DOWNTO, 0}, 3);
    EXPECT_DOUBLE_EQ(static_cast<double>(negative_two / signed_three), -0.75);
    EXPECT_DOUBLE_EQ(
        static_cast<double>(
            divide(negative_two, signed_three, round_mode::round_to_zero, 3)
        ),
        -0.5
    );

    DynSfixed negative_five({3, Direction::DOWNTO, 0}, -5);
    EXPECT_EQ(static_cast<int>(remainder(negative_five, signed_three)), -2);
    EXPECT_EQ(static_cast<int>(modulo(negative_five, signed_three)), 1);

    EXPECT_DOUBLE_EQ(static_cast<double>(reciprocal(three)), 0.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(reciprocal(signed_three)), 0.25);
    EXPECT_THROW(
        static_cast<void>(divide(two, DynUfixed({1, Direction::DOWNTO, 0}, 0))),
        std::domain_error
    );
}

TEST(DynFixed, MixedAndNativeArithmetic) {
    Range const range{7, Direction::DOWNTO, -4};
    DynUfixed u(range, 5.25);
    DynSfixed s(range, -2.5);

    EXPECT_DOUBLE_EQ(static_cast<double>(u + s), 2.75);
    EXPECT_DOUBLE_EQ(static_cast<double>(s - u), -7.75);
    EXPECT_DOUBLE_EQ(static_cast<double>(u * s), -13.125);

    DynUnsigned unsigned_integer(8, 2);
    DynSigned signed_integer(8, -2);
    EXPECT_DOUBLE_EQ(static_cast<double>(u + unsigned_integer), 7.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(u + signed_integer), 3.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(signed_integer + u), 3.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(s + 2), -0.5);
    EXPECT_DOUBLE_EQ(static_cast<double>(2 + s), -0.5);

    DynUfixed compound_u(range, 5.25);
    compound_u += unsigned_integer;
    EXPECT_DOUBLE_EQ(static_cast<double>(compound_u), 7.25);
    EXPECT_THROW(compound_u += DynSigned(8, -20), std::out_of_range);

    DynSfixed compound_s(range, -5.25);
    compound_s += unsigned_integer;
    compound_s *= 2;
    EXPECT_DOUBLE_EQ(static_cast<double>(compound_s), -6.5);

    DynUfixed incremented(range, 5.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(++incremented), 6.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(incremented--), 6.25);
    EXPECT_DOUBLE_EQ(static_cast<double>(incremented), 5.25);
}

TEST(DynFixed, StaticFixedArithmeticInterop) {
    using StaticUfixed = Ufixed<3, 0>;
    using StaticSfixed = Sfixed<3, 0>;

    static_assert(DynamicUnsignedArithmetic<DynUfixed, StaticUfixed>);
    static_assert(DynamicUnsignedArithmetic<StaticUfixed, DynUfixed>);
    static_assert(DynamicSignedArithmetic<DynSfixed, StaticSfixed>);
    static_assert(DynamicSignedArithmetic<StaticSfixed, DynSfixed>);
    static_assert(DynamicSignedArithmetic<DynUfixed, StaticSfixed>);
    static_assert(DynamicSignedArithmetic<StaticSfixed, DynUfixed>);
    static_assert(DynamicSignedArithmetic<DynSfixed, StaticUfixed>);
    static_assert(DynamicSignedArithmetic<StaticUfixed, DynSfixed>);

    Range const range{7, Direction::DOWNTO, 0};
    DynUfixed dynamic_unsigned(range, 6);
    StaticUfixed static_unsigned(2);
    EXPECT_EQ(static_cast<int>(dynamic_unsigned + static_unsigned), 8);
    EXPECT_EQ(static_cast<int>(static_unsigned - dynamic_unsigned), -4);
    EXPECT_EQ(static_cast<int>(dynamic_unsigned * static_unsigned), 12);
    EXPECT_EQ(static_cast<int>(dynamic_unsigned / static_unsigned), 3);
    EXPECT_EQ(static_cast<int>(dynamic_unsigned % StaticUfixed(4)), 2);

    DynSfixed dynamic_signed(range, -6);
    StaticSfixed static_signed(2);
    EXPECT_EQ(static_cast<int>(dynamic_signed + static_signed), -4);
    EXPECT_EQ(static_cast<int>(static_signed - dynamic_signed), 8);
    EXPECT_EQ(static_cast<int>(dynamic_signed * static_signed), -12);
    EXPECT_EQ(static_cast<int>(dynamic_signed / static_signed), -3);
    EXPECT_EQ(static_cast<int>(dynamic_signed % StaticSfixed(4)), -2);

    EXPECT_EQ(static_cast<int>(dynamic_unsigned + StaticSfixed(-2)), 4);
    EXPECT_EQ(static_cast<int>(StaticSfixed(-2) - dynamic_unsigned), -8);
    EXPECT_EQ(static_cast<int>(dynamic_signed + static_unsigned), -4);
    EXPECT_EQ(static_cast<int>(static_unsigned - dynamic_signed), 8);

    DynUfixed compound_unsigned(range, 8);
    compound_unsigned += static_unsigned;
    compound_unsigned -= static_unsigned;
    compound_unsigned *= static_unsigned;
    compound_unsigned /= static_unsigned;
    compound_unsigned %= StaticUfixed(3);
    compound_unsigned += StaticSfixed(-1);
    EXPECT_EQ(static_cast<int>(compound_unsigned), 1);

    DynSfixed compound_signed(range, -8);
    compound_signed += static_unsigned;
    compound_signed -= StaticSfixed(-2);
    compound_signed *= static_unsigned;
    compound_signed /= StaticSfixed(-2);
    compound_signed %= StaticUfixed(3);
    EXPECT_EQ(static_cast<int>(compound_signed), 1);

    StaticUfixed static_compound_unsigned(8);
    static_assert(
        std::same_as<decltype(static_compound_unsigned += dynamic_unsigned), StaticUfixed&>
    );
    static_compound_unsigned += dynamic_unsigned;
    EXPECT_EQ(static_cast<int>(static_compound_unsigned), 14);
    static_compound_unsigned -= DynUfixed(range, 2);
    EXPECT_EQ(static_cast<int>(static_compound_unsigned), 12);
    static_compound_unsigned *= DynUfixed(range, 2);
    EXPECT_EQ(static_cast<int>(static_compound_unsigned), 8);
    static_compound_unsigned /= DynUfixed(range, 2);
    EXPECT_EQ(static_cast<int>(static_compound_unsigned), 4);
    static_compound_unsigned %= DynUfixed(range, 3);
    EXPECT_EQ(static_cast<int>(static_compound_unsigned), 1);
    EXPECT_THROW(static_compound_unsigned += DynSfixed(range, -2), std::out_of_range);

    StaticSfixed static_compound_signed(-7);
    static_assert(
        std::same_as<decltype(static_compound_signed += dynamic_unsigned), StaticSfixed&>
    );
    static_compound_signed += dynamic_unsigned;
    EXPECT_EQ(static_cast<int>(static_compound_signed), -1);
    static_compound_signed -= DynSfixed(range, -2);
    EXPECT_EQ(static_cast<int>(static_compound_signed), 1);
    static_compound_signed *= DynUfixed(range, 2);
    EXPECT_EQ(static_cast<int>(static_compound_signed), 2);
    static_compound_signed /= DynSfixed(range, -2);
    EXPECT_EQ(static_cast<int>(static_compound_signed), -1);
    static_compound_signed %= DynUfixed(range, 3);
    EXPECT_EQ(static_cast<int>(static_compound_signed), -1);
}

TEST(DynFixed, BitSurfaceReverseFormattingAndHash) {
    Range const range{3, Direction::DOWNTO, -4};
    DynUfixed value(range, 5.0625);
    static_assert(std::ranges::random_access_range<DynUfixed>);
    static_assert(coconext::types::is_fixed<DynUfixed>);

    EXPECT_EQ(std::ranges::distance(value), 8);
    EXPECT_EQ(
        std::ranges::count_if(value, [](auto bit) { return static_cast<bool>(bit); }), 3
    );
    EXPECT_FALSE(static_cast<bool>(and_reduce(value)));
    EXPECT_TRUE(static_cast<bool>(or_reduce(value)));
    EXPECT_TRUE(static_cast<bool>(xor_reduce(value)));

    static_assert(ExplicitAsAvailable<BitVector, DynUfixed>);
    static_assert(ExplicitAsAvailable<BitVector, DynSfixed>);
    static_assert(!ExplicitAsAvailable<BitVector, DynUfixed&>);
    auto reinterpreted = as<BitVector>(DynUfixed(value));
    EXPECT_EQ(reinterpreted.range(), range);
    EXPECT_EQ(reinterpreted, BitVector("01010001", range));

    auto signed_reinterpreted = as<BitVector>(DynSfixed(range, -5.0625));
    EXPECT_EQ(signed_reinterpreted.range(), range);
    EXPECT_EQ(signed_reinterpreted, BitVector("10101111", range));

    auto inverted = ~value;
    EXPECT_EQ(inverted.size(), value.size());
    auto concatenated = concat(value, inverted);
    EXPECT_EQ(concatenated.size(), 16);

    auto to_value = reverse(value);
    EXPECT_EQ(to_value.range(), (Range{-4, Direction::TO, 3}));
    auto round_trip = reverse(to_value);
    EXPECT_EQ(round_trip.raw_binary(), "10001010");

    EXPECT_EQ(std::format("{}", value), "DynUfixed[3 downto -4]{5.0625}");
    EXPECT_EQ(std::format("{:b}", value), "DynUfixed[3 downto -4]{0101.0001}");
    DynSfixed signed_value(range, -5.0625);
    EXPECT_EQ(std::format("{}", signed_value), "DynSfixed[3 downto -4]{-5.0625}");
    EXPECT_EQ(std::format("{:b}", signed_value), "DynSfixed[3 downto -4]{1010.1111}");

    std::unordered_set<DynUfixed> values;
    values.insert(value);
    values.insert(DynUfixed(range, 5.0625));
    values.insert(DynUfixed(range, 6.0));
    EXPECT_EQ(values.size(), 2);
}

TEST(DynFixed, NullAndToRangesRetainBitContainerBehavior) {
    Range const null_range{-1, Direction::DOWNTO, 0};
    DynUfixed null_u(null_range);
    DynSfixed null_s(null_range);
    EXPECT_EQ(null_u.begin(), null_u.end());
    EXPECT_EQ(null_s.begin(), null_s.end());
    EXPECT_EQ(std::format("{:d}", null_u), "DynUfixed[-1 downto 0]{}");
    EXPECT_EQ(std::format("{:d}", null_s), "DynSfixed[-1 downto 0]{}");

    auto sum = null_u + DynUfixed(null_range);
    EXPECT_EQ(sum.range(), (Range{0, Direction::DOWNTO, 0}));
    EXPECT_FALSE(static_cast<bool>(sum));

    null_u += DynUfixed({3, Direction::DOWNTO, 0}, 1);
    null_s -= DynSfixed({3, Direction::DOWNTO, 0}, 1);
    EXPECT_EQ(null_u.size(), 0);
    EXPECT_EQ(null_s.size(), 0);

    BitVector bits("10010110", Range{-4, Direction::TO, 3});
    auto to_value = as<DynUfixed>(std::move(bits));
    EXPECT_EQ(std::format("{:b}", to_value), "DynUfixed[-4 to 3]{1001.0110}");
    EXPECT_THROW(static_cast<void>(static_cast<bool>(to_value)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(to_value << 1), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(static_cast<double>(to_value)), std::invalid_argument);
    EXPECT_THROW(
        static_cast<void>(std::vformat("{:d}", std::make_format_args(to_value))),
        std::format_error
    );

    EXPECT_THROW(static_cast<void>(null_u << 1), std::domain_error);
    EXPECT_THROW(static_cast<void>(null_s >> 1), std::domain_error);
}

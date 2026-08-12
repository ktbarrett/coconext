// LCOV_EXCL_BR_START
#include <gtest/gtest.h>

#include <coconext/types/dyn_int_base.hpp>
#include <stdexcept>
#include <unordered_set>

using coconext::types::detail::DynBits;
namespace detail = coconext::types::detail;

using DSW = detail::dyn_same_width;

// The inline buffer is the one tunable. Everything below is written against
// sbo_words / sbo_bits rather than literals, so raising the knob only means
// re-running these, not editing them.
TEST(DynBits, layout) {
    static_assert(sizeof(DynBits) == (DynBits::sbo_words + 1) * sizeof(uint64_t));
    static_assert(alignof(DynBits) == alignof(uint64_t));
    static_assert(DynBits::sbo_bits == DynBits::sbo_words * 64);
    SUCCEED();
}

TEST(DynBits, default_ctor_is_zero_of_width) {
    DynBits a(200);
    EXPECT_EQ(a.width(), 200u);
    EXPECT_EQ(a.popcount(), 0u);
    EXPECT_EQ(a.count_leading_zeros(), 200u);
    EXPECT_EQ(a.count_trailing_zeros(), 200u);
}

TEST(DynBits, native_int_ctor_extends_by_signedness) {
    DynBits u(200, uint64_t{0xDEADBEEF});
    EXPECT_EQ(u.to_decimal_string(), "3735928559");

    DynBits s(200, int64_t{-1});
    EXPECT_EQ(s.to_decimal_string(true), "-1");
    EXPECT_EQ(s.popcount(), 200u);
}

TEST(DynBits, string_ctor) {
    DynBits hex(200, "0x1234567890ABCDEF1122334455667788AABBCCDD");
    DynBits dec(200, "103929005307927756724023193881144724129310297309");
    EXPECT_EQ(hex, dec);

    EXPECT_EQ(DynBits(200, "-1").to_decimal_string(true), "-1");

    // Overflow throws rather than truncating, at both tiers.
    EXPECT_THROW(DynBits(200, "0x" + std::string(51, 'F')), std::out_of_range);
    EXPECT_NO_THROW(DynBits(200, "0x" + std::string(50, 'F')));
    EXPECT_THROW(DynBits(8, "999"), std::out_of_range);
    EXPECT_THROW(DynBits(64, "12x3"), std::invalid_argument);
}

// Values at or below sbo_bits live inline; wider ones are on the heap. Both
// arms must behave identically, so every value test below is run at a width on
// each side of the boundary.
TEST(DynBits, both_storage_arms_agree) {
    for (size_t w : {DynBits::sbo_bits, DynBits::sbo_bits + 1}) {
        DynBits a(w, uint64_t{12345});
        DynBits b(w, uint64_t{678});

        EXPECT_EQ(DSW::add(a, b).to_decimal_string(), "13023") << "width " << w;
        EXPECT_EQ(DSW::sub(a, b).to_decimal_string(), "11667") << "width " << w;
        EXPECT_EQ(DSW::mul(a, b).to_decimal_string(), "8369910") << "width " << w;
        EXPECT_EQ(DSW::udiv(a, b).to_decimal_string(), "18") << "width " << w;
        EXPECT_EQ(DSW::umod(a, b).to_decimal_string(), "141") << "width " << w;
        EXPECT_EQ(a.popcount(), 6u) << "width " << w;
        EXPECT_TRUE(b.ult(a)) << "width " << w;
        EXPECT_EQ((a & b).to_decimal_string(), "32") << "width " << w;
        EXPECT_EQ((a << 3).to_decimal_string(), "98760") << "width " << w;
        EXPECT_EQ(a.srl(3).to_decimal_string(), "1543") << "width " << w;
    }
}

TEST(DynBits, copy_and_move_on_both_arms) {
    for (size_t w : {DynBits::sbo_bits, DynBits::sbo_bits + 1}) {
        DynBits a(w, uint64_t{5});

        // Copy is independent.
        DynBits b = a;
        b = DSW::add(b, DynBits(w, uint64_t{1}));
        EXPECT_EQ(a.to_decimal_string(), "5") << "width " << w;
        EXPECT_EQ(b.to_decimal_string(), "6") << "width " << w;

        // Copy assignment across differing widths reshapes the destination.
        DynBits c(8, uint64_t{99});
        c = a;
        EXPECT_EQ(c.width(), w);
        EXPECT_EQ(c.to_decimal_string(), "5");

        // Move leaves the source destructible.
        DynBits d = std::move(b);
        EXPECT_EQ(d.to_decimal_string(), "6") << "width " << w;

        DynBits e(8, uint64_t{7});
        e = std::move(d);
        EXPECT_EQ(e.width(), w);
        EXPECT_EQ(e.to_decimal_string(), "6") << "width " << w;

        // Self-assignment is safe.
        e = e;
        EXPECT_EQ(e.to_decimal_string(), "6") << "width " << w;
    }
}

TEST(DynBits, width_changing_ops) {
    DynBits neg(8, uint64_t{0xFF});
    EXPECT_EQ(neg.zero_extend(200).to_decimal_string(), "255");
    EXPECT_EQ(neg.sign_extend(200).to_decimal_string(true), "-1");

    DynBits wide(200, "0x1234567890ABCDEF");
    EXPECT_EQ(wide.truncate(32).to_hexadecimal_string(), "90abcdef");

    // Saturation clamps at each end.
    DynBits big(200, uint64_t{5000});
    EXPECT_EQ(big.saturate_unsigned(8).to_decimal_string(), "255");
    EXPECT_EQ(big.saturate_signed(8).to_decimal_string(true), "127");
    DynBits very_neg(200, int64_t{-5000});
    EXPECT_EQ(very_neg.saturate_signed(8).to_decimal_string(true), "-128");

    // Values that fit are untouched.
    EXPECT_EQ(DynBits(200, uint64_t{42}).saturate_unsigned(8).to_decimal_string(), "42");

    EXPECT_THROW(wide.zero_extend(8), std::invalid_argument);
    EXPECT_THROW(neg.truncate(200), std::invalid_argument);
}

// Growing arithmetic handles differing widths natively -- that is the whole
// point -- so mismatched operands are ordinary here, not an error.
TEST(DynBits, growing_arithmetic_accepts_mixed_widths) {
    DynBits a(8, uint64_t{200});
    DynBits b(16, uint64_t{1000});

    auto sum = detail::add_unsigned(a, b);
    EXPECT_EQ(sum.width(), 17u);
    EXPECT_EQ(sum.to_decimal_string(), "1200");

    auto prod = detail::mul_unsigned(a, b);
    EXPECT_EQ(prod.width(), 24u);
    EXPECT_EQ(prod.to_decimal_string(), "200000");

    // Unsigned subtraction borrows into the extra bit instead of wrapping.
    EXPECT_EQ(detail::sub_unsigned(a, b).to_decimal_string(true), "-800");
}

TEST(DynBits, signed_growing_arithmetic) {
    DynBits neg(8, uint64_t{200});    // -56
    DynBits pos(8, uint64_t{100});    // 100
    DynBits neg3(8, uint64_t{0xFD});  // -3
    DynBits pos56(8, uint64_t{56});

    EXPECT_EQ(detail::add_signed(neg, pos).to_decimal_string(true), "44");
    EXPECT_EQ(detail::mul_signed(neg, pos).to_decimal_string(true), "-5600");
    EXPECT_EQ(detail::negate_signed(neg).to_decimal_string(true), "56");
    EXPECT_EQ(detail::abs_signed(neg).to_decimal_string(true), "56");

    // rem follows the dividend's sign, mod the divisor's.
    EXPECT_EQ(detail::rem_signed(neg, pos).to_decimal_string(true), "-56");
    EXPECT_EQ(detail::mod_signed(neg, pos).to_decimal_string(true), "44");
    EXPECT_EQ(detail::rem_signed(pos56, neg3).to_decimal_string(true), "2");
    EXPECT_EQ(detail::mod_signed(pos56, neg3).to_decimal_string(true), "-1");

    // The extra quotient bit keeps signed_min / -1 representable.
    DynBits min_val(8, uint64_t{0x80});
    DynBits minus_one(8, uint64_t{0xFF});
    EXPECT_EQ(detail::div_signed(min_val, minus_one).to_decimal_string(true), "128");
}

TEST(DynBits, wide_arithmetic_matches_reference) {
    DynBits a(200, "0x1234567890ABCDEF1122334455667788AABBCCDD");
    DynBits b(200, "0xFEDCBA98765432100123456789");

    EXPECT_EQ(
        DSW::add(a, b).to_decimal_string(),
        "103929005307927776916288754849918835164314678374"
    );
    EXPECT_EQ(DSW::udiv(a, b).to_decimal_string(), "5146971002046463");
    EXPECT_EQ(DSW::umod(a, b).to_decimal_string(), "20112278405973339191843622874214");

    // Division identity.
    EXPECT_EQ(DSW::add(DSW::mul(DSW::udiv(a, b), b), DSW::umod(a, b)), a);

    DynBits neg(200, "-1000000000000000000000");
    DynBits seven(200, uint64_t{7});
    EXPECT_EQ(DSW::sdiv(neg, seven).to_decimal_string(true), "-142857142857142857142");
    EXPECT_EQ(DSW::smod(neg, seven).to_decimal_string(true), "-6");
}

TEST(DynBits, division_by_zero_throws) {
    DynBits a(200, uint64_t{5});
    DynBits zero(200);
    EXPECT_THROW(DSW::udiv(a, zero), std::domain_error);
    EXPECT_THROW(DSW::umod(a, zero), std::domain_error);
    EXPECT_THROW(DSW::sdiv(a, zero), std::domain_error);
    EXPECT_THROW(detail::div_unsigned(a, zero), std::domain_error);
}

TEST(DynBits, division_pairs) {
    DynBits minus_five(200, int64_t{-5});
    DynBits three(200, int64_t{3});

    auto [quotient, remainder] = DSW::sdivrem(minus_five, three);
    EXPECT_EQ(quotient.to_decimal_string(true), "-1");
    EXPECT_EQ(remainder.to_decimal_string(true), "-2");

    auto [floor_quotient, modulo] = DSW::sdivmod(minus_five, three);
    EXPECT_EQ(floor_quotient.to_decimal_string(true), "-2");
    EXPECT_EQ(modulo.to_decimal_string(true), "1");

    auto [unsigned_quotient, unsigned_remainder] =
        DSW::udivrem(DynBits(200, uint64_t{17}), DynBits(200, uint64_t{5}));
    EXPECT_EQ(unsigned_quotient.to_decimal_string(), "3");
    EXPECT_EQ(unsigned_remainder.to_decimal_string(), "2");
}

// Bits<W> makes a width mismatch a compile error; DynBits cannot. Equality is
// total so the type stays usable as a hash key, while the orderings and the
// same-width primitives throw -- comparing across widths has no answer that is
// right for both zero- and sign-extension.
TEST(DynBits, width_mismatch_policy) {
    DynBits a(200, uint64_t{1});
    DynBits b(128, uint64_t{1});

    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);

    EXPECT_THROW(a.ult(b), std::invalid_argument);
    EXPECT_THROW(a.slt(b), std::invalid_argument);
    EXPECT_THROW(DSW::add(a, b), std::invalid_argument);
    EXPECT_THROW(DSW::udiv(a, b), std::invalid_argument);
    EXPECT_THROW((void)(a & b), std::invalid_argument);

    // Growing arithmetic is exempt: it extends both operands first.
    EXPECT_NO_THROW((void)detail::add_unsigned(a, b));
}

TEST(DynBits, string_conversions) {
    DynBits a(16, uint64_t{0xABCD});
    EXPECT_EQ(a.to_binary_string(), "1010101111001101");
    EXPECT_EQ(a.to_hexadecimal_string(), "abcd");
    EXPECT_EQ(a.to_decimal_string(), "43981");

    DynBits o(9, uint64_t{0777});
    EXPECT_EQ(o.to_octal_string(), "777");

    // Null vector stringifies to empty in every base.
    DynBits n(0);
    EXPECT_EQ(n.to_binary_string(), "");
    EXPECT_EQ(n.to_decimal_string(), "");
    EXPECT_EQ(n.to_hexadecimal_string(), "");
    EXPECT_EQ(n.to_octal_string(), "");
}

TEST(DynBits, zero_width) {
    DynBits a(0);
    DynBits b(0);

    EXPECT_EQ(a.width(), 0u);
    EXPECT_TRUE(a == b);
    EXPECT_EQ(a.popcount(), 0u);
    EXPECT_TRUE((a & b) == a);
    EXPECT_TRUE(~a == a);

    // The null vector has no value, so raw() and the integer ctor are errors.
    EXPECT_THROW((void)a.raw(), std::domain_error);
    EXPECT_THROW(DynBits(0, uint64_t{1}), std::invalid_argument);
}

// DynBits and Bits<W> must agree bit for bit at matched widths, especially
// where the two tier boundaries fall.
TEST(DynBits, agrees_with_static_bits_at_tier_boundaries) {
    auto check = [](auto static_a, auto static_b, size_t w) {
        DynBits a(static_a);
        DynBits b(static_b);
        EXPECT_EQ(a.width(), w);
        EXPECT_EQ(
            DSW::add(a, b).to_decimal_string(),
            detail::same_width::add(static_a, static_b).to_decimal_string()
        );
        EXPECT_EQ(
            DSW::mul(a, b).to_decimal_string(),
            detail::same_width::mul(static_a, static_b).to_decimal_string()
        );
        EXPECT_EQ(
            DSW::udiv(a, b).to_decimal_string(),
            detail::same_width::udiv(static_a, static_b).to_decimal_string()
        );
        EXPECT_EQ(a.popcount(), static_a.popcount());
        EXPECT_EQ(a.count_leading_zeros(), static_a.count_leading_zeros());
        EXPECT_EQ(a.to_hexadecimal_string(), static_a.to_hexadecimal_string());
    };

    check(detail::Bits<63>(uint64_t{123456}), detail::Bits<63>(uint64_t{789}), 63);
    check(detail::Bits<64>(uint64_t{123456}), detail::Bits<64>(uint64_t{789}), 64);
    check(detail::Bits<65>(uint64_t{123456}), detail::Bits<65>(uint64_t{789}), 65);
    check(detail::Bits<127>(uint64_t{123456}), detail::Bits<127>(uint64_t{789}), 127);
    check(detail::Bits<128>(uint64_t{123456}), detail::Bits<128>(uint64_t{789}), 128);
    check(detail::Bits<129>(uint64_t{123456}), detail::Bits<129>(uint64_t{789}), 129);
}

// LCOV_EXCL_BR_STOP

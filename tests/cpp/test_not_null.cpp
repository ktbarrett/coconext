// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/not_null.hpp>

#include <functional>
#include <type_traits>
#include <unordered_map>

using coconext::not_null;

namespace {

struct Base {
    int x = 1;
};
struct Derived : Base {
    int y = 2;
};

}  // namespace

TEST(TestNotNull, ConstructFromRawPointerAndAccess) {
    int i = 5;
    not_null p{&i};
    EXPECT_EQ(p.get(), &i);
    EXPECT_EQ(*p, 5);
    *p = 7;
    EXPECT_EQ(i, 7);
}

TEST(TestNotNull, ArrowOperator) {
    Derived d;
    not_null p{&d};
    EXPECT_EQ(p->x, 1);
    EXPECT_EQ(p->y, 2);
}

TEST(TestNotNull, ImplicitConversionToRawPointer) {
    int i = 0;
    not_null nn{&i};
    // Flows into an API that takes a raw int*.
    auto takes_raw = [](int* p) { return p; };
    EXPECT_EQ(takes_raw(nn), &i);
    // Direct comparison of converted value.
    int* raw = nn;
    EXPECT_EQ(raw, &i);
}

TEST(TestNotNull, CopyAndAssignment) {
    int i = 0;
    int j = 0;
    not_null a{&i};
    not_null b = a;
    EXPECT_EQ(b.get(), &i);
    not_null c{&j};
    c = a;
    EXPECT_EQ(c.get(), &i);
}

TEST(TestNotNull, ConvertingConstructorDerivedToBase) {
    Derived d;
    not_null<Derived*> dp{&d};
    not_null<Base*> bp = dp;
    EXPECT_EQ(bp.get(), static_cast<Base*>(&d));
    EXPECT_EQ(bp->x, 1);
}

TEST(TestNotNull, HomogeneousComparisons) {
    int i = 0;
    int j = 0;
    not_null a{&i};
    not_null b{&i};
    not_null c{&j};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE((a <=> b) == 0);
    EXPECT_TRUE((a <=> c) != 0);
}

TEST(TestNotNull, HeterogeneousComparisonWithRawPointer) {
    int i = 0;
    int j = 0;
    not_null a{&i};
    EXPECT_TRUE(a == &i);
    EXPECT_TRUE(&i == a);  // rewritten operator via C++20
    EXPECT_FALSE(a == &j);
    EXPECT_TRUE((a <=> &i) == 0);
    EXPECT_TRUE((a <=> &j) != 0);
}

TEST(TestNotNull, HashMatchesUnderlyingPointer) {
    int i = 0;
    not_null a{&i};
    EXPECT_EQ(std::hash<not_null<int*>>{}(a), std::hash<int*>{}(&i));

    // Usable as a key in an unordered_map.
    std::unordered_map<not_null<int*>, int> m;
    m.emplace(a, 42);
    EXPECT_EQ(m.at(a), 42);
    EXPECT_EQ(m.at(not_null{&i}), 42);
}

TEST(TestNotNull, DeductionGuideDeducesPointerType) {
    int i = 0;
    not_null nn{&i};
    static_assert(std::is_same_v<decltype(nn), not_null<int*>>);
    Derived d;
    not_null nnd{&d};
    static_assert(std::is_same_v<decltype(nnd), not_null<Derived*>>);
}

TEST(TestNotNull, NullptrConstructionAndComparisonBanned) {
    // Deleted overloads / requires-clauses must forbid these at compile time.
    static_assert(!std::is_constructible_v<not_null<int*>, std::nullptr_t>);
    static_assert(!std::is_default_constructible_v<not_null<int*>>);
    static_assert(!std::is_assignable_v<not_null<int*>, std::nullptr_t>);
    // Comparisons against nullptr are deleted; testing via a trait would
    // require a bespoke concept, so we settle for the constructability check.
}

TEST(TestNotNull, ConstexprConstructionAndAccess) {
    static int i = 3;
    constexpr auto make = []() {
        not_null p{&i};
        return p.get();
    };
    static_assert(make() == &i);
}

// LCOV_EXCL_BR_STOP

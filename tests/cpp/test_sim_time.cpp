// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <chrono>
#include <cstdint>
#include <format>
#include <limits>
#include <ratio>
#include <type_traits>

#include <gtest/gtest.h>

#include <coconext/types.hpp>

using namespace coconext::types;
using namespace coconext::literals;

// 9'223'372'036'854'775'808_fs won't compile bc consteval range check

// femto pico checks
static_assert(std::is_same_v<SimTime::rep, std::int64_t>);
static_assert(std::is_same_v<SimTime::period, std::femto>);
static_assert(std::is_same_v<SimTime, femtoseconds>);
static_assert(std::is_same_v<picoseconds::rep, std::int64_t>);
static_assert(std::is_same_v<picoseconds::period, std::pico>);

// std duration re-exports same type
static_assert(std::is_same_v<nanoseconds, std::chrono::nanoseconds>);
static_assert(std::is_same_v<microseconds, std::chrono::microseconds>);
static_assert(std::is_same_v<milliseconds, std::chrono::milliseconds>);
static_assert(std::is_same_v<seconds, std::chrono::seconds>);

// literal return types
static_assert(std::is_same_v<decltype(10_fs), SimTime>);
static_assert(
    std::is_same_v<decltype(1.5_fs), std::chrono::duration<long double, std::femto>>
);
static_assert(std::is_same_v<decltype(10_ps), picoseconds>);
static_assert(
    std::is_same_v<decltype(7.5_ps), std::chrono::duration<long double, std::pico>>
);

// conversions
static_assert(std::is_convertible_v<picoseconds, SimTime>);
static_assert(!std::is_convertible_v<SimTime, picoseconds>);

// using round instead of duration_cast due to floating point imprecision
static_assert(std::chrono::round<SimTime>(0.001_ps) == 1_fs);
static_assert(std::chrono::round<SimTime>(0.01_ps) == 10_fs);
static_assert(std::chrono::round<SimTime>(0.1_ps) == 100_fs);
static_assert(std::chrono::round<SimTime>(2.5_ps) == 2500_fs);

static_assert(!std::is_convertible_v<decltype(1.5_ps), SimTime>);

TEST(TestSimTime, Arithmetic) {
    static_assert(2_ps + 500_fs == 2500_fs);
    static_assert(std::is_same_v<decltype(5_ps + 250_fs), SimTime>);
    static_assert(1ns - 1_ps == 999'000_fs);
    static_assert(2 * 5_ps == 10_ps);
    static_assert(6_fs / 3_fs == 2);
    static_assert(12_ps % 5_ps == 2_ps);

    SimTime st{1ns};
    EXPECT_EQ(st.count(), 1'000'000);
}

TEST(TestSimTime, Comparisons) {
    static_assert(1_ps == 1000_fs);
    static_assert(1ns > 999_ps);
    static_assert(1_fs < 1_ps);

    SimTime st{};
    EXPECT_EQ(st, 0_fs);

    static_assert(SimTime::max().count() == std::numeric_limits<std::int64_t>::max());
}

TEST(TestSimTime, Formatter) {
    EXPECT_EQ(std::format("{}", 2_fs), "2fs");
    EXPECT_EQ(std::format("{}", 5_ps), "5ps");
    EXPECT_EQ(std::format("{}", SimTime{3ns}), "3000000fs");
    EXPECT_EQ(std::format("{:%Q}", 4_fs), "4");
    EXPECT_EQ(std::format("{:%q}", 1_ps), "ps");
}
// LCOV_EXCL_BR_STOP

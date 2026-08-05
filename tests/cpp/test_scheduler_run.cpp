// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>

#include <stdexcept>
#include <string>
#include <utility>

using coconext::Coro;
using coconext::run;
using coconext::Task;

namespace {

Coro<int> coro_return_int() { co_return 42; }

Coro<void> coro_return_void() { co_return; }

Coro<std::string> coro_return_string() { co_return std::string("hello"); }

Coro<int> coro_awaits_coro() {
    int v = co_await coro_return_int();
    co_return v + 1;
}

Coro<int> coro_awaits_two() {
    int a = co_await coro_return_int();
    int b = co_await coro_return_int();
    co_return a + b;
}

Coro<int> coro_deep(int depth) {
    if (depth == 0) {
        co_return 0;
    }
    int inner = co_await coro_deep(depth - 1);
    co_return inner + 1;
}

Coro<int> coro_throws() {
    throw std::runtime_error("boom");
    co_return 0;  // unreachable
}

Coro<int> coro_awaits_throwing() {
    int v = co_await coro_throws();
    co_return v;
}

Coro<int> coro_catches_throwing() {
    try {
        (void)co_await coro_throws();
    } catch (std::runtime_error const&) {
        co_return 7;
    }
    co_return -1;
}

Coro<int> coro_awaits_task_int() {
    Task<int> t = coro_return_int();
    int v = co_await t;
    co_return v * 2;
}

Coro<void> coro_awaits_task_void() {
    Task<void> t = coro_return_void();
    co_await t;
    co_return;
}

Coro<int> coro_awaits_task_twice() {
    // A Task is a shared handle -- awaiting the same finished Task twice must
    // return the same result without re-executing the body.
    Task<int> t = coro_return_int();
    int a = co_await t;
    int b = co_await t;
    co_return a + b;
}

}  // namespace

// -- run(Coro<T>) minimal cases -------------------------------------------

TEST(TestRun, CoroReturnsInt) { EXPECT_EQ(run(coro_return_int()), 42); }

TEST(TestRun, CoroReturnsVoid) { EXPECT_NO_THROW(run(coro_return_void())); }

TEST(TestRun, CoroReturnsString) {
    EXPECT_EQ(run(coro_return_string()), std::string("hello"));
}

// -- run(Task<T>) via Coro->Task conversion --------------------------------

TEST(TestRun, TaskFromCoro) {
    Task<int> t = coro_return_int();
    EXPECT_FALSE(t.done());
    EXPECT_TRUE(t.unstarted());
    EXPECT_EQ(run(std::move(t)), 42);
}

TEST(TestRun, TaskVoidFromCoro) {
    Task<void> t = coro_return_void();
    EXPECT_NO_THROW(run(std::move(t)));
}

// -- Coro awaiting Coro (symmetric transfer chain) -------------------------

TEST(TestRun, CoroAwaitsCoro) { EXPECT_EQ(run(coro_awaits_coro()), 43); }

TEST(TestRun, CoroAwaitsTwoSequentially) { EXPECT_EQ(run(coro_awaits_two()), 84); }

TEST(TestRun, CoroDeepChainIsBounded) {
    // A deep Coro->Coro chain must not blow the stack; symmetric transfer
    // keeps it O(1) at the tail-call level. 500 is comfortably below any
    // reasonable recursion limit but well above what a non-tail-called
    // implementation could handle.
    EXPECT_EQ(run(coro_deep(500)), 500);
}

// -- Exception propagation through Coros -----------------------------------

TEST(TestRun, CoroThrowsPropagatesFromRun) {
    EXPECT_THROW(run(coro_throws()), std::runtime_error);
}

TEST(TestRun, CoroAwaitingThrowingCoroPropagates) {
    EXPECT_THROW(run(coro_awaits_throwing()), std::runtime_error);
}

TEST(TestRun, CoroCanCatchThrownFromChild) { EXPECT_EQ(run(coro_catches_throwing()), 7); }

// -- Coro awaiting Task ----------------------------------------------------

TEST(TestRun, CoroAwaitsUnstartedTask) {
    // Awaiting an unstarted Task should implicitly kick it off.
    EXPECT_EQ(run(coro_awaits_task_int()), 84);
}

TEST(TestRun, CoroAwaitsVoidTask) { EXPECT_NO_THROW(run(coro_awaits_task_void())); }

TEST(TestRun, TaskAwaitedTwiceReturnsSameResult) {
    EXPECT_EQ(run(coro_awaits_task_twice()), 84);
}

// LCOV_EXCL_BR_STOP

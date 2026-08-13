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
using coconext::get_context;
using coconext::run;
using coconext::Task;
using coconext::TaskContext;

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

Coro<int> coro_awaits_started_task_int() {
    Task<int> t = coconext::start_soon(coro_return_int());
    int v = co_await t;
    co_return v * 2;
}

Coro<void> coro_awaits_started_task_void() {
    Task<void> t = coconext::start_soon(coro_return_void());
    co_await t;
    co_return;
}

Coro<int> coro_awaits_task_twice() {
    // A Task is a shared handle -- awaiting the same finished Task twice must
    // return the same result without re-executing the body.
    Task<int> t = coconext::start_soon(coro_return_int());
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

TEST(TestRun, CoroAwaitsStartedTask) { EXPECT_EQ(run(coro_awaits_started_task_int()), 84); }

TEST(TestRun, CoroAwaitsVoidTask) { EXPECT_NO_THROW(run(coro_awaits_started_task_void())); }

TEST(TestRun, TaskAwaitedTwiceReturnsSameResult) {
    EXPECT_EQ(run(coro_awaits_task_twice()), 84);
}

// -- get_context() ---------------------------------------------------------

namespace {

Coro<TaskContext> coro_captures_context_deep(int depth) {
    if (depth == 0) {
        co_return co_await get_context();
    }
    co_return co_await coro_captures_context_deep(depth - 1);
}

}  // namespace

TEST(TestRun, GetContextReturnsEnclosingTaskBindings) {
    // A Coro several levels deep asks for its enclosing context. The
    // returned TaskContext must report the same Task/loop/global manager
    // the outer Task itself sees -- proving get_context walks the promise
    // chain rather than reading TLS after the fact.
    auto body = []() -> Coro<void> {
        TaskContext outer = co_await get_context();
        TaskContext inner = co_await coro_captures_context_deep(3);
        EXPECT_EQ(outer.get_task().get(), inner.get_task().get());
        EXPECT_EQ(outer.get_event_loop().get(), inner.get_event_loop().get());
        EXPECT_EQ(
            outer.get_global_task_manager().get(), inner.get_global_task_manager().get()
        );
        co_return;
    };
    EXPECT_NO_THROW(run(body()));
}

TEST(TestRun, UnawaitedTaskContextThrows) {
    // A default-constructed TaskContext (as returned by get_context() before
    // it is co_awaited) has no bound task; every accessor must throw.
    TaskContext ctxt = get_context();
    EXPECT_THROW((void)ctxt.get_task(), std::runtime_error);
    EXPECT_THROW((void)ctxt.get_event_loop(), std::runtime_error);
    EXPECT_THROW((void)ctxt.get_global_task_manager(), std::runtime_error);
}

// LCOV_EXCL_BR_STOP

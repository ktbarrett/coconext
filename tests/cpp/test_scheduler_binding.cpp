// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/future.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

using coconext::Cancelled;
using coconext::Coro;
using coconext::current_context;
using coconext::current_task;
using coconext::Future;
using coconext::run;
using coconext::start_soon;
using coconext::Task;
using coconext::TaskManager;

namespace {

Coro<int> return_int(int value = 42) { co_return value; }

Coro<void> throw_runtime() {
    throw std::runtime_error("boom");
    co_return;
}

Task<int> make_unstarted_task() { co_return 42; }

Coro<void> self_cancel() {
    current_task()->cancel();
    co_return;
}

Coro<void> wait_for(Future<void> future) { co_await future; }

Coro<void> cancel_task(Task<void> task) {
    task.cancel();
    co_return;
}

}  // namespace

static_assert(!std::is_constructible_v<Task<int>, Coro<int>>);

TEST(TestSchedulerBinding, CurrentTaskAndContextThrowOutsideTask) {
    EXPECT_THROW((void)current_task(), std::runtime_error);
    EXPECT_THROW((void)current_context(), std::runtime_error);
}

TEST(TestSchedulerBinding, CurrentTaskInsideTaskReturnsSelf) {
    auto body = []() -> Coro<void> {
        auto context = co_await coconext::get_context();
        EXPECT_EQ(current_task().get(), context.get_task().get());
    };

    EXPECT_NO_THROW(run(body()));
    EXPECT_THROW((void)current_task(), std::runtime_error);
}

TEST(TestSchedulerBinding, AwaitingUnstartedTaskIsRejected) {
    auto body = []() -> Coro<void> {
        Task<int> task = make_unstarted_task();
        bool rejected = false;
        try {
            (void)(co_await task);
        } catch (std::runtime_error const&) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, FreeStartSoonSpawnsAndReturnsTask) {
    auto body = []() -> Coro<int> {
        Task<int> task = start_soon(return_int());
        EXPECT_TRUE(task.started());
        co_return co_await task;
    };

    EXPECT_EQ(run(body()), 42);
}

TEST(TestSchedulerBinding, FreeStartSoonUsesFailFastGlobalManager) {
    auto body = []() -> Coro<void> {
        (void)start_soon(throw_runtime());
        Future<void> never;
        co_await never;
    };

    EXPECT_THROW(run(body()), std::runtime_error);
}

TEST(TestSchedulerBinding, SelfCancellationPropagatesFromRun) {
    EXPECT_THROW(run(self_cancel()), Cancelled);
}

TEST(TestSchedulerBinding, PendingTaskCancellationWakesAwaiter) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> never;
        co_await manager.start();
        Task<void> waiter = manager.start_soon(wait_for(never));
        (void)manager.start_soon(cancel_task(waiter));
        co_await manager.join();
        EXPECT_THROW(waiter.result(), Cancelled);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, UncancelRejectsTaskThatWasNotCancelled) {
    auto body = []() -> Coro<void> {
        Task<int> task = start_soon(return_int());
        EXPECT_THROW(task.uncancel(), std::runtime_error);
        (void)(co_await task);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, TaskDoneCallbackFiresExactlyOnce) {
    int calls = 0;
    auto body = [&calls]() -> Coro<void> {
        Task<int> task = start_soon(return_int());
        task.add_done_callback([&calls]() { ++calls; });
        (void)(co_await task);
    };

    EXPECT_NO_THROW(run(body()));
    EXPECT_EQ(calls, 1);
}

TEST(TestSchedulerBinding, FinishedTaskCanBeAwaitedInAnotherRun) {
    std::optional<Task<int>> finished;
    auto first = [&finished]() -> Coro<void> {
        finished.emplace(start_soon(return_int()));
        (void)(co_await *finished);
    };
    run(first());

    auto second = [&finished]() -> Coro<int> { co_return co_await *finished; };
    EXPECT_EQ(run(second()), 42);
}

TEST(TestSchedulerBinding, CoroMoveAssignmentTransfersOwnership) {
    Coro<int> first = return_int(1);
    Coro<int> second = return_int(42);
    auto* new_state = &second.get_state();
    first = std::move(second);
    EXPECT_EQ(&first.get_state(), new_state);
    EXPECT_EQ(run(std::move(first)), 42);
}

// LCOV_EXCL_BR_STOP

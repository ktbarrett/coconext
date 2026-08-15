// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/future.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

using coconext::CancelledError;
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

Coro<void> signal_then_wait(Future<void> started, Future<void> future) {
    started.set_void();
    co_await future;
}

Coro<void> swallow_cancellation(Future<void> started, Future<void> future) {
    started.set_void();
    try {
        co_await future;
    } catch (CancelledError const&) {}
}

Coro<void> replace_cancellation(Future<void> started, Future<void> future) {
    started.set_void();
    try {
        co_await future;
    } catch (CancelledError const&) {
        throw std::logic_error("replacement");
    }
}

Coro<void> continue_after_cancellation(
    Future<void> started, Future<void> first, Future<void> second
) {
    started.set_void();
    try {
        co_await first;
    } catch (CancelledError const&) {}
    co_await second;
}

Coro<void> throw_cancelled() {
    throw CancelledError{};
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
        auto task = current_task();
        EXPECT_EQ(current_task().get(), task.get());
        co_return;
    };

    EXPECT_NO_THROW(run(body()));
    EXPECT_THROW((void)current_task(), std::runtime_error);
}

TEST(TestSchedulerBinding, AwaitingUnstartedTaskStartsIt) {
    auto body = []() -> Coro<int> {
        Task<int> task = make_unstarted_task();
        EXPECT_FALSE(task.started());
        auto result = co_await task;
        EXPECT_TRUE(task.started());
        co_return result;
    };

    EXPECT_EQ(run(body()), 42);
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
    EXPECT_THROW(run(self_cancel()), CancelledError);
}

TEST(TestSchedulerBinding, SelfCancellationRecordsRequestedAndTerminalCancellation) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        co_await manager.start();
        Task<void> task = manager.start_soon(self_cancel());
        co_await manager.join();

        EXPECT_TRUE(task.done());
        EXPECT_TRUE(task.cancelling());
        EXPECT_TRUE(task.cancelled());
        EXPECT_THROW(task.result(), CancelledError);
        EXPECT_THROW(std::rethrow_exception(task.exception()), CancelledError);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, PendingTaskCancellationWakesAwaiter) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> never;
        co_await manager.start();
        Task<void> waiter = manager.start_soon(wait_for(never));
        (void)manager.start_soon(cancel_task(waiter));
        co_await manager.join();
        EXPECT_THROW(waiter.result(), CancelledError);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, PendingCancellationIsNotYetACancelledOutcome) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> started;
        Future<void> never;
        co_await manager.start();
        Task<void> task = manager.start_soon(signal_then_wait(started, never));

        co_await started;
        task.cancel();
        EXPECT_TRUE(task.cancelling());
        EXPECT_FALSE(task.done());
        EXPECT_FALSE(task.cancelled());

        co_await manager.join();
        EXPECT_TRUE(task.done());
        EXPECT_TRUE(task.cancelled());
        EXPECT_THROW(task.result(), CancelledError);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, SuppressedCancellationFailsTask) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> started;
        Future<void> never;
        co_await manager.start();
        Task<void> task = manager.start_soon(swallow_cancellation(started, never));

        co_await started;
        task.cancel();
        co_await manager.join();

        EXPECT_TRUE(task.cancelling());
        EXPECT_FALSE(task.cancelled());
        EXPECT_THROW(task.result(), std::runtime_error);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, ReplacingCancellationWithAnotherExceptionFailsTask) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> started;
        Future<void> never;
        co_await manager.start();
        Task<void> task = manager.start_soon(replace_cancellation(started, never));

        co_await started;
        task.cancel();
        co_await manager.join();

        EXPECT_TRUE(task.cancelling());
        EXPECT_FALSE(task.cancelled());
        try {
            task.result();
            ADD_FAILURE() << "Task did not reject an ignored cancellation";
        } catch (std::runtime_error const& error) {
            EXPECT_STREQ(error.what(), "Task ignored cancellation");
        }
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, SuspendingAfterCancellationWithoutUncancelFailsTask) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> started;
        Future<void> first;
        Future<void> second;
        co_await manager.start();
        Task<void> task =
            manager.start_soon(continue_after_cancellation(started, first, second));

        co_await started;
        task.cancel();
        co_await manager.join();

        EXPECT_TRUE(task.done());
        EXPECT_FALSE(second.done());
        EXPECT_FALSE(task.cancelled());
        EXPECT_THROW(task.result(), std::runtime_error);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, UnrequestedCancelledIsAnOrdinaryException) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        co_await manager.start();
        Task<void> task = manager.start_soon(throw_cancelled());
        co_await manager.join();

        EXPECT_FALSE(task.cancelling());
        EXPECT_FALSE(task.cancelled());
        EXPECT_THROW(task.result(), CancelledError);
        EXPECT_THROW(std::rethrow_exception(task.exception()), CancelledError);
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

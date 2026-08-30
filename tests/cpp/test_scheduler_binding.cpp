// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/awaitable.hpp>
#include <coconext/coro.hpp>
#include <coconext/future.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <concepts>
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

using coconext::await_result_t;
using coconext::Awaitable;
using coconext::Awaiter;
using coconext::CancelledError;
using coconext::CoconextAwaitable;
using coconext::CoconextAwaiter;
using coconext::Coro;
using coconext::Future;
using coconext::get_awaiter;
using coconext::get_context;
using coconext::lookup_context;
using coconext::lookup_task;
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

Coro<int> await_task_reference(Task<int>* task, Future<void> started) {
    started.set_void();
    co_return co_await *task;
}

Coro<int> destroy_last_task_wrapper_while_awaited() {
    std::optional<Task<int>> task{make_unstarted_task()};
    Future<void> started;
    Task<int> awaiter = start_soon(await_task_reference(&*task, started));
    co_await started;

    task.reset();
    co_return co_await awaiter;
}

Coro<void> self_cancel() {
    lookup_task()->cancel();
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

class ImmediateIntAwaitable {
  public:
    class Awaiter {
      public:
        using coconext_awaiter = void;

        [[nodiscard]] explicit Awaiter(int value) noexcept : value_(value) {}
        [[nodiscard]] bool await_ready() const noexcept { return true; }

        template <typename PromiseType>
        void await_suspend(std::coroutine_handle<PromiseType>) const noexcept {}

        [[nodiscard]] int await_resume() const noexcept { return value_; }

      private:
        int value_;
    };

    [[nodiscard]] explicit ImmediateIntAwaitable(int value) noexcept : value_(value) {}

    [[nodiscard]] CoconextAwaitable auto operator co_await() const noexcept {
        return Awaiter{value_};
    }

  private:
    int value_;
};

class UnmarkedIntAwaitable {
  public:
    [[nodiscard]] bool await_ready() const noexcept { return true; }

    template <typename PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType>) const noexcept {}

    [[nodiscard]] int await_resume() const noexcept { return 42; }
};

}  // namespace

static_assert(!std::is_constructible_v<Task<int>, Coro<int>>);
static_assert(Awaitable<Coro<int>>);
static_assert(Awaiter<ImmediateIntAwaitable::Awaiter>);
static_assert(CoconextAwaiter<ImmediateIntAwaitable::Awaiter>);
static_assert(CoconextAwaitable<Coro<int>>);
static_assert(CoconextAwaitable<Task<int>>);
static_assert(CoconextAwaitable<Future<int>>);
static_assert(CoconextAwaitable<decltype(get_context())>);
static_assert(std::same_as<await_result_t<ImmediateIntAwaitable>, int>);
static_assert(std::same_as<
              decltype(get_awaiter(std::declval<ImmediateIntAwaitable>())),
              ImmediateIntAwaitable::Awaiter>);
static_assert(Awaiter<UnmarkedIntAwaitable>);
static_assert(Awaitable<UnmarkedIntAwaitable>);
static_assert(!CoconextAwaiter<UnmarkedIntAwaitable>);
static_assert(!CoconextAwaitable<UnmarkedIntAwaitable>);

TEST(TestSchedulerBinding, LookupTaskAndContextThrowOutsideTask) {
    EXPECT_THROW((void)lookup_task(), std::runtime_error);
    EXPECT_THROW((void)lookup_context(), std::runtime_error);
}

TEST(TestSchedulerBinding, LookupTaskInsideTaskReturnsSelf) {
    auto body = []() -> Coro<void> {
        auto task = lookup_task();
        EXPECT_EQ(lookup_task().get(), task.get());
        co_return;
    };

    EXPECT_NO_THROW(run(body()));
    EXPECT_THROW((void)lookup_task(), std::runtime_error);
}

TEST(TestSchedulerBinding, GetContextReturnsEnclosingTaskContext) {
    auto body = []() -> Coro<void> {
        auto context = co_await get_context();
        auto lookup = lookup_context();

        EXPECT_EQ(context.get_task(), lookup_task().get());
        EXPECT_EQ(context.get_task(), lookup.get_task());
        EXPECT_EQ(context.get_event_loop(), lookup.get_event_loop());
        EXPECT_EQ(context.get_global_task_manager(), lookup.get_global_task_manager());
    };

    EXPECT_NO_THROW(run(body()));
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

TEST(TestSchedulerBinding, AwaiterOwnsStateAfterLastTaskWrapperIsDestroyed) {
    EXPECT_EQ(run(destroy_last_task_wrapper_while_awaited()), 42);
}

TEST(TestSchedulerBinding, FreeStartSoonSpawnsAndReturnsTask) {
    auto body = []() -> Coro<int> {
        Task<int> task = start_soon(return_int());
        EXPECT_TRUE(task.started());
        co_return co_await task;
    };

    EXPECT_EQ(run(body()), 42);
}

TEST(TestSchedulerBinding, FreeStartSoonAcceptsAnyCoconextAwaitable) {
    auto body = []() -> Coro<int> {
        co_return co_await start_soon(ImmediateIntAwaitable{21});
    };

    EXPECT_EQ(run(body()), 21);
}

TEST(TestSchedulerBinding, FreeStartSoonAdoptsUnstartedTask) {
    auto body = []() -> Coro<int> {
        Task<int> task = make_unstarted_task();
        auto state = task.get_state();

        Task<int> adopted = start_soon(task);
        EXPECT_EQ(adopted.get_state(), state);
        EXPECT_TRUE(task.started());
        co_return co_await adopted;
    };

    EXPECT_EQ(run(body()), 42);
}

TEST(TestSchedulerBinding, FreeStartSoonAdoptsRvalueTask) {
    auto body = []() -> Coro<int> {
        Task<int> task = make_unstarted_task();
        auto state = task.get_state();

        Task<int> adopted = start_soon(std::move(task));
        EXPECT_EQ(adopted.get_state(), state);
        co_return co_await adopted;
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

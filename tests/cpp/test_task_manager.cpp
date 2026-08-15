// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/future.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <exception>
#include <stdexcept>
#include <type_traits>
#include <vector>

using coconext::CancelledError;
using coconext::Coro;
using coconext::current_task;
using coconext::Future;
using coconext::not_null;
using coconext::run;
using coconext::start_soon;
using coconext::Task;
using coconext::TaskManager;
using coconext::TaskState;

namespace {

class RecordingTaskManager final : public TaskManager {
  public:
    std::vector<TaskState<>*> added;
    std::vector<TaskState<>*> completed;
    int done_calls = 0;

  private:
    void on_add(not_null<TaskState<>*> task) noexcept final { added.push_back(task); }

    void on_child_done(not_null<TaskState<>*> task) noexcept final {
        completed.push_back(task);
        TaskManager::on_child_done(task);
    }

    void on_done() noexcept final { ++done_calls; }
};

class FailFastTaskManager final : public TaskManager {
  private:
    void on_child_done(not_null<TaskState<>*> task) noexcept final {
        if (auto exception = task->exception()) {
            set_exception(exception);
            cancel();
            return;
        }
        TaskManager::on_child_done(task);
    }
};

Coro<int> return_int(int value = 42) { co_return value; }

Coro<void> throw_runtime() {
    throw std::runtime_error("boom");
    co_return;
}

Coro<void> throw_cancelled() {
    throw CancelledError{};
    co_return;
}

Coro<void> await_gate(Future<void> gate) { co_await gate; }

Coro<void> increment(int* calls) {
    ++*calls;
    co_return;
}

Coro<void> add_sibling(TaskManager* manager, int* calls) {
    (void)manager->start_soon(increment(calls));
    co_return;
}

Coro<void> set_gate(Future<void> gate) {
    gate.set_void();
    co_return;
}

Coro<void> yield_once() { co_return; }

Coro<void> start_only(TaskManager* manager) { co_await manager->start(); }

Coro<void> start_close_join(TaskManager* manager) {
    co_await manager->start();
    manager->close();
    co_await manager->join();
}

Coro<void> start_and_close(TaskManager* manager) {
    co_await manager->start();
    manager->close();
}

Coro<void> abandon_started_manager(bool* caught) {
    try {
        TaskManager manager;
        co_await manager.start();
        (void)manager.start_soon(return_int());
    } catch (std::logic_error const&) {
        *caught = true;
    }
}

Coro<void> abandon_manager_with_pending_child(bool* caught) {
    Future<void> never;
    try {
        TaskManager manager;
        co_await manager.start();
        (void)manager.start_soon(await_gate(never));
        co_await start_soon(yield_once());
    } catch (std::logic_error const&) {
        *caught = true;
    }
}

Coro<void> join_manager(TaskManager* manager, bool* joined) {
    co_await manager->join();
    *joined = true;
}

Coro<void> request_second_join_cancellation(
    Future<void> started,
    Future<void> work,
    TaskState<>** joiner_state,
    bool* second_cancel_requested
) {
    started.set_void();
    try {
        co_await work;
    } catch (CancelledError const&) {
        *second_cancel_requested = true;
        (*joiner_state)->cancel();
        throw;
    }
    co_return;
}

Coro<void> join_and_expose_task(
    TaskManager* manager, TaskState<>** joiner_state, Future<void> started
) {
    *joiner_state = current_task().get();
    started.set_void();
    co_await manager->join();
}

}  // namespace

static_assert(!std::is_copy_constructible_v<TaskManager>);
static_assert(!std::is_copy_assignable_v<TaskManager>);
static_assert(!std::is_move_constructible_v<TaskManager>);
static_assert(!std::is_move_assignable_v<TaskManager>);

TEST(TestTaskManager, StartBindsManagerAndStartSoonReturnsStartedTask) {
    auto body = []() -> Coro<int> {
        RecordingTaskManager manager;
        EXPECT_FALSE(manager.started());
        co_await manager.start();
        EXPECT_TRUE(manager.started());

        Task<int> child = manager.start_soon(return_int());
        EXPECT_TRUE(child.started());
        co_await manager.join();

        EXPECT_TRUE(manager.done());
        EXPECT_EQ(manager.added.size(), 1u);
        EXPECT_EQ(manager.completed.size(), 1u);
        EXPECT_EQ(manager.done_calls, 1);
        co_return child.result();
    };

    EXPECT_EQ(run(body()), 42);
}

TEST(TestTaskManager, StartSoonBeforeStartIsRejected) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        EXPECT_THROW((void)manager.start_soon(return_int()), std::logic_error);
        co_return;
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, StartingTwiceIsRejected) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        co_await manager.start();
        EXPECT_THROW((void)manager.start(), std::logic_error);
        manager.close();
        co_await manager.join();
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, JoiningBeforeStartIsRejected) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        bool rejected = false;
        try {
            co_await manager.join();
        } catch (std::logic_error const&) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, EmptyManagerRequiresExplicitClose) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        co_await manager.start();
        EXPECT_FALSE(manager.done());
        manager.close();
        EXPECT_TRUE(manager.done());
        co_await manager.join();
        EXPECT_TRUE(manager.done());
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, JoinDoesNotCloseManager) {
    auto observe_open = [](TaskManager* manager, bool* observed) -> Coro<void> {
        *observed = !manager->closed();
        co_return;
    };

    auto body = [observe_open]() -> Coro<void> {
        TaskManager manager;
        bool observed_open = false;
        co_await manager.start();
        Task<void> child = manager.start_soon(observe_open(&manager, &observed_open));
        co_await manager.join();
        EXPECT_TRUE(child.done());
        EXPECT_TRUE(observed_open);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, ChildCanAddSiblingWhileJoinWaits) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        int sibling_calls = 0;
        co_await manager.start();
        (void)manager.start_soon(add_sibling(&manager, &sibling_calls));
        co_await manager.join();
        EXPECT_EQ(sibling_calls, 1);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, CancelClosesManagerAndCancelsChildren) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> gate;
        co_await manager.start();
        Task<void> child = manager.start_soon(await_gate(gate));
        manager.cancel();
        EXPECT_TRUE(manager.closed());
        co_await manager.join();
        EXPECT_TRUE(child.done());
        EXPECT_THROW(child.result(), CancelledError);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, PolicyCanPropagateChildFailureAndCancelSiblings) {
    auto body = []() -> Coro<void> {
        FailFastTaskManager manager;
        Future<void> gate;
        co_await manager.start();
        (void)manager.start_soon(throw_runtime());
        Task<void> sibling = manager.start_soon(await_gate(gate));
        EXPECT_THROW(co_await manager.join(), std::runtime_error);
        EXPECT_TRUE(manager.done());
        EXPECT_THROW(sibling.result(), CancelledError);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, CancelledManagerExceptionIsNotJoinerCancellation) {
    auto body = []() -> Coro<void> {
        FailFastTaskManager manager;
        auto task = current_task();
        co_await manager.start();
        (void)manager.start_soon(throw_cancelled());

        bool caught = false;
        try {
            co_await manager.join();
        } catch (CancelledError const&) {
            caught = true;
        }
        EXPECT_TRUE(caught);
        EXPECT_TRUE(manager.done());
        EXPECT_FALSE(task->cancelled());
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, MultipleJoinersMayWaitForCompletion) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> gate;
        bool first_joined = false;
        bool second_joined = false;
        co_await manager.start();
        (void)manager.start_soon(await_gate(gate));

        Task<void> first = start_soon(join_manager(&manager, &first_joined));
        Task<void> second = start_soon(join_manager(&manager, &second_joined));
        Task<void> release = start_soon(set_gate(gate));

        co_await first;
        co_await second;
        co_await release;
        co_await manager.join();
        EXPECT_TRUE(first_joined);
        EXPECT_TRUE(second_joined);
        EXPECT_TRUE(manager.done());
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, CancelledJoinWaitsForChildrenAndCoalescesRepeatedRequests) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        TaskManager joiners;
        Future<void> child_started;
        Future<void> joiner_started;
        Future<void> work;
        TaskState<>* joiner_state = nullptr;
        bool second_cancel_requested = false;
        bool join_cancelled = false;

        co_await manager.start();
        co_await joiners.start();
        Task<void> child = manager.start_soon(request_second_join_cancellation(
            child_started, work, &joiner_state, &second_cancel_requested
        ));
        Task<void> joiner = joiners.start_soon(
            join_and_expose_task(&manager, &joiner_state, joiner_started)
        );

        co_await child_started;
        co_await joiner_started;
        EXPECT_NE(joiner_state, nullptr);
        joiner.cancel();
        try {
            co_await joiner;
        } catch (CancelledError const&) {
            join_cancelled = true;
        }

        EXPECT_TRUE(join_cancelled);
        EXPECT_TRUE(second_cancel_requested);
        EXPECT_TRUE(manager.done());
        EXPECT_TRUE(joiners.done());
        EXPECT_TRUE(child.done());
        EXPECT_TRUE(child.cancelled());
        EXPECT_TRUE(joiner.cancelling());
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, DestructorThrowsIfStartedButNotDone) {
    EXPECT_THROW(
        {
            TaskManager manager;
            run(start_only(&manager));
        },
        std::logic_error
    );
}

TEST(TestTaskManager, DestructorDoesNotThrowAfterDrainingWithoutJoin) {
    EXPECT_NO_THROW({
        TaskManager manager;
        run(start_and_close(&manager));
    });
}

TEST(TestTaskManager, DestructorDoesNotThrowAfterSuccessfulJoin) {
    EXPECT_NO_THROW({
        TaskManager manager;
        run(start_close_join(&manager));
    });
}

TEST(TestTaskManager, PrematureDestructorCancelsQueuedChildrenSafely) {
    bool caught = false;
    EXPECT_NO_THROW(run(abandon_started_manager(&caught)));
    EXPECT_TRUE(caught);
}

TEST(TestTaskManager, PrematureDestructorCancelsPendingChildrenSafely) {
    bool caught = false;
    EXPECT_NO_THROW(run(abandon_manager_with_pending_child(&caught)));
    EXPECT_TRUE(caught);
}

// LCOV_EXCL_BR_STOP

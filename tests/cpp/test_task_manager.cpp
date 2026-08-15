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
using coconext::Future;
using coconext::lookup_task;
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
    TaskState<>* joining_task,
    bool* second_cancel_requested
) {
    started.set_void();
    try {
        co_await work;
    } catch (CancelledError const&) {
        *second_cancel_requested = true;
        joining_task->cancel();
        throw;
    }
    co_return;
}

Coro<void> cancel_task(TaskState<>* task) {
    task->cancel();
    co_return;
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

TEST(TestTaskManager, StartSoonAdoptsAndStartsUnstartedTask) {
    auto body = []() -> Coro<void> {
        RecordingTaskManager manager;
        Task<int> task = []() -> Task<int> { co_return 42; }();
        auto state = task.get_state();

        EXPECT_FALSE(task.started());
        co_await manager.start();
        Task<int> adopted = manager.start_soon(task);

        EXPECT_EQ(adopted.get_state(), state);
        EXPECT_EQ(task.get_state(), state);
        EXPECT_TRUE(task.started());
        EXPECT_EQ(manager.added.size(), 1u);
        if (!manager.added.empty()) {
            EXPECT_EQ(manager.added.front(), state.get());
        }

        co_await manager.join();
        EXPECT_TRUE(manager.done());
        EXPECT_EQ(task.result(), 42);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, StartSoonAdoptsRvalueTaskWithoutWrapping) {
    auto body = []() -> Coro<void> {
        RecordingTaskManager manager;
        Task<int> task = []() -> Task<int> { co_return 42; }();
        auto state = task.get_state();

        co_await manager.start();
        Task<int> adopted = manager.start_soon(std::move(task));

        EXPECT_EQ(adopted.get_state(), state);
        EXPECT_TRUE(adopted.started());
        co_await manager.join();
        EXPECT_EQ(adopted.result(), 42);
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, StartSoonRejectsCompletedTask) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Task<int> task = []() -> Task<int> { co_return 42; }();

        co_await manager.start();
        EXPECT_EQ(co_await task, 42);
        EXPECT_TRUE(task.done());
        EXPECT_THROW((void)manager.start_soon(task), std::logic_error);

        manager.close();
        co_await manager.join();
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, CancellingManagerCancelsAdoptedTaskDirectly) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        Future<void> never;
        Task<void> task = [](Future<void> gate) -> Task<void> { co_await gate; }(never);

        co_await manager.start();
        Task<void> adopted = manager.start_soon(task);
        manager.cancel();
        co_await manager.join();

        EXPECT_EQ(adopted.get_state(), task.get_state());
        EXPECT_TRUE(task.done());
        EXPECT_TRUE(task.cancelled());
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, StartSoonRejectsTaskAlreadyOwnedByManager) {
    auto body = []() -> Coro<void> {
        TaskManager first;
        TaskManager second;
        co_await first.start();
        co_await second.start();

        Task<int> task = first.start_soon(return_int());
        EXPECT_THROW((void)second.start_soon(task), std::logic_error);

        second.close();
        co_await first.join();
        co_await second.join();
    };

    EXPECT_NO_THROW(run(body()));
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
        auto task = lookup_task();
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

TEST(TestTaskManager, JoinFromTaskOtherThanOwnerIsRejected) {
    auto body = []() -> Coro<void> {
        TaskManager manager;
        bool joined = false;
        co_await manager.start();

        Task<void> other = manager.start_soon(join_manager(&manager, &joined));
        EXPECT_THROW(co_await other, std::logic_error);
        EXPECT_FALSE(joined);
        co_await manager.join();
        EXPECT_TRUE(manager.done());
    };

    EXPECT_NO_THROW(run(body()));
}

TEST(TestTaskManager, CancelledJoinWaitsForChildrenAndCoalescesRepeatedRequests) {
    bool second_cancel_requested = false;
    bool manager_done = false;
    bool child_done = false;
    bool child_cancelled = false;

    auto body = [&]() -> Coro<void> {
        TaskManager manager;
        Future<void> child_started;
        Future<void> work;
        auto joining_task = lookup_task();

        co_await manager.start();
        Task<void> child = manager.start_soon(request_second_join_cancellation(
            child_started, work, joining_task, &second_cancel_requested
        ));

        co_await child_started;
        (void)start_soon(cancel_task(joining_task));
        try {
            co_await manager.join();
        } catch (CancelledError const&) {
            manager_done = manager.done();
            child_done = child.done();
            child_cancelled = child.cancelled();
            throw;
        }
    };

    EXPECT_THROW(run(body()), CancelledError);
    EXPECT_TRUE(second_cancel_requested);
    EXPECT_TRUE(manager_done);
    EXPECT_TRUE(child_done);
    EXPECT_TRUE(child_cancelled);
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

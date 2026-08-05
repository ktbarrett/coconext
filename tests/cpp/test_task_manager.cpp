// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/not_null.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <stdexcept>
#include <utility>
#include <vector>

using coconext::Coro;
using coconext::not_null;
using coconext::run;
using coconext::Task;
using coconext::TaskManager;
using coconext::TaskManagerState;
using coconext::TaskState;

namespace {

// Minimal TaskManager subclass for tests: auto-closes on drain (so awaiting
// it after all children finish resolves), forwards the first child exception,
// otherwise sets void.
class SimpleTaskManagerState final : public TaskManagerState<void> {
  public:
    std::vector<TaskState<>*> added;
    std::vector<TaskState<>*> completed;
    int drain_calls = 0;

  private:
    void on_add(not_null<TaskState<>*> task) noexcept final { added.push_back(task); }

    void on_child_done(not_null<TaskState<>*> task) noexcept final {
        completed.push_back(task);
        if (task->exception() && !first_exc_) {
            first_exc_ = task->exception();
        }
        if (tasks_.empty()) {
            this->close();
        }
    }

    void on_drain_complete() noexcept final {
        ++drain_calls;
        if (first_exc_) {
            this->set_exception(first_exc_);
        } else {
            this->set_void();
        }
    }

    std::exception_ptr first_exc_;
};

using SimpleTaskManager = TaskManager<SimpleTaskManagerState>;

SimpleTaskManager make_tm() {
    return SimpleTaskManager{
        not_null<SimpleTaskManagerState*>(new SimpleTaskManagerState{})
    };
}

Coro<int> coro_return_int() { co_return 42; }

Coro<void> coro_return_void() { co_return; }

Coro<void> coro_throws_runtime() {
    throw std::runtime_error("boom");
    co_return;
}

}  // namespace

// -- start_soon / add semantics --------------------------------------------

TEST(TestTaskManager, AwaitingManagerStartsQueuedChildren) {
    // Add children before the manager is started. They must not run until
    // the manager itself is started (here, by being awaited).
    auto body = [](SimpleTaskManager tm, Task<int> child) -> Coro<int> {
        // Child was added before the manager was started; must still be
        // unstarted at this point.
        EXPECT_FALSE(child.started());
        EXPECT_FALSE(tm.get_state()->started());
        co_await tm;
        // After awaiting the manager, the child must have run to completion.
        EXPECT_TRUE(child.done());
        co_return child.result();
    };

    SimpleTaskManager tm = make_tm();
    Task<int> child = coro_return_int();
    tm.add(child.get_state());

    EXPECT_EQ(run(body(tm, child)), 42);
    EXPECT_EQ(tm.get_state()->added.size(), 1u);
    EXPECT_EQ(tm.get_state()->completed.size(), 1u);
    EXPECT_EQ(tm.get_state()->drain_calls, 1);
}

TEST(TestTaskManager, AddAfterManagerStartedStartsImmediately) {
    // Once a manager is running, add()'d tasks must be scheduled right away.
    // We start the manager via its own start_soon() so the manager is running
    // by the time we call add() on `late`.
    auto body = [](SimpleTaskManager tm) -> Coro<void> {
        tm.get_state()->start_soon();
        EXPECT_TRUE(tm.get_state()->started());
        Task<int> late = coro_return_int();
        EXPECT_FALSE(late.started());
        tm.add(late.get_state());
        EXPECT_TRUE(late.started());
        co_await tm;
        EXPECT_TRUE(late.done());
        EXPECT_EQ(late.result(), 42);
        co_return;
    };

    SimpleTaskManager tm = make_tm();
    Task<void> priming = coro_return_void();
    tm.add(priming.get_state());

    EXPECT_NO_THROW(run(body(tm)));
    EXPECT_EQ(tm.get_state()->completed.size(), 2u);
}

TEST(TestTaskManager, AddRejectsTaskAlreadyInAnotherManager) {
    auto body = [](SimpleTaskManager a, SimpleTaskManager b) -> Coro<void> {
        Task<int> t = coro_return_int();
        a.add(t.get_state());
        EXPECT_THROW(b.add(t.get_state()), std::runtime_error);
        co_await a;
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm(), make_tm())));
}

TEST(TestTaskManager, ChildExceptionPropagatesViaHook) {
    auto body = [](SimpleTaskManager tm) -> Coro<void> {
        Task<void> t = coro_throws_runtime();
        tm.add(t.get_state());
        // Awaiting the manager should rethrow the child's exception via
        // set_exception from on_drain_complete.
        EXPECT_THROW(co_await tm, std::runtime_error);
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm())));
}

TEST(TestTaskManager, DrainCompleteFiresExactlyOnceAfterClose) {
    // No auto-close on drain: an explicit close (from on_child_done in this
    // test manager) is what triggers on_drain_complete, and it must fire
    // exactly once.
    auto body = [](SimpleTaskManager tm) -> Coro<void> {
        Task<int> a = coro_return_int();
        Task<int> b = coro_return_int();
        tm.add(a.get_state());
        tm.add(b.get_state());
        co_await tm;
        co_return;
    };

    SimpleTaskManager tm = make_tm();
    run(body(tm));
    EXPECT_EQ(tm.get_state()->drain_calls, 1);
    EXPECT_EQ(tm.get_state()->completed.size(), 2u);
}

TEST(TestTaskManager, ManagerCancelCancelsChildren) {
    // A manager cancel() must propagate to every child. The manager must be
    // started before cancel() -- children cannot be cancelled before they
    // have an EventLoop.
    auto body = [](SimpleTaskManager tm) -> Coro<void> {
        Task<int> t = coro_return_int();
        tm.add(t.get_state());
        tm.get_state()->start_soon();
        tm.cancel();
        EXPECT_TRUE(tm.cancelled());
        EXPECT_TRUE(t.cancelled());
        try {
            co_await tm;
        } catch (...) {
            // Cancellation surfaces through our hook as the child's exception.
        }
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm())));
}

// LCOV_EXCL_BR_STOP

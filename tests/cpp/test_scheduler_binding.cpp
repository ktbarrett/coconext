// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/not_null.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <stdexcept>
#include <thread>
#include <utility>

using coconext::Coro;
using coconext::current_context;
using coconext::current_task;
using coconext::not_null;
using coconext::run;
using coconext::Task;
using coconext::TaskContext;
using coconext::TaskManager;
using coconext::TaskManagerState;
using coconext::TaskState;

namespace {

class SimpleTaskManagerState final : public TaskManagerState<void> {
  private:
    void on_add(not_null<TaskState<>*>) noexcept final {}

    void on_child_done(not_null<TaskState<>*> task) noexcept final {
        if (task->exception() && !first_exc_) {
            first_exc_ = task->exception();
        }
        if (tasks_.empty()) {
            this->close();
        }
    }

    void on_drain_complete() noexcept final {
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

}  // namespace

// -- current_task / current_context outside any task -----------------------

TEST(TestSchedulerBinding, CurrentTaskThrowsOutsideTask) {
    EXPECT_THROW((void)current_task(), std::runtime_error);
}

TEST(TestSchedulerBinding, CurrentContextThrowsOutsideTask) {
    EXPECT_THROW((void)current_context(), std::runtime_error);
}

TEST(TestSchedulerBinding, CurrentTaskInsideTaskReturnsSelf) {
    auto body = []() -> Coro<void> {
        auto ctxt = co_await coconext::get_context();
        auto here = current_task();
        EXPECT_EQ(here.get(), ctxt.get_task().get());
        co_return;
    };
    EXPECT_NO_THROW(run(body()));
}

// -- Task::start_soon: already-started error paths -------------------------

TEST(TestSchedulerBinding, TaskStartSoonNoArgTwiceThrows) {
    auto body = []() -> Coro<void> {
        Task<int> t = coro_return_int();
        t.start_soon();
        EXPECT_TRUE(t.started());
        EXPECT_THROW(t.start_soon(), std::runtime_error);
        (void)(co_await t);
        co_return;
    };
    EXPECT_NO_THROW(run(body()));
}

TEST(TestSchedulerBinding, TaskStartSoonCtxtTwiceThrows) {
    auto body = []() -> Coro<void> {
        auto ctxt = co_await coconext::get_context();
        Task<int> t = coro_return_int();
        t.start_soon(ctxt);
        EXPECT_TRUE(t.started());
        EXPECT_THROW(t.start_soon(ctxt), std::runtime_error);
        (void)(co_await t);
        co_return;
    };
    EXPECT_NO_THROW(run(body()));
}

// -- TaskManager::start_soon: already-started error paths ------------------

// SimpleTaskManagerState's on_drain_complete only fires from a child completing
// (via internal_child_done), so an empty + started + closed manager would never
// resolve. Prime with a trivial child that completes and triggers close.
Coro<int> coro_return_int_helper() { co_return 0; }

TEST(TestSchedulerBinding, TaskManagerStartSoonTwiceThrows) {
    Task<int> priming = coro_return_int_helper();
    auto body = [](SimpleTaskManager m, Task<int> child) -> Coro<void> {
        m.add(child.get_state());
        m.get_state()->start_soon();
        EXPECT_TRUE(m.get_state()->started());
        EXPECT_THROW(m.get_state()->start_soon(), std::runtime_error);
        co_await m;
        co_return;
    };
    EXPECT_NO_THROW(run(body(make_tm(), priming)));
}

TEST(TestSchedulerBinding, TaskManagerStartSoonCtxtTwiceThrows) {
    Task<int> priming = coro_return_int_helper();
    auto body = [](SimpleTaskManager m, Task<int> child) -> Coro<void> {
        auto ctxt = co_await coconext::get_context();
        m.add(child.get_state());
        m.get_state()->start_soon(ctxt);
        EXPECT_TRUE(m.get_state()->started());
        EXPECT_THROW(m.get_state()->start_soon(ctxt), std::runtime_error);
        co_await m;
        co_return;
    };
    EXPECT_NO_THROW(run(body(make_tm(), priming)));
}

// -- TaskManager::add: rejection paths -------------------------------------

TEST(TestSchedulerBinding, AddToClosedTaskManagerThrows) {
    auto body = [](SimpleTaskManager m) -> Coro<void> {
        m.get_state()->close();
        EXPECT_TRUE(m.get_state()->closed());
        Task<int> t = coro_return_int();
        EXPECT_THROW(m.add(t.get_state()), std::runtime_error);
        co_return;
    };
    EXPECT_NO_THROW(run(body(make_tm())));
}

TEST(TestSchedulerBinding, AddToDoneTaskManagerThrows) {
    Task<int> priming = coro_return_int_helper();
    auto body = [](SimpleTaskManager m, Task<int> child) -> Coro<void> {
        m.add(child.get_state());
        co_await m;
        EXPECT_TRUE(m.done());
        Task<int> t = coro_return_int();
        EXPECT_THROW(m.add(t.get_state()), std::runtime_error);
        co_return;
    };
    EXPECT_NO_THROW(run(body(make_tm(), priming)));
}

// -- Cross-EventLoop binding rejection -------------------------------------
//
// Regression guard for commit "Prevent cross-EventLoop scheduling on
// TaskManager". Runs the outer scope on a worker thread so the first
// run()'s EventLoop lives on the worker's stack; after join, that stack
// is unmapped and its address cannot collide with the main-thread loop
// address the second run() creates. Sequential runs on one thread would
// silently reuse the same stack slot.

TEST(TestSchedulerBinding, TaskStartSoonAcrossEventLoopsThrows) {
    Task<int> t = coro_return_int();

    std::thread worker([&t] {
        auto outer = [&t]() -> Coro<void> {
            t.start_soon();
            (void)(co_await t);
            co_return;
        };
        run(outer());
    });
    worker.join();

    ASSERT_TRUE(t.done());

    auto inner = [&t]() -> Coro<void> {
        auto ctxt = co_await coconext::get_context();
        // t.event_loop_ still points at the worker's now-unmapped loop.
        // The bind check compares that stale pointer against the current
        // loop and must throw before touching the stale loop object.
        EXPECT_THROW(t.start_soon(ctxt), std::runtime_error);
        co_return;
    };
    EXPECT_NO_THROW(run(inner()));
}

// LCOV_EXCL_BR_STOP

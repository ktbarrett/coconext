// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/future.hpp>
#include <coconext/not_null.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <stdexcept>
#include <thread>
#include <utility>

using coconext::AbstractFuture;
using coconext::AbstractFutureState;
using coconext::Cancelled;
using coconext::Coro;
using coconext::current_context;
using coconext::current_task;
using coconext::Future;
using coconext::not_null;
using coconext::run;
using coconext::start_soon;
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

// -- Task cancel / uncancel edge cases -------------------------------------

namespace {

Coro<void> self_cancel_coro() {
    current_task()->cancel();  // running() -> throws Cancelled in place
    co_return;
}

}  // namespace

TEST(TestSchedulerBinding, SelfCancelFromRunningThrowsCancelled) {
    // A task that cancels itself while Running has cancel() throw Cancelled
    // synchronously; it propagates out of the coroutine body and shows up
    // via run() as the task's exception.
    EXPECT_THROW(run(self_cancel_coro()), Cancelled);
}

TEST(TestSchedulerBinding, UncancelOnNonCancelledTaskThrows) {
    Task<int> t = coro_return_int();
    EXPECT_THROW(t.uncancel(), std::runtime_error);
}

TEST(TestSchedulerBinding, UncancelAfterDoneIsNoOp) {
    Task<int> t = coro_return_int();
    (void)run(t);
    EXPECT_TRUE(t.done());
    EXPECT_NO_THROW(t.uncancel());
    EXPECT_NO_THROW(t.uncancel());  // idempotent
}

// -- Task::cancel() while Pending ------------------------------------------

namespace {

// A never-resolved Future so the awaiter stays Pending until cancelled.
class UnresolvableVoidState : public AbstractFutureState<void> {};
using UnresolvableVoid = AbstractFuture<UnresolvableVoidState>;

// Coro-returning form: the Task-from-Coro wrapper stacks an extra frame
// between the Task's outer handle and the coroutine actually suspended on
// the Future. cancel() reschedules the pending awaiter event, which knows
// to resume the inner Coro's handle -- if cancel() built a fresh event
// against the Task's outer handle instead, the inner Coro would never
// resume and Cancelled would surface as "Coro does not have a result".
Coro<void> await_forever(UnresolvableVoid fut) { co_await fut; }

Coro<void> cancel_other(Task<void> other) {
    other.cancel();
    co_return;
}

}  // namespace

TEST(TestSchedulerBinding, CancelPendingTaskWakesItWithCancelled) {
    // Awaiter suspends on a never-resolved Future. Canceller runs second,
    // calls cancel() while the awaiter is Pending -- the awaiter is rescheduled
    // and await_resume sees cancelled_ > 0 and throws Cancelled.
    UnresolvableVoid fut;
    Task<void> awaiter = await_forever(fut);
    Task<void> canceller = cancel_other(awaiter);

    auto body = [](SimpleTaskManager tm, Task<void> a, Task<void> c) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(c.get_state());
        try {
            co_await tm;  // manager surfaces awaiter's Cancelled
        } catch (Cancelled const&) {}
        EXPECT_TRUE(a.done());
        EXPECT_THROW(a.result(), Cancelled);
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm(), awaiter, canceller)));
}

// -- Free-function start_soon() spawning API -------------------------------

TEST(TestSchedulerBinding, FreeStartSoonSpawnsAndReturnsAwaitableTask) {
    auto body = []() -> Coro<int> {
        Task<int> t = start_soon(coro_return_int());
        EXPECT_TRUE(t.started());
        int v = co_await t;
        co_return v;
    };
    EXPECT_EQ(run(body()), 42);
}

TEST(TestSchedulerBinding, FreeStartSoonAcceptsTaskLValue) {
    auto body = []() -> Coro<int> {
        Task<int> t = coro_return_int();
        Task<int> same = start_soon(t);
        EXPECT_EQ(t.get_state().get(), same.get_state().get());
        int v = co_await t;
        co_return v;
    };
    EXPECT_EQ(run(body()), 42);
}

// -- add_done_callback: Task and TaskManager -------------------------------

TEST(TestSchedulerBinding, TaskAddDoneCallbackFiresExactlyOnce) {
    int calls = 0;
    Task<int> t = coro_return_int();
    t.add_done_callback([&calls]() { ++calls; });
    (void)run(t);
    EXPECT_EQ(calls, 1);
}

TEST(TestSchedulerBinding, TaskManagerAddDoneCallbackFiresExactlyOnce) {
    int calls = 0;
    SimpleTaskManager m = make_tm();
    m.add_done_callback([&calls]() { ++calls; });
    Task<int> priming = coro_return_int_helper();
    auto body = [](SimpleTaskManager tm, Task<int> child) -> Coro<void> {
        tm.add(child.get_state());
        co_await tm;
        co_return;
    };
    run(body(m, priming));
    EXPECT_EQ(calls, 1);
}

// -- AwaitableAwaiter::await_ready short-circuit ---------------------------

TEST(TestSchedulerBinding, AwaitAlreadyDoneTaskReturnsValueViaShortCircuit) {
    // Task run to completion in one run(); awaited in a second run(). The
    // awaiter's await_ready observes task->done() and returns true, so
    // await_suspend never runs. await_resume must return the value without
    // going through the cancellation-check path (task_ stays nullptr).
    Task<int> pre = coro_return_int();
    ASSERT_EQ(run(pre), 42);
    ASSERT_TRUE(pre.done());

    auto body = [&pre]() -> Coro<int> {
        int v = co_await pre;
        co_return v;
    };
    EXPECT_EQ(run(body()), 42);
}

// -- Coro handle lifecycle -------------------------------------------------

TEST(TestSchedulerBinding, CoroGetStateReturnsStableReference) {
    Coro<int> c = coro_return_int();
    auto& s1 = c.get_state();
    auto& s2 = c.get_state();
    EXPECT_EQ(&s1, &s2);
}

TEST(TestSchedulerBinding, CoroMoveAssignmentOverLiveHandleWorks) {
    // Move-assign over a Coro that already owns a coroutine handle. The
    // destination's old handle must be destroyed (no leak), the source
    // must end up empty (no double-destroy at scope exit), and the new
    // handle must remain runnable through the destination.
    Coro<int> a = coro_return_int_helper();  // returns 0
    Coro<int> b = coro_return_int();         // returns 42
    auto* new_state = &b.get_state();
    a = std::move(b);
    EXPECT_EQ(&a.get_state(), new_state);
    EXPECT_EQ(run(std::move(a)), 42);
}

TEST(TestSchedulerBinding, CoroMovedFromDestroysWithoutCrash) {
    // A moved-from Coro must be destructible without touching its (now
    // transferred) handle. Any double-destroy would trip UB/asan.
    Coro<int> a = coro_return_int();
    { Coro<int> b = std::move(a); }
    SUCCEED();
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

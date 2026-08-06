// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/future.hpp>
#include <coconext/not_null.hpp>
#include <coconext/outcome.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

using coconext::Coro;
using coconext::Future;
using coconext::FutureState;
using coconext::not_null;
using coconext::run;
using coconext::Task;
using coconext::TaskManager;
using coconext::TaskManagerState;
using coconext::TaskState;

namespace {

// Expose the protected setters so tests can resolve a Future by hand.
class IntFutureState : public FutureState<int> {
  public:
    using FutureState<int>::set_result;
    using FutureState<int>::set_exception;
};

class VoidFutureState : public FutureState<void> {
  public:
    using FutureState<void>::set_void;
    using FutureState<void>::set_exception;
};

using IntFuture = Future<IntFutureState>;
using VoidFuture = Future<VoidFutureState>;

// Auto-closing TaskManager that surfaces the first child exception. Used
// to schedule setter/awaiter pairs deterministically: children added
// before start run in FIFO order once the manager is awaited.
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

// Free-function coroutines so the frame owns its arguments -- do NOT use
// capturing lambdas here (the lambda temporary would be destroyed at the
// end of the full expression, dangling the captures across suspend points).
Coro<int> int_awaiter(IntFuture fut) { co_return co_await fut; }

Coro<void> void_awaiter(VoidFuture fut) { co_await fut; }

Coro<void> int_setter(IntFuture fut, int value) {
    fut.get_state()->set_result(value);
    co_return;
}

Coro<void> void_setter(VoidFuture fut) {
    fut.get_state()->set_void();
    co_return;
}

Coro<void> int_thrower(IntFuture fut) {
    fut.get_state()->set_exception(std::make_exception_ptr(std::runtime_error("boom")));
    co_return;
}

}  // namespace

// -- basic state queries before completion ---------------------------------

TEST(TestFuture, FreshFutureNotDone) {
    IntFuture fut;
    EXPECT_FALSE(fut.done());
    EXPECT_THROW((void)fut.result(), std::runtime_error);
    EXPECT_THROW((void)fut.exception(), std::runtime_error);
}

TEST(TestFuture, GetStateReturnsSameStateAcrossCopies) {
    IntFuture a;
    IntFuture b = a;
    EXPECT_EQ(a.get_state().get(), b.get_state().get());
}

// -- co_await + set_result / set_void / set_exception ----------------------
//
// Sequencing: add the awaiter first, then the setter. Both are queued on the
// TaskManager and dispatched FIFO when the manager starts, so the awaiter
// binds its event loop into the Future via on_awaited() before the setter's
// set_*() call needs it. The awaiter's Task result gives us the awaited value.

TEST(TestFuture, SetResultResolvesAwaiter) {
    IntFuture fut;
    Task<int> awaiter = int_awaiter(fut);
    Task<void> setter = int_setter(fut, 42);

    auto body = [](SimpleTaskManager tm, Task<int> a, Task<void> s) -> Coro<int> {
        tm.add(a.get_state());
        tm.add(s.get_state());
        co_await tm;
        co_return a.result();
    };

    EXPECT_EQ(run(body(make_tm(), awaiter, setter)), 42);
}

TEST(TestFuture, SetVoidResolvesAwaiter) {
    VoidFuture fut;
    Task<void> awaiter = void_awaiter(fut);
    Task<void> setter = void_setter(fut);

    auto body = [](SimpleTaskManager tm, Task<void> a, Task<void> s) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(s.get_state());
        co_await tm;
        a.result();  // rethrows if the awaiter observed an exception
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm(), awaiter, setter)));
}

TEST(TestFuture, SetExceptionPropagatesThroughAwait) {
    IntFuture fut;
    Task<int> awaiter = int_awaiter(fut);
    Task<void> thrower = int_thrower(fut);

    auto body = [](SimpleTaskManager tm, Task<int> a, Task<void> t) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(t.get_state());
        try {
            co_await tm;
        } catch (...) {
            // manager surfaces the awaiter's runtime_error; swallow so we can
            // inspect the awaiter task directly.
        }
        EXPECT_TRUE(a.done());
        EXPECT_THROW((void)a.result(), std::runtime_error);
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm(), awaiter, thrower)));
}

TEST(TestFuture, ExceptionAccessorReturnsPtrAfterSetException) {
    IntFuture fut;
    Task<int> awaiter = int_awaiter(fut);
    Task<void> thrower = int_thrower(fut);

    auto body = [&fut](SimpleTaskManager tm, Task<int> a, Task<void> t) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(t.get_state());
        try {
            co_await tm;
        } catch (...) {}
        EXPECT_TRUE(fut.done());
        EXPECT_TRUE(fut.exception() != nullptr);
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm(), awaiter, thrower)));
}

TEST(TestFuture, ExceptionReturnsNullptrOnSuccessfulResult) {
    IntFuture fut;
    Task<int> awaiter = int_awaiter(fut);
    Task<void> setter = int_setter(fut, 1);

    auto body = [&fut](SimpleTaskManager tm, Task<int> a, Task<void> s) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(s.get_state());
        co_await tm;
        EXPECT_TRUE(fut.done());
        EXPECT_TRUE(fut.exception() == nullptr);
        EXPECT_EQ(fut.result(), 1);
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm(), awaiter, setter)));
}

// -- multiple simultaneous waiters ----------------------------------------

TEST(TestFuture, MultipleWaitersAllResumedBySetResult) {
    IntFuture fut;
    Task<int> a1 = int_awaiter(fut);
    Task<int> a2 = int_awaiter(fut);
    Task<void> setter = int_setter(fut, 7);

    auto body =
        [](SimpleTaskManager tm, Task<int> t1, Task<int> t2, Task<void> s) -> Coro<void> {
        tm.add(t1.get_state());
        tm.add(t2.get_state());
        tm.add(s.get_state());
        co_await tm;
        EXPECT_EQ(t1.result(), 7);
        EXPECT_EQ(t2.result(), 7);
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm(), a1, a2, setter)));
}

// -- done callbacks --------------------------------------------------------

TEST(TestFuture, AddDoneCallbackFiresExactlyOnce) {
    IntFuture fut;
    int calls = 0;
    fut.add_done_callback([&calls]() { ++calls; });
    Task<int> awaiter = int_awaiter(fut);
    Task<void> setter = int_setter(fut, 9);

    auto body = [](SimpleTaskManager tm, Task<int> a, Task<void> s) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(s.get_state());
        co_await tm;
        co_return;
    };

    run(body(make_tm(), awaiter, setter));
    EXPECT_EQ(calls, 1);
}

TEST(TestFuture, MultipleDoneCallbacksFireInRegistrationOrder) {
    IntFuture fut;
    std::string order;
    fut.add_done_callback([&order]() { order += "a"; });
    fut.add_done_callback([&order]() { order += "b"; });
    fut.add_done_callback([&order]() { order += "c"; });
    Task<int> awaiter = int_awaiter(fut);
    Task<void> setter = int_setter(fut, 1);

    auto body = [](SimpleTaskManager tm, Task<int> a, Task<void> s) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(s.get_state());
        co_await tm;
        co_return;
    };

    run(body(make_tm(), awaiter, setter));
    EXPECT_EQ(order, "abc");
}

TEST(TestFuture, DoneCallbackFiresBeforeAwaiterResumes) {
    // Contract: on_done() invokes callbacks *then* schedules the waiter
    // events. So a callback must observe done()==true, and the awaiter,
    // when it resumes, must observe that the callback already ran.
    IntFuture fut;
    int callback_seen_done = -1;
    int flag = 0;
    fut.add_done_callback([&]() {
        callback_seen_done = fut.done() ? 1 : 0;
        flag = 1;
    });

    // Custom awaiter that captures `flag` at resume time.
    int awaiter_seen_flag = -1;
    auto awaiter_body = [&](IntFuture f) -> Coro<void> {
        (void)(co_await f);
        awaiter_seen_flag = flag;
        co_return;
    };
    Task<void> awaiter = awaiter_body(fut);
    Task<void> setter = int_setter(fut, 3);

    auto body = [](SimpleTaskManager tm, Task<void> a, Task<void> s) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(s.get_state());
        co_await tm;
        co_return;
    };

    run(body(make_tm(), awaiter, setter));
    EXPECT_EQ(callback_seen_done, 1);
    EXPECT_EQ(awaiter_seen_flag, 1);
}

// -- shared handle semantics ----------------------------------------------

TEST(TestFuture, CopyingFutureSharesResolution) {
    // A copied Future observes the same completion as the original.
    IntFuture fut;
    IntFuture copy = fut;
    Task<int> awaiter = int_awaiter(copy);
    Task<void> setter = int_setter(fut, 11);

    auto body = [&fut,
                 &copy](SimpleTaskManager tm, Task<int> a, Task<void> s) -> Coro<void> {
        tm.add(a.get_state());
        tm.add(s.get_state());
        co_await tm;
        EXPECT_TRUE(fut.done());
        EXPECT_TRUE(copy.done());
        EXPECT_EQ(fut.result(), 11);
        EXPECT_EQ(a.result(), 11);
        co_return;
    };

    EXPECT_NO_THROW(run(body(make_tm(), awaiter, setter)));
}

// LCOV_EXCL_BR_STOP

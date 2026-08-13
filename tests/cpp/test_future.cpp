// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/coro.hpp>
#include <coconext/future.hpp>
#include <coconext/run.hpp>
#include <coconext/task.hpp>

#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

using coconext::AbstractFuture;
using coconext::AbstractFutureState;
using coconext::Coro;
using coconext::Future;
using coconext::FutureState;
using coconext::not_null;
using coconext::run;
using coconext::start_soon;
using coconext::Task;

namespace {

class IntFutureState : public AbstractFutureState<int> {
  public:
    using AbstractFutureState<int>::set_exception;
    using AbstractFutureState<int>::set_result;
};

class VoidFutureState : public AbstractFutureState<void> {
  public:
    using AbstractFutureState<void>::set_exception;
    using AbstractFutureState<void>::set_void;
};

using IntFuture = AbstractFuture<IntFutureState>;
using VoidFuture = AbstractFuture<VoidFutureState>;

static_assert(requires(FutureState<int>& state, std::exception_ptr exception) {
    state.set_result(1);
    state.set_exception(exception);
});
static_assert(requires(FutureState<void>& state, std::exception_ptr exception) {
    state.set_void();
    state.set_exception(exception);
});

Coro<int> int_awaiter(IntFuture future) { co_return co_await future; }

Coro<void> void_awaiter(VoidFuture future) { co_await future; }

Coro<void> int_setter(IntFuture future, int value) {
    future.get_state()->set_result(value);
    co_return;
}

Coro<void> void_setter(VoidFuture future) {
    future.get_state()->set_void();
    co_return;
}

Coro<void> int_thrower(IntFuture future) {
    future.get_state()->set_exception(std::make_exception_ptr(std::runtime_error("boom")));
    co_return;
}

Coro<int> resolve_int(IntFuture future, int value) {
    Task<int> awaiter = start_soon(int_awaiter(future));
    (void)start_soon(int_setter(future, value));
    co_return co_await awaiter;
}

Coro<void> resolve_void(VoidFuture future) {
    Task<void> awaiter = start_soon(void_awaiter(future));
    (void)start_soon(void_setter(future));
    co_await awaiter;
}

Coro<int> throw_into_awaiter(IntFuture future) {
    Task<int> awaiter = start_soon(int_awaiter(future));
    (void)start_soon(int_thrower(future));
    co_return co_await awaiter;
}

Coro<void> observe_callback_order(IntFuture future, int* flag, int* seen) {
    (void)(co_await future);
    *seen = *flag;
}

class ManualTrigger {
  public:
    void prime(std::function<void(int)> callback) {
        callback_ = std::move(callback);
        primed_ = true;
    }

    void unprime() noexcept {
        callback_ = {};
        primed_ = false;
        ++unprime_calls_;
    }

    void fire(int value) {
        auto callback = std::move(callback_);
        primed_ = false;
        callback(value);
    }

    [[nodiscard]] bool primed() const noexcept { return primed_; }
    [[nodiscard]] int unprime_calls() const noexcept { return unprime_calls_; }

  private:
    std::function<void(int)> callback_;
    bool primed_ = false;
    int unprime_calls_ = 0;
};

class TriggerFutureState final : public AbstractFutureState<int> {
  public:
    explicit TriggerFutureState(ManualTrigger& trigger) : trigger_(trigger) {
        trigger_.prime([this](int value) { set_result(value); });
    }

    ~TriggerFutureState() override {
        if (!done()) {
            trigger_.unprime();
        }
    }

    [[nodiscard]] bool primed() const noexcept { return trigger_.primed(); }

  private:
    ManualTrigger& trigger_;
};

class TriggerFuture final : public AbstractFuture<TriggerFutureState> {
  public:
    explicit TriggerFuture(ManualTrigger& trigger)
        : AbstractFuture<TriggerFutureState>(
              not_null<TriggerFutureState*>{new TriggerFutureState{trigger}}
          ) {}

    [[nodiscard]] bool primed() const noexcept { return get_state()->primed(); }
};

Coro<int> trigger_awaiter(TriggerFuture future) { co_return co_await future; }

Coro<void> trigger_firer(ManualTrigger* trigger, int value) {
    trigger->fire(value);
    co_return;
}

Coro<int> resolve_trigger(TriggerFuture future, ManualTrigger* trigger, int value) {
    Task<int> awaiter = start_soon(trigger_awaiter(future));
    (void)start_soon(trigger_firer(trigger, value));
    co_return co_await awaiter;
}

Coro<int> concrete_awaiter(Future<int> future) { co_return co_await future; }

Coro<void> concrete_setter(Future<int> future, int value) {
    future.set_result(value);
    co_return;
}

Coro<int> resolve_concrete(Future<int> future, int value) {
    Task<int> awaiter = start_soon(concrete_awaiter(future));
    (void)start_soon(concrete_setter(future, value));
    co_return co_await awaiter;
}

Coro<void> concrete_void_awaiter(Future<void> future) { co_await future; }

Coro<void> concrete_void_setter(Future<void> future) {
    future.set_void();
    co_return;
}

Coro<void> resolve_concrete_void(Future<void> future) {
    Task<void> awaiter = start_soon(concrete_void_awaiter(future));
    (void)start_soon(concrete_void_setter(future));
    co_await awaiter;
}

}  // namespace

TEST(TestFuture, FreshFutureNotDone) {
    IntFuture future;
    EXPECT_FALSE(future.done());
    EXPECT_THROW((void)future.result(), std::runtime_error);
    EXPECT_THROW((void)future.exception(), std::runtime_error);
}

TEST(TestFuture, CopiesShareState) {
    IntFuture original;
    IntFuture copy = original;
    EXPECT_EQ(original.get_state().get(), copy.get_state().get());
    EXPECT_EQ(run(resolve_int(copy, 11)), 11);
    EXPECT_TRUE(original.done());
    EXPECT_EQ(original.result(), 11);
}

TEST(TestFuture, SetResultResolvesAwaiter) {
    IntFuture future;
    EXPECT_EQ(run(resolve_int(future, 42)), 42);
    EXPECT_EQ(future.result(), 42);
    EXPECT_EQ(future.exception(), nullptr);
}

TEST(TestFuture, SetVoidResolvesAwaiter) {
    VoidFuture future;
    EXPECT_NO_THROW(run(resolve_void(future)));
    EXPECT_TRUE(future.done());
    EXPECT_EQ(future.exception(), nullptr);
}

TEST(TestFuture, SetExceptionPropagatesThroughAwait) {
    IntFuture future;
    EXPECT_THROW((void)run(throw_into_awaiter(future)), std::runtime_error);
    EXPECT_TRUE(future.exception() != nullptr);
    EXPECT_THROW((void)future.result(), std::runtime_error);
}

TEST(TestFuture, MultipleWaitersAreResumed) {
    auto body = [](IntFuture future) -> Coro<void> {
        Task<int> first = start_soon(int_awaiter(future));
        Task<int> second = start_soon(int_awaiter(future));
        (void)start_soon(int_setter(future, 7));
        EXPECT_EQ(co_await first, 7);
        EXPECT_EQ(co_await second, 7);
    };

    EXPECT_NO_THROW(run(body(IntFuture{})));
}

TEST(TestFuture, DoneCallbacksRunInRegistrationOrder) {
    IntFuture future;
    std::string order;
    future.add_done_callback([&order]() { order += "a"; });
    future.add_done_callback([&order]() { order += "b"; });
    future.add_done_callback([&order]() { order += "c"; });
    EXPECT_EQ(run(resolve_int(future, 1)), 1);
    EXPECT_EQ(order, "abc");
}

TEST(TestFuture, DoneCallbackRunsBeforeAwaiterResumes) {
    IntFuture future;
    int flag = 0;
    int seen = -1;
    future.add_done_callback([&flag]() { flag = 1; });

    auto body = [](IntFuture shared, int* flag_ptr, int* seen_ptr) -> Coro<void> {
        Task<void> awaiter = start_soon(observe_callback_order(shared, flag_ptr, seen_ptr));
        (void)start_soon(int_setter(shared, 3));
        co_await awaiter;
    };

    run(body(future, &flag, &seen));
    EXPECT_EQ(seen, 1);
}

TEST(TestFuture, TriggerBackedStateUnprimesWhenLastReferenceDrops) {
    ManualTrigger trigger;
    {
        TriggerFuture future{trigger};
        EXPECT_TRUE(future.primed());
        {
            TriggerFuture copy = future;
            EXPECT_TRUE(copy.primed());
        }
        EXPECT_TRUE(trigger.primed());
        EXPECT_EQ(trigger.unprime_calls(), 0);
    }
    EXPECT_FALSE(trigger.primed());
    EXPECT_EQ(trigger.unprime_calls(), 1);
}

TEST(TestFuture, TriggerCallbackResolvesAbstractFuture) {
    ManualTrigger trigger;
    TriggerFuture future{trigger};
    EXPECT_EQ(run(resolve_trigger(future, &trigger, 17)), 17);
    EXPECT_FALSE(trigger.primed());
    EXPECT_EQ(trigger.unprime_calls(), 0);
}

TEST(TestFuture, ConcreteFutureProvidesPublicCompletionAPI) {
    Future<int> future;
    EXPECT_EQ(run(resolve_concrete(future, 23)), 23);
}

TEST(TestFuture, ConcreteFutureCanResolveBeforeFirstAwait) {
    Future<int> future;
    future.set_result(29);
    EXPECT_EQ(run(concrete_awaiter(future)), 29);
}

TEST(TestFuture, ConcreteVoidFutureProvidesPublicCompletionAPI) {
    Future<void> future;
    EXPECT_NO_THROW(run(resolve_concrete_void(future)));
}

TEST(TestFuture, ConcreteFutureRejectsNullException) {
    Future<int> future;
    EXPECT_THROW(future.set_exception(nullptr), std::invalid_argument);
}

TEST(TestFuture, ConcreteFutureProvidesPublicExceptionAPI) {
    Future<int> future;
    future.set_exception(std::make_exception_ptr(std::runtime_error("boom")));
    EXPECT_THROW((void)run(concrete_awaiter(future)), std::runtime_error);
}

// LCOV_EXCL_BR_STOP

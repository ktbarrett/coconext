#ifndef COCONEXT_TASK_MANAGER_HPP
#define COCONEXT_TASK_MANAGER_HPP

#include <coconext/future.hpp>
#include <coconext/intrusive_deque.hpp>
#include <coconext/task.hpp>
#include <coconext/task_awaiter.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>

namespace coconext {

class TaskManager;

template <typename T>
T run(Task<T> task);

namespace detail {

class TaskManagerState {
    friend class ::coconext::TaskManager;

  public:
    void inc_ref() noexcept { ++ref_count_; }
    void dec_ref() noexcept {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    void add(Task<>& task) {
        bind_event_loop(task.get_state()->get_event_loop());
        if (task.unstarted()) {
            task.get_state()->start_soon(this);
        }
        if (cancelled_ > 0) {
            throw std::runtime_error("Cannot add task to cancelled TaskManager");
        }
        task.get_state()->inc_ref();
        tasks_.push_back(task.get_state());
    }

    EventLoop* get_event_loop() noexcept { return event_loop_; }

    bool done() const noexcept;
    bool cancelled() const noexcept;
    void result() const {}
    std::exception_ptr exception() const noexcept { return nullptr; }

    void cancel() noexcept {
        if (done()) {
            return;
        }
        if (!cancelled_) {
            for (auto& task : tasks_) {
                task.cancel();
            }
        }
        cancelled_++;
    }
    void uncancel() {
        if (done()) {
            return;
        }
        if (cancelled_ == 0) {
            throw std::runtime_error("TaskManager is not cancelled");
        }
        cancelled_--;
    }

    void child_done(TaskState<>* task) {
        task->remove_child();
        task->dec_ref();
        if (cancelled_ > 0 && tasks_.empty()) {
            on_done();
        }
    }

    void bind_event_loop(EventLoop* loop) {
        if (event_loop_ == nullptr) {
            event_loop_ = loop;
        } else if (event_loop_ != loop) {
            throw std::runtime_error("TaskManager is already bound to another EventLoop");
        }
    }

    class DoneFutureState;

    using DoneFuture = Future<void, DoneFutureState>;

    class DoneFutureState : public FutureState<void> {
        friend class TaskManagerState;

      public:
        template <typename PromiseType>
        void await_suspend(std::coroutine_handle<PromiseType> h) {
            FutureState<void>::await_suspend(h);
            task_manager_->bind_event_loop(h.promise().get_task()->get_event_loop());
        }

      private:
        explicit DoneFutureState(TaskManagerState* task_manager)
            : task_manager_(task_manager) {}

        TaskManagerState* task_manager_;
    };

    DoneFuture wait_complete() {
        if (!wait_complete_future_) {
            wait_complete_future_ = new DoneFutureState{this};
            wait_complete_future_->inc_ref();
            if (done()) {
                wait_complete_future_->set_void();
            }
        }
        return wait_complete_future_;
    }

  private:
    void on_done() noexcept {
        if (wait_complete_future_) {
            wait_complete_future_->set_void();
        }
    }

    IntrusiveDeque<TaskState<>> tasks_;
    mutable DoneFutureState* wait_complete_future_;
    EventLoop* event_loop_;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

}  // namespace detail

class TaskManager {
  public:
    TaskManager() : state_(new detail::TaskManagerState{}) { state_->inc_ref(); }
    ~TaskManager() { state_->dec_ref(); }

    void add(Task<>& task) { state_->add(task); }

    bool done() const noexcept { return state_->done(); }
    bool cancelled() const noexcept { return state_->cancelled(); }
    void result() const { state_->result(); }
    std::exception_ptr exception() const noexcept { return state_->exception(); }

    void cancel() noexcept { state_->cancel(); }
    void uncancel() noexcept { state_->uncancel(); }

    auto operator co_await() { return state_->wait_complete().operator co_await(); }

    detail::TaskManagerState* get_state() const noexcept { return state_; }
    static TaskManager from_state(detail::TaskManagerState* state) {
        return TaskManager{state};
    }

  private:
    explicit TaskManager(detail::TaskManagerState* state) : state_(state) {
        state_->inc_ref();
    }

    detail::TaskManagerState* state_;
};

}  // namespace coconext

#endif  // COCONEXT_TASK_MANAGER_HPP

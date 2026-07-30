#ifndef COCONEXT_TASK_MANAGER_HPP
#define COCONEXT_TASK_MANAGER_HPP

#include <coconext/future.hpp>
#include <coconext/intrusive_deque.hpp>
#include <coconext/task.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>

namespace coconext {

class TaskManager;

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

    Future<void> result_future() { return result_future_; }

    void child_done(TaskState<>* task) {
        task->remove_child();
        task->dec_ref();
        if (cancelled_ > 0 && tasks_.empty()) {
            on_done();
        }
    }

  private:
    void on_done() noexcept { result_future_.get_state()->set_void(); }

    IntrusiveDeque<TaskState<>> tasks_;
    Future<void> result_future_;
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

    auto operator co_await() { return state_->result_future().operator co_await(); }

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

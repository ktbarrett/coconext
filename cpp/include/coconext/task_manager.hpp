#ifndef COCONEXT_TASK_MANAGER_HPP
#define COCONEXT_TASK_MANAGER_HPP

#include "intrusive_deque.hpp"
#include <coconext/task.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>

namespace coconext {

class TaskManager;

namespace detail {

class TaskManagerState : public ManagedObject {
    friend class ::coconext::TaskManager;

  public:
    void inc_ref() noexcept override { ++ref_count_; }
    void dec_ref() noexcept override {
        if (--ref_count_ == 0) {
            delete this;
        }
    }

    ManagedObject* get_parent() noexcept override { return parent_; }
    EventLoop* get_event_loop() noexcept override { return event_loop_; }

    void add(Task<> task) {
        task.handle_->inc_ref();
        tasks_.push_back(task.handle_);
    }

    bool done() const noexcept override;
    bool cancelled() const noexcept override;
    void result() const {}
    std::exception_ptr exception() const noexcept override { return nullptr; }

    void cancel() noexcept override {
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
    void uncancel() noexcept override {
        if (done()) {
            return;
        }
        if (cancelled_ == 0) {
            throw std::runtime_error("TaskManager is not cancelled");
        }
        cancelled_--;
    }

  private:
    TaskManagerState(ManagedObject* parent, EventLoop* event_loop)
        : parent_(parent), event_loop_(event_loop) {
        inc_ref();
    }

    IntrusiveDeque<ManagedObject> tasks_;
    ManagedObject* parent_;
    EventLoop* event_loop_;
    size_t ref_count_{0};
    uint16_t cancelled_{0};
};

}  // namespace detail

class TaskManager {
  public:
    ~TaskManager() { state_->dec_ref(); }

    bool done() const noexcept { return state_->done(); }
    bool cancelled() const noexcept { return state_->cancelled(); }
    void result() const { state_->result(); }
    std::exception_ptr exception() const noexcept { return state_->exception(); }

    void cancel() noexcept { state_->cancel(); }
    void uncancel() noexcept { state_->uncancel(); }

  private:
    TaskManager(detail::ManagedObject* parent, detail::EventLoop* event_loop)
        : state_(new detail::TaskManagerState{parent, event_loop}) {}

    detail::TaskManagerState* state_;
};

}  // namespace coconext

#endif  // COCONEXT_TASK_MANAGER_HPP

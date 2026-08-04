#ifndef COCONEXT_RUN_HPP
#define COCONEXT_RUN_HPP

#include "scheduler.hpp"
#include <coconext/coro.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/outcome.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <condition_variable>

namespace coconext {

namespace detail {

template <typename T>
class RunTaskManagerState : public TaskManagerState<T> {
  public:
    void set_root(TaskState<>* root) {
        if (root_) {
            throw std::runtime_error("Root task already set");
        }
        root_ = root;
    }

  private:
    void on_add(TaskState<>*) final {}
    void on_child_done(TaskState<>* task) final {
        if (task == root_) {
            TaskManagerState<T>::close();
            for (auto& t : this->tasks_) {
                t.cancel();
            }
        }
    }
    void on_drain_complete() final {
        if (root_->exception()) {
            set_exception(root_->exception());
        }
        set_void();
    }

  private:
    TaskState<>* root_ = nullptr;
};

}  // namespace detail

template <typename T>
T run(Coro<T> coro) {
    Task<T> task = coro;
    return run(std::move(task));
}

template <typename T>
T run(Task<T> task) {
    detail::EventLoop loop;
    TaskManager<detail::RunTaskManagerState<T>> manager;
    manager.get_state()->bind(&loop, manager.get_state());
    manager.get_state()->set_root(task.get_state());
    manager.add(task);
    {
        auto handle = loop.acquire();
        handle.run();
        if (!task.done()) {
            std::condition_variable_any cv;
            task.add_done_callback([&cv]() { cv.notify_one(); });
            handle.wait(cv, [&task] { return task.done(); });
        }
    }
    return task.result();
}

}  // namespace coconext

#endif  // COCONEXT_RUN_HPP

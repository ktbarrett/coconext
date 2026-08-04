#ifndef COCONEXT_RUN_HPP
#define COCONEXT_RUN_HPP

#include <coconext/coro.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/not_null.hpp>
#include <coconext/outcome.hpp>
#include <coconext/scheduler.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <condition_variable>

namespace coconext {

namespace detail {

template <typename T>
class RunTaskManagerState final : public TaskManagerState<T> {
  public:
    RunTaskManagerState(not_null<TaskState<>*> root) noexcept : root_(root) {}

  private:
    void on_add(not_null<TaskState<>*>) noexcept final {}
    void on_child_done(not_null<TaskState<>*>) noexcept final {
        if (root_->done()) {
            TaskManagerState<T>::close();
            for (auto& t : this->tasks_) {
                t.cancel();
            }
        }
    }
    void on_drain_complete() noexcept final {
        if (root_->exception()) {
            this->set_exception(root_->exception());
        } else {
            this->set_void();
        }
    }

  private:
    not_null<TaskState<>*> root_;
};

template <typename T>
using RunTaskManager = TaskManager<RunTaskManagerState<T>>;

}  // namespace detail

template <typename T>
T run(Coro<T> coro) {
    Task<T> task = coro;
    return run(std::move(task));
}

template <typename T>
T run(Task<T> task) {
    detail::EventLoop loop;
    detail::RunTaskManager<T> manager(task.get_state());
    task.get_state()->bind_event_loop(&loop);
    task.get_state()->bind_global_task_manager(manager.get_state());
    manager.add(task.get_state());
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

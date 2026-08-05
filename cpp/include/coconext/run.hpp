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
#include <utility>

namespace coconext {

namespace detail {

template <typename T>
class RunTaskManagerState final : public TaskManagerState<T> {
  public:
    RunTaskManagerState(not_null<TaskState<T>*> root) noexcept : root_(root) {}

  private:
    void on_add(not_null<TaskState<>*>) noexcept final {}
    void on_child_done(not_null<TaskState<>*>) noexcept final {
        if (root_->done()) {
            this->close();
            for (auto& t : this->tasks_) {
                t.cancel();
            }
        }
    }
    void on_drain_complete() noexcept final {
        if (root_->exception()) {
            this->set_exception(root_->exception());
        } else {
            if constexpr (std::is_void_v<T>) {
                this->set_void();
            } else {
                this->set_result(root_->result());
            }
        }
    }

  private:
    not_null<TaskState<T>*> root_;
};

template <typename T>
class RunTaskManager : public TaskManager<RunTaskManagerState<T>> {
  public:
    RunTaskManager(detail::EventLoop* loop, Task<T> root) noexcept
        : TaskManager<RunTaskManagerState<T>>(new RunTaskManagerState<T>(root.get_state())),
          loop_(loop) {
        this->get_state()->add(root.get_state());
        this->get_state()->start_soon(loop_, this->get_state());
    }

  private:
    detail::EventLoop* loop_;
};

}  // namespace detail

template <typename T>
T run(Coro<T> coro) {
    Task<T> task = std::move(coro);
    return run(std::move(task));
}

template <typename T>
T run(Task<T> task) {
    detail::EventLoop loop;
    detail::RunTaskManager<T> manager(&loop, std::move(task));
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

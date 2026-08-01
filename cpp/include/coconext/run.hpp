#ifndef COCONEXT_RUN_HPP
#define COCONEXT_RUN_HPP

#include <coconext/coro.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/outcome.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

#include <condition_variable>

namespace coconext {

namespace detail {

// Minimal TaskManager for run(): finishes as soon as the tracked root task completes.
// close() cancels any other children; drain completes with the root's outcome.
class RunTaskManagerState : public TaskManagerState<void> {
  public:
    void set_root(TaskState<>* root) noexcept { root_ = root; }

  protected:
    void on_child_done(TaskState<>* task) override {
        if (task == root_) {
            close();
        }
    }
    Outcome<void> on_drain_complete() override {
        if (root_->exception()) {
            return Outcome<void>{root_->exception()};
        }
        return Outcome<void>{};
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
    TaskManager<detail::RunTaskManagerState> manager;
    manager.get_state()->bind_event_loop(&loop);
    manager.get_state()->set_root(task.get_state());
    manager.add(task);
    {
        auto handle = loop.acquire();
        handle.run();
        if (!task.done()) {
            std::condition_variable_any cv;
            task.add_done_callback([&cv](Task<T> const&) { cv.notify_one(); });
            handle.wait(cv, [&task] { return task.done(); });
        }
    }
    return task.result();
}

}  // namespace coconext

#endif  // COCONEXT_RUN_HPP

#ifndef COCONEXT_RUN_HPP
#define COCONEXT_RUN_HPP

#include <coconext/coro.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/not_null.hpp>
#include <coconext/task_manager.hpp>

#include <condition_variable>
#include <exception>
#include <utility>

namespace coconext {

namespace detail {

template <typename T>
class RunTaskManager final : public TaskManager {
  public:
    RunTaskManager(
        not_null<detail::EventLoop*> loop, std::condition_variable_any& done, Coro<T> root
    )
        : done_(done), root_(std::move(root)) {
        this->start_internal(TaskContext{loop, this, nullptr});
        this->add_and_start(root_.get_state());
    }

    [[nodiscard]] T result() const {
        this->completion_.result();
        return root_.result();
    }

  private:
    void on_child_done(not_null<TaskState<>*> task) noexcept final {
        if (!this->closed() && task->exception()) {
            this->set_exception(task->exception());
            this->cancel();
            return;
        }

        if (task.get() == root_.get_state().get()) {
            // The root defines the lifetime of run(). Successful fire-and-forget
            // siblings are still torn down once it returns.
            this->cancel();
        }
    }

    void on_done() noexcept final { done_.notify_one(); }

    std::condition_variable_any& done_;
    Task<T> root_;
};

}  // namespace detail

template <typename T>
T run(Coro<T> coro) {
    detail::EventLoop loop;
    std::condition_variable_any done;
    detail::RunTaskManager<T> manager{&loop, done, std::move(coro)};

    {
        auto handle = loop.acquire();
        while (!manager.done()) {
            handle.run();
            if (!manager.done()) {
                handle.wait(done, [&manager] { return manager.done(); });
            }
        }
    }

    return manager.result();
}

}  // namespace coconext

#endif  // COCONEXT_RUN_HPP

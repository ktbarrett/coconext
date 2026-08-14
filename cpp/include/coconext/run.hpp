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
        this->start_internal(TaskContext{loop, this});
        this->add_and_start(root_.get_state());
    }

    [[nodiscard]] bool finished() const noexcept { return this->done(); }

    void finish() {
        this->finish_join();
        if (first_exception_) {
            std::rethrow_exception(first_exception_);
        }
    }

    [[nodiscard]] T result() const { return root_.result(); }

  private:
    void on_child_done(not_null<TaskState<>*> task) noexcept final {
        if (!this->closed() && task->exception()) {
            first_exception_ = task->exception();
            this->set_exception(first_exception_);
            cancel_remaining();
            return;
        }

        if (task.get() == root_.get_state().get()) {
            // The root defines the lifetime of run(). Successful fire-and-forget
            // siblings are still torn down once it returns.
            cancel_remaining();
        }
    }

    void on_done() noexcept final { done_.notify_one(); }

    void cancel_remaining() noexcept {
        try {
            this->cancel();
        } catch (...) {
            if (!first_exception_) {
                first_exception_ = std::current_exception();
                this->set_exception(first_exception_);
            }
            this->close();
        }
    }

    std::condition_variable_any& done_;
    Task<T> root_;
    std::exception_ptr first_exception_;
};

}  // namespace detail

template <typename T>
T run(Coro<T> coro) {
    detail::EventLoop loop;
    std::condition_variable_any done;
    detail::RunTaskManager<T> manager{&loop, done, std::move(coro)};

    {
        auto handle = loop.acquire();
        while (!manager.finished()) {
            handle.run();
            if (!manager.finished()) {
                handle.wait(done, [&manager] { return manager.finished(); });
            }
        }
    }

    manager.finish();
    return manager.result();
}

}  // namespace coconext

#endif  // COCONEXT_RUN_HPP

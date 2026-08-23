#ifndef COCONEXT_TASK_MANAGER_HPP
#define COCONEXT_TASK_MANAGER_HPP

#include <coconext/coro.hpp>
#include <coconext/future.hpp>
#include <coconext/task.hpp>

#include <cassert>
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

namespace coconext {

namespace detail {

template <typename T>
class RunTaskManager;

}  // namespace detail

class TaskManager {
    template <typename>
    friend class detail::RunTaskManager;
    friend class TaskState<detail::Erased>;

    enum class State {
        Created,
        Open,
        Closed,
        Done
    };

  public:
    class StartAwaiter {
        friend class TaskManager;

      public:
        using coconext_awaiter = void;

        [[nodiscard]] bool await_ready() const noexcept { return false; }

        template <typename PromiseType>
        bool await_suspend(std::coroutine_handle<PromiseType> parent) {
            manager_.start_internal(parent.promise().get_context());
            return false;
        }

        void await_resume() const noexcept {}

      private:
        explicit StartAwaiter(TaskManager& manager) noexcept : manager_(manager) {}

        TaskManager& manager_;
    };

    TaskManager() noexcept = default;
    TaskManager(TaskManager const&) = delete;
    TaskManager& operator=(TaskManager const&) = delete;
    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    virtual ~TaskManager() noexcept(false) {
        if (state_ != State::Created && state_ != State::Done) {
            abandon_children();
            throw std::logic_error("TaskManager destroyed before it completed");
        }
    }

    [[nodiscard]] StartAwaiter start() & {
        if (state_ != State::Created) {
            throw std::logic_error("TaskManager is already started");
        }
        return StartAwaiter{*this};
    }

    [[nodiscard]] Coro<void> join() &;

    template <CoconextAwaitable A>
    [[nodiscard]] Task<await_result_t<A>> start_soon(A awaitable) {
        using Result = await_result_t<A>;
        Task<Result> task{Task<Result>::wrap_impl(std::move(awaitable))};
        add_and_start(task.get_state());
        return task;
    }

    template <typename T>
    [[nodiscard]] Task<T> start_soon(Task<T>& task) {
        add_and_start(task.get_state());
        return task;
    }

    template <typename T>
    [[nodiscard]] Task<T> start_soon(Task<T>&& task) {
        add_and_start(task.get_state());
        return task;
    }

    void close() {
        if (state_ == State::Created) {
            throw std::logic_error("Cannot close an unstarted TaskManager");
        }
        if (state_ != State::Open) {
            return;
        }
        state_ = State::Closed;
        complete_if_ready();
    }

    void cancel() {
        if (cancelling_) {
            return;
        }
        cancelling_ = true;
        close();
        for (auto it = tasks_.begin(); it != tasks_.end();) {
            auto& task = *it++;
            task.cancel();
        }
        cancelling_ = false;
    }

    [[nodiscard]] bool started() const noexcept { return state_ != State::Created; }
    [[nodiscard]] bool closed() const noexcept { return state_ == State::Closed || done(); }
    [[nodiscard]] bool done() const noexcept { return state_ == State::Done; }
    [[nodiscard]] bool empty() const noexcept { return tasks_.empty(); }

  protected:
    // Called after a child is linked and scheduled.
    virtual void on_add(not_null<TaskState<>*>) noexcept {}

    // Called after a completed child is unlinked. The default policy closes once no
    // child remains, but specialized managers can close or cancel earlier.
    virtual void on_child_done(not_null<TaskState<>*>) noexcept {
        if (empty()) {
            close();
        }
    }

    // Called exactly once when a closed manager finishes draining.
    virtual void on_done() noexcept {}

    void set_exception(std::exception_ptr exc) noexcept {
        assert(exc);
        if (!exception_) {
            exception_ = exc;
        }
    }

  private:
    void start_internal(TaskContext context) {
        if (state_ != State::Created) {
            throw std::logic_error("TaskManager is already started");
        }
        context_ = context;
        state_ = State::Open;
    }

    void add_and_start(not_null<TaskState<>*> task) {
        if (state_ != State::Open) {
            throw std::logic_error("Cannot start a Task on a non-open TaskManager");
        }
        if (task->done()) {
            throw std::logic_error("Cannot adopt a completed Task");
        }
        if (task->task_manager_ != nullptr) {
            throw std::logic_error("Task is already managed by another TaskManager");
        }

        task->task_manager_ = this;
        task->inc_ref();
        tasks_.push_back(task);
        if (!task->started()) {
            task->start_soon(*context_);
        }
        on_add(task);
    }

    void child_done(not_null<TaskState<>*> task) noexcept {
        task->deque_remove();
        task->task_manager_ = nullptr;
        on_child_done(task);
        complete_if_ready();
        task->dec_ref();
    }

    void complete_if_ready() noexcept {
        if (state_ != State::Closed || !tasks_.empty()) {
            return;
        }
        state_ = State::Done;
        on_done();
        if (exception_) {
            completion_.set_exception(exception_);
        } else {
            completion_.set_void();
        }
    }

    void abandon_children() noexcept {
        while (auto task = tasks_.pop_front()) {
            task->task_manager_ = nullptr;
            task->cancel();
            task->dec_ref();
        }
    }

  private:
    detail::IntrusiveDeque<TaskState<>> tasks_;
    Future<void> completion_;
    std::optional<TaskContext> context_;
    std::exception_ptr exception_;
    State state_ = State::Created;
    bool cancelling_ = false;
};

inline Coro<void> TaskManager::join() & {
    if (state_ == State::Created) {
        throw std::logic_error("Cannot join an unstarted TaskManager");
    }

    auto context = co_await get_context();
    if (context.get_task() != context_->get_task()) {
        throw std::logic_error(
            "TaskManager can only be joined by the Task that started it"
        );
    }
    auto task = not_null{context.get_task()};
    bool cancelled = false;
    std::exception_ptr completion_exception;

    while (true) {
        try {
            co_await completion_;
            break;
        } catch (CancelledError const&) {
            if (!task->cancelling()) {
                completion_exception = std::current_exception();
                break;
            }
            cancel();
            cancelled = true;
            task->uncancel();
        } catch (...) {
            completion_exception = std::current_exception();
            break;
        }
    }

    if (cancelled) {
        task->cancel();
    }
    if (completion_exception) {
        std::rethrow_exception(completion_exception);
    }
}

template <CoconextAwaitable A>
[[nodiscard]] Task<await_result_t<A>> start_soon(A awaitable) {
    auto global_task_manager = lookup_context().get_global_task_manager();
    if (global_task_manager == nullptr) {
        throw std::runtime_error("No global TaskManager");
    }
    return global_task_manager->start_soon(std::move(awaitable));
}

template <typename T>
[[nodiscard]] Task<T> start_soon(Task<T>& task) {
    auto global_task_manager = lookup_context().get_global_task_manager();
    if (global_task_manager == nullptr) {
        throw std::runtime_error("No global TaskManager");
    }
    return global_task_manager->start_soon(task);
}

template <typename T>
[[nodiscard]] Task<T> start_soon(Task<T>&& task) {
    auto global_task_manager = lookup_context().get_global_task_manager();
    if (global_task_manager == nullptr) {
        throw std::runtime_error("No global TaskManager");
    }
    return global_task_manager->start_soon(std::move(task));
}

inline void TaskState<>::on_done(Outcome outcome) noexcept {
    state_ = Done{std::move(outcome)};
    for (auto& callback : callbacks_) {
        callback();
    }
    if (!waiters_.empty()) {
        assert(event_loop_ != nullptr);
        event_loop_->acquire().schedule_all_back(std::move(waiters_));
    }
    if (task_manager_ != nullptr) {
        task_manager_->child_done(this);
    }
}

}  // namespace coconext

#endif  // COCONEXT_TASK_MANAGER_HPP

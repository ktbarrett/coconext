#ifndef COCONEXT_TASK_IMPL_HPP
#define COCONEXT_TASK_IMPL_HPP

#include <coconext/future.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>

namespace coconext {

template <typename T>
detail::TaskStateBase<T>::~TaskStateBase() {
    if (wait_complete_future_) {
        wait_complete_future_->dec_ref();
    }
}

template <typename T>
void detail::TaskStateBase<T>::on_done() {
    auto task = Task<T>::from_state(static_cast<TaskState<T>*>(this));
    for (auto& callback : callbacks_) {
        callback(task);
    }
    task_manager_->child_done(static_cast<TaskState<T>*>(this));
    if (wait_complete_future_) {
        wait_complete_future_->set_void();
    }
}

template <typename T>
Future<void> detail::TaskStateBase<T>::wait_complete() const noexcept {
    if (!wait_complete_future_) {
        wait_complete_future_ = new FutureState<void>{};
        wait_complete_future_->inc_ref();
        if (done()) {
            wait_complete_future_->set_void();
        }
    }
    return Future<void>::from_state(wait_complete_future_);
}

template <typename T>
Future<void> Task<T>::wait_complete() const noexcept {
    return handle_->wait_complete();
}

template <typename T>
void detail::TaskStateBase<T>::start_soon(detail::TaskManagerState* tm) {
    if (!unstarted()) {
        throw std::runtime_error("Task already started");
    }
    bind_event_loop(tm->get_event_loop());
    state_ = Scheduled{*this};
    event_loop_->acquire().schedule_back(&std::get<Scheduled>(state_));
}

template <typename T>
void Task<T>::start_soon(TaskManager& loop) {
    handle_->start_soon(loop.get_state());
}

}  // namespace coconext

#endif  // COCONEXT_TASK_IMPL_HPP

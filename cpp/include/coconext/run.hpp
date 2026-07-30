#ifndef COCONEXT_RUN_HPP
#define COCONEXT_RUN_HPP

#include <coconext/coro.hpp>
#include <coconext/event_loop.hpp>
#include <coconext/task.hpp>
#include <coconext/task_manager.hpp>
#include <condition_variable>

namespace coconext {

template <typename T>
T run(Coro<T> coro) {
    Task<T> task = coro;
    return run(std::move(task));
}

template <typename T>
T run(Task<T> task) {
    detail::EventLoop loop;
    TaskManager manager;
    manager.get_state()->bind_event_loop(&loop);
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

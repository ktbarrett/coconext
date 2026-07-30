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
    detail::EventLoop loop;
    TaskManager manager{&loop};
    Task<T> task = coro;
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

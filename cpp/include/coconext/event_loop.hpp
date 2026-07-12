#ifndef COCONEXT_EVENT_LOOP_HPP
#define COCONEXT_EVENT_LOOP_HPP

#include <coconext/event_deque.hpp>
#include <mutex>

namespace coconext::event_loop {

// An external source of events drives the loop by acquiring a handle, scheduling callbacks,
// and running the loop until exhaustion before releasing ownership to allow another
// external event to be handled. The EventLoop does not own the entries; it merely manages
// their scheduling and invocation. Task lifetime is managed by TaskManagers.
class EventLoop {
  public:
    class Handle {
        friend class EventLoop;

        Handle(EventLoop& loop, std::unique_lock<std::mutex>&& lock)
            : loop(loop), lock(std::move(lock)) {}

      public:
        void run() {
            while (!loop.queue_.empty()) {
                auto entry = loop.queue_.pop_front();
                entry->coro_handle.resume();
            }
        }

      private:
        EventLoop& loop;
        std::unique_lock<std::mutex> lock;
    };

  public:
    [[nodiscard]] Handle acquire() {
        return Handle(*this, std::unique_lock<std::mutex>(mtx_));
    }

  private:
    Cmarqueue queue_;
    std::mutex mtx_;
};

}  // namespace coconext::event_loop

#endif  // COCONEXT_EVENT_LOOP_HPP

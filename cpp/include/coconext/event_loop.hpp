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

        Handle(EventLoop& loop) : loop_(loop) {}
        Handle(Handle&& other) = default;
        ~Handle();

      public:
        void run() {
            while (!loop_.queue_.empty()) {
                auto event = loop_.queue_.pop_front();
                event->coro_handle.resume();
            }
        }

        template <typename DequeT>
        void schedule_all_back(DequeT&& deque) {
            loop_.queue_.extend_back(std::forward<DequeT>(deque));
        }

      private:
        EventLoop& loop_;
    };
    friend class Handle;

  public:
    [[nodiscard]] Handle acquire();

  private:
    Cmarqueue queue_;
    std::mutex mtx_;
};

EventLoop::Handle& get_current_event_loop();

}  // namespace coconext::event_loop

#endif  // COCONEXT_EVENT_LOOP_HPP

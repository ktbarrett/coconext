#ifndef COCONEXT_EVENT_LOOP_HPP
#define COCONEXT_EVENT_LOOP_HPP

#include <coconext/cmarqueue.hpp>
#include <coroutine>
#include <mutex>

namespace coconext::event_loop {

// An external source of events drives the loop by acquiring a handle, scheduling callbacks,
// and running the loop until exhaustion before releasing ownership to allow another
// external event to be handled. The EventLoop does not own the entries; it merely manages
// their scheduling and invocation. Task lifetime is managed by TaskManagers.
class EventLoop {
  public:
    class Event {
      public:
        Event* prev = nullptr;
        Event* next = nullptr;
        // Consider making this a virtual run() function to support things other than
        // coroutines.
        std::coroutine_handle<> coro_handle = nullptr;

        Event() noexcept = default;

        void remove() noexcept {
            prev->next = next;
            next->prev = prev;
        }
    };

    class ExternalHandle {
        friend class EventLoop;

        ExternalHandle(EventLoop& loop) : loop_(loop) {}
        ExternalHandle(ExternalHandle&& other) = default;
        ~ExternalHandle() { loop_.mtx_.unlock(); }

      public:
        void run() {
            while (!loop_.queue_.empty()) {
                auto event = loop_.queue_.pop_front();
                event->coro_handle.resume();
            }
        }

      private:
        EventLoop& loop_;
    };
    friend class ExternalHandle;

    class InternalHandle {
        friend class EventLoop;

        InternalHandle(EventLoop& loop) : loop_(loop) {}
        InternalHandle(InternalHandle&& other) = default;

      public:
        template <typename DequeT>
        void schedule_all_back(DequeT&& deque) {
            loop_.queue_.extend_back(std::forward<DequeT>(deque));
        }

      private:
        EventLoop& loop_;
    };

  public:
    [[nodiscard]] ExternalHandle acquire() {
        mtx_.lock();
        return ExternalHandle(*this);
    }

  private:
    coconext::detail::Cmarqueue<Event> queue_;
    std::mutex mtx_;
};

}  // namespace coconext::event_loop

#endif  // COCONEXT_EVENT_LOOP_HPP

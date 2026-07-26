#ifndef COCONEXT_EVENT_LOOP_HPP
#define COCONEXT_EVENT_LOOP_HPP

#include <cassert>
#include <coconext/intrusive_deque.hpp>
#include <mutex>
#include <stdexcept>

namespace coconext {

class EventLoop;

namespace detail {

template <typename DequeT>
void schedule_all_back(EventLoop& loop, DequeT&& deque);

}  // namespace detail

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
        Event() noexcept = default;

        virtual void event_run() { throw std::runtime_error("Event run not implemented"); }

      protected:
        void event_unschedule() noexcept {
            assert(prev != nullptr);
            prev->next = next;
            next->prev = prev;
#ifndef NDEBUG
            prev = nullptr;
#endif
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
                event->event_run();
            }
        }

      private:
        EventLoop& loop_;
    };
    friend class ExternalHandle;

    [[nodiscard]] ExternalHandle acquire_external() {
        mtx_.lock();
        return ExternalHandle(*this);
    }

  private:
    template <typename DequeT>
    friend void detail::schedule_all_back(EventLoop& loop, DequeT&& deque);

  private:
    coconext::detail::IntrusiveDeque<Event> queue_;
    std::mutex mtx_;
};

namespace detail {

template <typename DequeT>
void schedule_all_back(EventLoop& loop, DequeT&& deque) {
    loop.queue_.extend_back(std::forward<DequeT>(deque));
}

}  // namespace detail

}  // namespace coconext

#endif  // COCONEXT_EVENT_LOOP_HPP

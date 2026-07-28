#ifndef COCONEXT_EVENT_LOOP_HPP
#define COCONEXT_EVENT_LOOP_HPP

#include <cassert>
#include <coconext/intrusive_deque.hpp>
#include <mutex>
#include <stdexcept>

namespace coconext::detail {

class Event {
  public:
    Event* prev = nullptr;
    Event* next = nullptr;
    Event() noexcept = default;

    virtual void event_run() { throw std::runtime_error("Event run not implemented"); }

  private:
    void event_unschedule() noexcept {
        assert(prev != nullptr);
        prev->next = next;
        next->prev = prev;
#ifndef NDEBUG
        prev = nullptr;
#endif
    }
};

// An external source of events drives the loop by acquiring a handle, scheduling callbacks,
// and running the loop until exhaustion before releasing ownership to allow another
// external event to be handled. The EventLoop does not own the entries; it merely manages
// their scheduling and invocation. Task lifetime is managed by TaskManagers.
class EventLoop {
  public:
    class Handle {
        friend class detail::EventLoop;

      public:
        Handle(Handle&& other) = default;
        ~Handle() {
            run();
            loop_.mtx_.unlock();
        }

        void schedule(Event* event) { loop_.queue_.push_back(event); }

        template <typename DequeT>
        void schedule_all_back(DequeT&& deque) {
            loop_.queue_.extend_back(std::forward<DequeT>(deque));
        }

        void run() {
            if (loop_.is_running_) {
                return;
            }
            loop_.is_running_ = true;
            while (!loop_.queue_.empty()) {
                auto event = loop_.queue_.pop_front();
                event->event_run();
            }
            loop_.is_running_ = false;
        }

      private:
        Handle(detail::EventLoop& loop) : loop_(loop) {}

        detail::EventLoop& loop_;
    };
    friend class Handle;

    [[nodiscard]] Handle acquire() {
        mtx_.lock();
        return Handle(*this);
    }

  private:
    coconext::detail::IntrusiveDeque<Event> queue_;
    std::recursive_mutex mtx_;
    bool is_running_ = false;
};

}  // namespace coconext::detail

#endif  // COCONEXT_EVENT_LOOP_HPP

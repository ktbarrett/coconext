#ifndef COCONEXT_EVENT_LOOP_HPP
#define COCONEXT_EVENT_LOOP_HPP

#include <cassert>
#include <coconext/intrusive_deque.hpp>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

namespace coconext::detail {

class EventLoop;

class Event : public IntrusiveDequeNode {
    friend class EventLoop;

  public:
    void event_unschedule() noexcept { deque_remove(); }

  private:
    virtual void event_run() noexcept = 0;
    using IntrusiveDequeNode::deque_remove;
};

class EventLoop {
    // An external source of events drives the loop by acquiring a handle, scheduling
    // callbacks, and running the loop until exhaustion before releasing ownership to allow
    // another external event to be handled. The EventLoop does not own the entries; it
    // merely manages their scheduling and invocation. Task lifetime is managed by
    // TaskManagers.
    //
    // The lock acquisition uses a recursive mutex so multiple external users see
    // exclusivity, but the events in the event loop can schedule other events, making lock
    // acquiisition recursive. You might ask why we don't just use a non-recursive mutex and
    // have Futures and other schedulable things know whether they are recursively scheduled
    // or not? While this is generally true, and is true in cocotb currently, this buys us 2
    // things:
    //
    //   1. There is a single `fire()` in Future so that subclasses don't need to worry
    //      about whether they are recursively scheduled or not. Nor do we have to worry
    //      about exposing more API to service this.
    //   2. It is possible there are Futures that might be scheduled sometimes recursively
    //      and sometimes not. For example, a Future is set in a DPI import function.
    //      Sometimes this function is called directly from the RTL (external event), other
    //      times a cocotb function calls into a DPI export function, which then calls this
    //      function (recursive).
    //
    // Recursive mutexes are not free, but they are not expensive either. Likely implemented
    // as an atomic compare of thread_ids.
    //
    // The is_running_ flag is used to prevent recursive calls to run() from happening.

  public:
    class Handle {
        friend class detail::EventLoop;

      public:
        Handle(Handle&& other) noexcept = default;
        ~Handle() noexcept {
            // Force the loop to run any remaining events before releasing the lock. We
            // don't want to leave any events unprocessed for the next external event to
            // handle.
            if (!loop_.is_running_) {
                run_();
            }
            loop_.mtx_.unlock();
        }

        void schedule_back(Event* event) noexcept { loop_.queue_.push_back(event); }

        template <typename DequeT>
        void schedule_all_back(DequeT&& deque) noexcept {
            loop_.queue_.extend_back(std::forward<DequeT>(deque));
        }

        void run() {
            // Recursive calls to run() should never happen.
            if (loop_.is_running_) {
                throw std::runtime_error("EventLoop is already running");
            }
            run_();
        }

        template <typename Predicate>
        void wait(std::condition_variable_any& cv, Predicate pred) {
            cv.wait(loop_.mtx_, std::move(pred));
        }

      private:
        void run_() {
            loop_.is_running_ = true;
            while (!loop_.queue_.empty()) {
                auto event = loop_.queue_.pop_front();
                event->event_run();
            }
            loop_.is_running_ = false;
        }

        Handle(detail::EventLoop& loop) : loop_(loop) {}

        detail::EventLoop& loop_;
    };
    friend class Handle;

    [[nodiscard]] Handle acquire() {
        mtx_.lock();
        return Handle(*this);
    }

  private:
    detail::IntrusiveDeque<Event> queue_;
    std::recursive_mutex mtx_;
    bool is_running_ = false;
};

}  // namespace coconext::detail

#endif  // COCONEXT_EVENT_LOOP_HPP

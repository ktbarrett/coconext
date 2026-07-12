#ifndef COCONEXT_SCHEDULER_HPP
#define COCONEXT_SCHEDULER_HPP

#include <coconext/deque.hpp>
#include <mutex>

namespace coconext::scheduler {

// The EventLoop is a deque of callbacks. An external source of events drives the loop by
// acquiring a handle, scheduling callbacks, and running the loop until exhaustion before
// releasing ownership to allow another external event to be handled.
class EventLoop {
  private:
    struct Entry;
    Entry* head = nullptr;
    Entry* tail = nullptr;
    Entry* free_list = nullptr;

    class Entry {
        friend class EventLoop;
        // Consider using tombstoning instead of removing nodes immediately. This shrinks
        // each Node down to the two pointers, remove() is just setting the callback to
        // nullptr.
        Entry* next;
        Entry* prev;
        EventLoop& loop;
        void (*cb_func)(void*);
        void* cb_data;

      public:
        Entry(EventLoop& loop) : loop(loop) {}

        void remove() {
            if (prev) {
                prev->next = next;
            }
            if (next) {
                next->prev = prev;
            }
            next = loop.free_list;
            loop.free_list = this;
        }
    };

    std::mutex mtx;

  public:
    class Handle {
        friend class EventLoop;

        EventLoop& loop;
        std::unique_lock<std::mutex> lock;

        Handle(EventLoop& loop, std::unique_lock<std::mutex>&& lock)
            : loop(loop), lock(std::move(lock)) {}

      public:
        void run() {
            while (loop.head) {
                Entry* entry = loop.head;
                // pop the head of the linked list.
                loop.head = loop.head->next;
                if (loop.head) {
                    loop.head->prev = nullptr;
                } else {
                    loop.tail = nullptr;
                }
                // place the entry back into the free list.
                entry->next = loop.free_list;
                loop.free_list = entry;
                // invoke the callback function.
                entry->cb_func(entry->cb_data);
            }
        }
        [[nodiscard]] Entry* schedule(void (*cb_func)(void*), void* cb_data) {
            // Allocate Entry, try the free list first.
            Entry* entry;
            if (loop.free_list) {
                entry = loop.free_list;
                loop.free_list = loop.free_list->next;
            } else {
                entry = new Entry(loop);
            }

            // Append entry to the end of the linked list.
            entry->cb_func = cb_func;
            entry->cb_data = cb_data;
            entry->next = nullptr;
            entry->prev = loop.tail;
            if (loop.tail) {
                loop.tail->next = entry;
            }
            loop.tail = entry;
            if (!loop.head) {
                loop.head = entry;
            }
            return entry;
        }

        [[nodiscard]] Entry* schedule_left(void (*cb_func)(void*), void* cb_data) {
            // Allocate Entry, try the free list first.
            Entry* entry;
            if (loop.free_list) {
                entry = loop.free_list;
                loop.free_list = loop.free_list->next;
            } else {
                entry = new Entry(loop);
            }

            // Append entry to the beginning of the linked list.
            entry->cb_func = cb_func;
            entry->cb_data = cb_data;
            entry->prev = nullptr;
            entry->next = loop.head;
            if (loop.head) {
                loop.head->prev = entry;
            }
            loop.head = entry;
            if (!loop.tail) {
                loop.tail = entry;
            }
            return entry;
        }
    };

  public:
    [[nodiscard]] Handle acquire() {
        return Handle(*this, std::unique_lock<std::mutex>(mtx));
    }
};

}  // namespace coconext::scheduler

#endif  // COCONEXT_SCHEDULER_HPP

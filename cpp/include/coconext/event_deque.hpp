#ifndef COCONEXT_EVENT_DEQUE_HPP
#define COCONEXT_EVENT_DEQUE_HPP

#include <coroutine>

// This is a special deque implementation seen in both the EventLoop and Futures in this
// library. This deque does not own the nodes it contains (beyond the default-constructed
// anchors), so it is unconcerned with object lifetimes. Nodes are inserted into the deque
// and by nature of their structure, can be removed anonymously in O(1). Node is a base
// class for all types that wish to be added to the EventDeque.

namespace coconext::event_loop {

class Event {
    friend class Cmarqueue;
    Event* prev = nullptr;
    Event* next = nullptr;

  public:
    // Consider making this a virtual run() function to support things other than
    // coroutines.
    std::coroutine_handle<> coro_handle = nullptr;

    void remove() noexcept {
        prev->next = next;
        next->prev = prev;
    }
};

class Cmarqueue {

  public:
    Cmarqueue() noexcept {
        head.next = &tail;
        tail.prev = &head;
    }
    // EventDeque doesn't own anything, so there's nothing to clean up.

  public:
    template <typename NodeType, bool Forward>
    class Iterator {
        NodeType* current;

      public:
        explicit Iterator(NodeType* node) : current(node) {}
        NodeType& operator*() const { return *current; }
        NodeType* operator->() const { return current; }
        Iterator& operator++() {
            if constexpr (Forward) {
                current = current->next;
            } else {
                current = current->prev;
            }
            return *this;
        }
        bool operator!=(Iterator const& other) const { return current != other.current; }
    };

    using iterator = Iterator<Event, true>;
    using const_iterator = Iterator<Event const, true>;
    using reverse_iterator = Iterator<Event, false>;
    using const_reverse_iterator = Iterator<Event const, false>;

    iterator begin() noexcept { return Iterator<Event, true>(front()); }
    iterator end() noexcept { return Iterator<Event, true>(&tail); }
    const_iterator begin() const noexcept { return Iterator<Event const, true>(front()); }
    const_iterator end() const noexcept { return Iterator<Event const, true>(&tail); }
    reverse_iterator rbegin() noexcept { return Iterator<Event, false>(back()); }
    reverse_iterator rend() noexcept { return Iterator<Event, false>(&head); }
    const_reverse_iterator rbegin() const noexcept {
        return Iterator<Event const, false>(back());
    }
    const_reverse_iterator rend() const noexcept {
        return Iterator<Event const, false>(&head);
    }

  public:
    Event* front() const noexcept { return head.next == &tail ? nullptr : head.next; }
    Event* back() const noexcept { return tail.prev == &head ? nullptr : tail.prev; }
    bool empty() const noexcept { return head.next == &tail; }
    Event* push_back(Event* node) noexcept {
        back()->next = node;
        node->prev = back();
        set_back(node);
        return node;
    }
    Event* push_front(Event* node) noexcept {
        front()->prev = node;
        node->next = front();
        node->prev = &head;
        set_front(node);
        return node;
    }
    Event* pop_back() {
        if (empty()) {
            return nullptr;
        }
        Event* node = back();
        node->remove();
        return node;
    }
    Event* pop_front() {
        if (empty()) {
            return nullptr;
        }
        Event* node = front();
        node->remove();
        return node;
    }
    void steal_extend_back(Cmarqueue& other) noexcept {
        if (other.empty()) {
            return;
        }
        back()->next = other.front();
        other.front()->prev = back();
        set_back(other.back());
        other.clear();
    }
    void steal_extend_front(Cmarqueue& other) noexcept {
        if (other.empty()) {
            return;
        }
        front()->prev = other.back();
        other.back()->next = front();
        set_front(other.front());
        other.clear();
    }
    void clear() noexcept {
        head.next = &tail;
        tail.prev = &head;
    }

  private:
    void set_back(Event* node) noexcept { tail.prev = node; }
    void set_front(Event* node) noexcept { head.next = node; }
    Event head;
    Event tail;
};

}  // namespace coconext::event_loop

#endif  // COCONEXT_EVENT_DEQUE_HPP

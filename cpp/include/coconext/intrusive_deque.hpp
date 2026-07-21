#ifndef COCONEXT_INTRUSIVE_DEQUE_HPP
#define COCONEXT_INTRUSIVE_DEQUE_HPP

// This is a special deque implementation seen in the EventLoop, Futures, TaskManagers, and
// more. This deque does not own the nodes it contains (beyond the default-constructed
// anchors), so it is unconcerned with object lifetimes. Nodes are inserted into the deque
// and by nature of their structure, can be removed anonymously in O(1). Node is a base
// class for all types that wish to be added to the EventDeque.

#include <concepts>
#include <type_traits>
#include <utility>

namespace coconext::detail {

template <typename EntryT>
concept CmarqueueEntry = requires(EntryT* e) {
    requires std::same_as<decltype(e->prev), EntryT*>;
    requires std::same_as<decltype(e->next), EntryT*>;
} && std::is_default_constructible_v<EntryT> && std::is_destructible_v<EntryT>;

template <CmarqueueEntry EntryT>
class IntrusiveDeque {
  public:
    IntrusiveDeque() noexcept {
        head.next = &tail;
        tail.prev = &head;
    }
    // IntrusiveDeque doesn't own anything, so there's nothing to clean up.
    IntrusiveDeque(IntrusiveDeque&& other)
        : head(std::move(other.head)), tail(std::move(other.tail)) {
        other.clear();
    }
    IntrusiveDeque& operator=(IntrusiveDeque&& other) noexcept {
        if (this != &other) {
            head = std::move(other.head);
            tail = std::move(other.tail);
            other.clear();
        }
        return *this;
    }
    // Not copyable, entries are non-owning so they must be unique.
    IntrusiveDeque(IntrusiveDeque const&) = delete;
    IntrusiveDeque& operator=(IntrusiveDeque const&) = delete;

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

    using iterator = Iterator<EntryT, true>;
    using const_iterator = Iterator<EntryT const, true>;
    using reverse_iterator = Iterator<EntryT, false>;
    using const_reverse_iterator = Iterator<EntryT const, false>;

    iterator begin() noexcept { return Iterator<EntryT, true>(front()); }
    iterator end() noexcept { return Iterator<EntryT, true>(&tail); }
    const_iterator begin() const noexcept { return Iterator<EntryT const, true>(front()); }
    const_iterator end() const noexcept { return Iterator<EntryT const, true>(&tail); }
    reverse_iterator rbegin() noexcept { return Iterator<EntryT, false>(back()); }
    reverse_iterator rend() noexcept { return Iterator<EntryT, false>(&head); }
    const_reverse_iterator rbegin() const noexcept {
        return Iterator<EntryT const, false>(back());
    }
    const_reverse_iterator rend() const noexcept {
        return Iterator<EntryT const, false>(&head);
    }

  public:
    EntryT* front() const noexcept { return head.next == &tail ? nullptr : head.next; }
    EntryT* back() const noexcept { return tail.prev == &head ? nullptr : tail.prev; }
    bool empty() const noexcept { return head.next == &tail; }
    EntryT* push_back(EntryT* node) noexcept {
        back()->next = node;
        node->prev = back();
        set_back(node);
        return node;
    }
    EntryT* push_front(EntryT* node) noexcept {
        front()->prev = node;
        node->next = front();
        node->prev = &head;
        set_front(node);
        return node;
    }
    EntryT* pop_back() {
        if (empty()) {
            return nullptr;
        }
        EntryT* node = back();
        node->remove();
        return node;
    }
    EntryT* pop_front() {
        if (empty()) {
            return nullptr;
        }
        EntryT* node = front();
        node->remove();
        return node;
    }
    void extend_back(IntrusiveDeque<EntryT>&& other) noexcept {
        if (other.empty()) {
            return;
        }
        back()->next = other.front();
        other.front()->prev = back();
        set_back(other.back());
        other.clear();
    }
    void extend_front(IntrusiveDeque<EntryT>&& other) noexcept {
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
    void set_back(EntryT* node) noexcept { tail.prev = node; }
    void set_front(EntryT* node) noexcept { head.next = node; }
    EntryT head;
    EntryT tail;
};

}  // namespace coconext::detail

#endif  // COCONEXT_INTRUSIVE_DEQUE_HPP

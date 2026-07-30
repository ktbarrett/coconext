#ifndef COCONEXT_INTRUSIVE_DEQUE_HPP
#define COCONEXT_INTRUSIVE_DEQUE_HPP

// This is a special deque implementation seen in the EventLoop, Futures, TaskManagers, and
// more. This deque does not own the nodes it contains (beyond the default-constructed
// anchors), so it is unconcerned with object lifetimes. Nodes are inserted into the deque
// and by nature of their structure, can be removed anonymously in O(1). Node is a base
// class for all types that wish to be added to an IntrusiveDeque.

#include <iterator>
#include <type_traits>

namespace coconext::detail {

template <typename EntryT>
class IntrusiveDeque;

class IntrusiveDequeNode {
    template <typename>
    friend class IntrusiveDeque;

  protected:
    void deque_remove() noexcept {
        if (prev == nullptr) {
            return;
        }
        prev->next = next;
        next->prev = prev;
        prev = nullptr;
    }

  private:
    IntrusiveDequeNode* prev = nullptr;
    IntrusiveDequeNode* next = nullptr;
};

template <typename EntryT>
class IntrusiveDeque {
    static_assert(std::is_base_of_v<IntrusiveDequeNode, EntryT>);

  public:
    IntrusiveDeque() noexcept {
        head.next = &tail;
        tail.prev = &head;
    }
    // Sentinels have stable addresses that outside nodes point at; moving would leave those
    // nodes referencing the wrong sentinels. Non-owning entries must be unique, so copying
    // is banned too.
    IntrusiveDeque(IntrusiveDeque const&) = delete;
    IntrusiveDeque& operator=(IntrusiveDeque const&) = delete;
    IntrusiveDeque(IntrusiveDeque&&) = delete;
    IntrusiveDeque& operator=(IntrusiveDeque&&) = delete;

  public:
    template <typename NodeType, bool Forward>
    class Iterator {
        friend class IntrusiveDeque;

        using RawNode = std::conditional_t<
            std::is_const_v<NodeType>,
            IntrusiveDequeNode const,
            IntrusiveDequeNode>;
        RawNode* current;

        explicit Iterator(RawNode* node) noexcept : current(node) {}

      public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::remove_const_t<NodeType>;
        using difference_type = std::ptrdiff_t;
        using pointer = NodeType*;
        using reference = NodeType&;

        Iterator() noexcept : current(nullptr) {}

        reference operator*() const noexcept { return *static_cast<pointer>(current); }
        pointer operator->() const noexcept { return static_cast<pointer>(current); }

        Iterator& operator++() noexcept {
            current = Forward ? current->next : current->prev;
            return *this;
        }
        Iterator operator++(int) noexcept {
            Iterator tmp = *this;
            ++*this;
            return tmp;
        }
        Iterator& operator--() noexcept {
            current = Forward ? current->prev : current->next;
            return *this;
        }
        Iterator operator--(int) noexcept {
            Iterator tmp = *this;
            --*this;
            return tmp;
        }

        bool operator==(Iterator const& other) const noexcept {
            return current == other.current;
        }
        bool operator!=(Iterator const& other) const noexcept {
            return current != other.current;
        }
    };

    using iterator = Iterator<EntryT, true>;
    using const_iterator = Iterator<EntryT const, true>;
    using reverse_iterator = Iterator<EntryT, false>;
    using const_reverse_iterator = Iterator<EntryT const, false>;

    iterator begin() noexcept { return iterator(head.next); }
    iterator end() noexcept { return iterator(&tail); }
    const_iterator begin() const noexcept { return const_iterator(head.next); }
    const_iterator end() const noexcept { return const_iterator(&tail); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(tail.prev); }
    reverse_iterator rend() noexcept { return reverse_iterator(&head); }
    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(tail.prev);
    }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(&head); }

  public:
    EntryT* front() const noexcept {
        return empty() ? nullptr : static_cast<EntryT*>(head.next);
    }
    EntryT* back() const noexcept {
        return empty() ? nullptr : static_cast<EntryT*>(tail.prev);
    }
    bool empty() const noexcept { return head.next == &tail; }
    EntryT* push_back(EntryT* node) noexcept {
        node->prev = tail.prev;
        node->next = &tail;
        tail.prev->next = node;
        tail.prev = node;
        return node;
    }
    EntryT* push_front(EntryT* node) noexcept {
        node->prev = &head;
        node->next = head.next;
        head.next->prev = node;
        head.next = node;
        return node;
    }
    EntryT* pop_back() noexcept {
        if (empty()) {
            return nullptr;
        }
        IntrusiveDequeNode* node = tail.prev;
        node->deque_remove();
        return static_cast<EntryT*>(node);
    }
    EntryT* pop_front() noexcept {
        if (empty()) {
            return nullptr;
        }
        IntrusiveDequeNode* node = head.next;
        node->deque_remove();
        return static_cast<EntryT*>(node);
    }
    void extend_back(IntrusiveDeque<EntryT>&& other) noexcept {
        if (other.empty()) {
            return;
        }
        IntrusiveDequeNode* first = other.head.next;
        IntrusiveDequeNode* last = other.tail.prev;
        first->prev = tail.prev;
        last->next = &tail;
        tail.prev->next = first;
        tail.prev = last;
        other.clear();
    }
    void extend_front(IntrusiveDeque<EntryT>&& other) noexcept {
        if (other.empty()) {
            return;
        }
        IntrusiveDequeNode* first = other.head.next;
        IntrusiveDequeNode* last = other.tail.prev;
        first->prev = &head;
        last->next = head.next;
        head.next->prev = last;
        head.next = first;
        other.clear();
    }
    void clear() noexcept {
        head.next = &tail;
        tail.prev = &head;
    }

  private:
    IntrusiveDequeNode head;
    IntrusiveDequeNode tail;
};

}  // namespace coconext::detail

#endif  // COCONEXT_INTRUSIVE_DEQUE_HPP

// LCOV_EXCL_BR_START -- gtest macros generate noisy uncovered branches
#include <gtest/gtest.h>

#include <coconext/intrusive_deque.hpp>
#include <iterator>
#include <type_traits>
#include <vector>

using coconext::detail::IntrusiveDeque;
using coconext::detail::IntrusiveDequeNode;

namespace {

struct Node : public IntrusiveDequeNode {
    int v;
    explicit Node(int v) : v(v) {}
    // Exposed so tests can exercise anonymous self-removal.
    using IntrusiveDequeNode::deque_remove;
};

std::vector<int> to_vec(IntrusiveDeque<Node> const& d) {
    std::vector<int> out;
    for (auto const& n : d) {
        out.push_back(n.v);
    }
    return out;
}

std::vector<int> to_vec_reverse(IntrusiveDeque<Node> const& d) {
    std::vector<int> out;
    for (auto it = d.rbegin(); it != d.rend(); ++it) {
        out.push_back(it->v);
    }
    return out;
}

}  // namespace

// -- Basic state -----------------------------------------------------------

TEST(TestIntrusiveDeque, DefaultConstructedIsEmpty) {
    IntrusiveDeque<Node> d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.front(), nullptr);
    EXPECT_EQ(d.back(), nullptr);
    EXPECT_TRUE(d.begin() == d.end());
    EXPECT_TRUE(d.rbegin() == d.rend());
}

TEST(TestIntrusiveDeque, PopOnEmptyReturnsNull) {
    IntrusiveDeque<Node> d;
    EXPECT_EQ(d.pop_front(), nullptr);
    EXPECT_EQ(d.pop_back(), nullptr);
    EXPECT_TRUE(d.empty());
}

// -- push_back / push_front ------------------------------------------------

TEST(TestIntrusiveDeque, PushBackFromEmpty) {
    IntrusiveDeque<Node> d;
    Node a(1);
    EXPECT_EQ(d.push_back(&a), &a);
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.front(), &a);
    EXPECT_EQ(d.back(), &a);
    EXPECT_EQ(to_vec(d), (std::vector<int>{1}));
    EXPECT_EQ(to_vec_reverse(d), (std::vector<int>{1}));
}

TEST(TestIntrusiveDeque, PushBackMultiple) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);
    EXPECT_EQ(d.front(), &a);
    EXPECT_EQ(d.back(), &c);
    EXPECT_EQ(to_vec(d), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(to_vec_reverse(d), (std::vector<int>{3, 2, 1}));
}

TEST(TestIntrusiveDeque, PushFrontFromEmpty) {
    IntrusiveDeque<Node> d;
    Node a(1);
    EXPECT_EQ(d.push_front(&a), &a);
    EXPECT_EQ(d.front(), &a);
    EXPECT_EQ(d.back(), &a);
    EXPECT_EQ(to_vec(d), (std::vector<int>{1}));
    EXPECT_EQ(to_vec_reverse(d), (std::vector<int>{1}));
}

TEST(TestIntrusiveDeque, PushFrontMultiple) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_front(&a);
    d.push_front(&b);
    d.push_front(&c);
    EXPECT_EQ(to_vec(d), (std::vector<int>{3, 2, 1}));
    EXPECT_EQ(to_vec_reverse(d), (std::vector<int>{1, 2, 3}));
}

TEST(TestIntrusiveDeque, PushFrontAndPushBackMixed) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3), e(4);
    d.push_back(&b);
    d.push_front(&a);
    d.push_back(&c);
    d.push_front(&e);
    EXPECT_EQ(to_vec(d), (std::vector<int>{4, 1, 2, 3}));
}

// -- pop_back / pop_front --------------------------------------------------

TEST(TestIntrusiveDeque, PopFrontUntilEmpty) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);
    EXPECT_EQ(d.pop_front(), &a);
    EXPECT_EQ(d.pop_front(), &b);
    EXPECT_EQ(d.pop_front(), &c);
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.pop_front(), nullptr);
}

TEST(TestIntrusiveDeque, PopBackUntilEmpty) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);
    EXPECT_EQ(d.pop_back(), &c);
    EXPECT_EQ(d.pop_back(), &b);
    EXPECT_EQ(d.pop_back(), &a);
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.pop_back(), nullptr);
}

TEST(TestIntrusiveDeque, PoppedNodeCanBeReinserted) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2);
    d.push_back(&a);
    d.push_back(&b);
    EXPECT_EQ(d.pop_front(), &a);
    d.push_back(&a);
    EXPECT_EQ(to_vec(d), (std::vector<int>{2, 1}));
}

// -- Anonymous self-removal ------------------------------------------------

TEST(TestIntrusiveDeque, DequeRemoveMiddle) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);
    b.deque_remove();
    EXPECT_EQ(to_vec(d), (std::vector<int>{1, 3}));
    EXPECT_EQ(to_vec_reverse(d), (std::vector<int>{3, 1}));
    EXPECT_EQ(d.front(), &a);
    EXPECT_EQ(d.back(), &c);
}

TEST(TestIntrusiveDeque, DequeRemoveHead) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);
    a.deque_remove();
    EXPECT_EQ(to_vec(d), (std::vector<int>{2, 3}));
    EXPECT_EQ(d.front(), &b);
}

TEST(TestIntrusiveDeque, DequeRemoveTail) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);
    c.deque_remove();
    EXPECT_EQ(to_vec(d), (std::vector<int>{1, 2}));
    EXPECT_EQ(d.back(), &b);
}

TEST(TestIntrusiveDeque, DequeRemoveIsIdempotent) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2);
    d.push_back(&a);
    d.push_back(&b);
    a.deque_remove();
    a.deque_remove();  // no-op, must not corrupt the deque
    EXPECT_EQ(to_vec(d), (std::vector<int>{2}));
}

TEST(TestIntrusiveDeque, DequeRemoveOnUnlinkedNode) {
    Node unlinked(42);
    unlinked.deque_remove();  // never inserted; must be safe
    IntrusiveDeque<Node> d;
    d.push_back(&unlinked);
    EXPECT_EQ(d.front(), &unlinked);
}

// -- extend_back -----------------------------------------------------------

TEST(TestIntrusiveDeque, ExtendBackBasic) {
    IntrusiveDeque<Node> lhs, rhs;
    Node a(1), b(2), c(3), e(4);
    lhs.push_back(&a);
    lhs.push_back(&b);
    rhs.push_back(&c);
    rhs.push_back(&e);
    lhs.extend_back(std::move(rhs));
    EXPECT_TRUE(rhs.empty());
    EXPECT_EQ(to_vec(lhs), (std::vector<int>{1, 2, 3, 4}));
    // Reverse iteration must terminate (regression: extend_back used to leave
    // the spliced tail's next pointer aimed at the source's sentinel).
    EXPECT_EQ(to_vec_reverse(lhs), (std::vector<int>{4, 3, 2, 1}));
    EXPECT_EQ(lhs.back(), &e);
}

TEST(TestIntrusiveDeque, ExtendBackFromEmptySource) {
    IntrusiveDeque<Node> lhs, rhs;
    Node a(1);
    lhs.push_back(&a);
    lhs.extend_back(std::move(rhs));
    EXPECT_EQ(to_vec(lhs), (std::vector<int>{1}));
    EXPECT_TRUE(rhs.empty());
}

TEST(TestIntrusiveDeque, ExtendBackIntoEmptyDestination) {
    IntrusiveDeque<Node> lhs, rhs;
    Node a(1), b(2);
    rhs.push_back(&a);
    rhs.push_back(&b);
    lhs.extend_back(std::move(rhs));
    EXPECT_TRUE(rhs.empty());
    EXPECT_EQ(to_vec(lhs), (std::vector<int>{1, 2}));
    EXPECT_EQ(to_vec_reverse(lhs), (std::vector<int>{2, 1}));
    EXPECT_EQ(lhs.front(), &a);
    EXPECT_EQ(lhs.back(), &b);
}

TEST(TestIntrusiveDeque, ExtendBackNodesRemainRemovable) {
    IntrusiveDeque<Node> lhs, rhs;
    Node a(1), b(2), c(3);
    lhs.push_back(&a);
    rhs.push_back(&b);
    rhs.push_back(&c);
    lhs.extend_back(std::move(rhs));
    // b was moved from rhs; its prev/next must point into lhs now, so
    // deque_remove() must splice within lhs cleanly.
    b.deque_remove();
    EXPECT_EQ(to_vec(lhs), (std::vector<int>{1, 3}));
}

// -- extend_front ----------------------------------------------------------

TEST(TestIntrusiveDeque, ExtendFrontBasic) {
    IntrusiveDeque<Node> lhs, rhs;
    Node a(1), b(2), c(3), e(4);
    lhs.push_back(&c);
    lhs.push_back(&e);
    rhs.push_back(&a);
    rhs.push_back(&b);
    lhs.extend_front(std::move(rhs));
    EXPECT_TRUE(rhs.empty());
    EXPECT_EQ(to_vec(lhs), (std::vector<int>{1, 2, 3, 4}));
    EXPECT_EQ(to_vec_reverse(lhs), (std::vector<int>{4, 3, 2, 1}));
    EXPECT_EQ(lhs.front(), &a);
}

TEST(TestIntrusiveDeque, ExtendFrontFromEmptySource) {
    IntrusiveDeque<Node> lhs, rhs;
    Node a(1);
    lhs.push_back(&a);
    lhs.extend_front(std::move(rhs));
    EXPECT_EQ(to_vec(lhs), (std::vector<int>{1}));
}

TEST(TestIntrusiveDeque, ExtendFrontIntoEmptyDestination) {
    IntrusiveDeque<Node> lhs, rhs;
    Node a(1), b(2);
    rhs.push_back(&a);
    rhs.push_back(&b);
    lhs.extend_front(std::move(rhs));
    EXPECT_TRUE(rhs.empty());
    EXPECT_EQ(to_vec(lhs), (std::vector<int>{1, 2}));
    EXPECT_EQ(to_vec_reverse(lhs), (std::vector<int>{2, 1}));
}

TEST(TestIntrusiveDeque, ExtendFrontNodesRemainRemovable) {
    IntrusiveDeque<Node> lhs, rhs;
    Node a(1), b(2), c(3);
    lhs.push_back(&c);
    rhs.push_back(&a);
    rhs.push_back(&b);
    lhs.extend_front(std::move(rhs));
    a.deque_remove();
    EXPECT_EQ(to_vec(lhs), (std::vector<int>{2, 3}));
    EXPECT_EQ(lhs.front(), &b);
}

// -- clear ------------------------------------------------------------------

TEST(TestIntrusiveDeque, ClearLeavesReusableEmptyState) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.clear();
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.front(), nullptr);
    EXPECT_EQ(d.back(), nullptr);
    EXPECT_TRUE(d.begin() == d.end());
    d.push_back(&c);
    EXPECT_EQ(to_vec(d), (std::vector<int>{3}));
}

// -- Iterator concepts and mechanics ---------------------------------------

TEST(TestIntrusiveDeque, IteratorConcepts) {
    using D = IntrusiveDeque<Node>;
    static_assert(std::bidirectional_iterator<D::iterator>);
    static_assert(std::bidirectional_iterator<D::const_iterator>);
    static_assert(std::bidirectional_iterator<D::reverse_iterator>);
    static_assert(std::bidirectional_iterator<D::const_reverse_iterator>);
    static_assert(std::is_same_v<D::iterator::value_type, Node>);
    static_assert(std::is_same_v<D::const_iterator::pointer, Node const*>);
    static_assert(std::is_same_v<D::iterator::reference, Node&>);
    static_assert(std::is_same_v<D::const_iterator::reference, Node const&>);
    SUCCEED();
}

TEST(TestIntrusiveDeque, IteratorBidirectional) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);

    auto it = d.begin();
    EXPECT_EQ(&*it, &a);
    ++it;
    EXPECT_EQ(&*it, &b);
    auto post = it++;
    EXPECT_EQ(&*post, &b);
    EXPECT_EQ(&*it, &c);
    ++it;
    EXPECT_TRUE(it == d.end());
    --it;
    EXPECT_EQ(&*it, &c);
    auto post2 = it--;
    EXPECT_EQ(&*post2, &c);
    EXPECT_EQ(&*it, &b);
    --it;
    EXPECT_EQ(&*it, &a);
    EXPECT_TRUE(it == d.begin());
}

TEST(TestIntrusiveDeque, IteratorArrow) {
    IntrusiveDeque<Node> d;
    Node a(42);
    d.push_back(&a);
    EXPECT_EQ(d.begin()->v, 42);
}

TEST(TestIntrusiveDeque, StdDistanceCounts) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3), e(4);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);
    d.push_back(&e);
    EXPECT_EQ(std::distance(d.begin(), d.end()), 4);
    EXPECT_EQ(std::distance(d.rbegin(), d.rend()), 4);
}

TEST(TestIntrusiveDeque, ConstIteration) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2);
    d.push_back(&a);
    d.push_back(&b);
    IntrusiveDeque<Node> const& cd = d;
    std::vector<int> forward;
    for (auto const& n : cd) {
        forward.push_back(n.v);
    }
    EXPECT_EQ(forward, (std::vector<int>{1, 2}));
}

TEST(TestIntrusiveDeque, ReverseIteratorBidirectional) {
    IntrusiveDeque<Node> d;
    Node a(1), b(2), c(3);
    d.push_back(&a);
    d.push_back(&b);
    d.push_back(&c);

    auto rit = d.rbegin();
    EXPECT_EQ(&*rit, &c);
    ++rit;
    EXPECT_EQ(&*rit, &b);
    ++rit;
    EXPECT_EQ(&*rit, &a);
    ++rit;
    EXPECT_TRUE(rit == d.rend());
    --rit;
    EXPECT_EQ(&*rit, &a);
    --rit;
    EXPECT_EQ(&*rit, &b);
}

// -- Copy / move semantics -------------------------------------------------

TEST(TestIntrusiveDeque, NotCopyableOrMovable) {
    static_assert(!std::is_copy_constructible_v<IntrusiveDeque<Node>>);
    static_assert(!std::is_copy_assignable_v<IntrusiveDeque<Node>>);
    static_assert(!std::is_move_constructible_v<IntrusiveDeque<Node>>);
    static_assert(!std::is_move_assignable_v<IntrusiveDeque<Node>>);
    SUCCEED();
}

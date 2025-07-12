# Copyright cocotb contributors
# Licensed under the Revised BSD License, see LICENSE for details.
# SPDX-License-Identifier: BSD-3-Clause

"""Collection of asynchronous queues for cocotb."""

from __future__ import annotations

import heapq
import sys
from abc import abstractmethod
from asyncio import QueueEmpty, QueueFull
from collections import deque
from typing import Any, Generic, TypeVar

from cocotb.task import Task, current_task
from cocotb.triggers import Event

if sys.version_info >= (3, 11):
    from typing import Self

T = TypeVar("T")


class AbstractQueue(Generic[T]):
    """An asynchronous queue, useful for coordinating producer and consumer tasks.

    Queues can have various semantics with respect to the values :meth:`put` and :meth:`get` from them.
    This class does not enforce any particular semantics,
    but leaves the implementation and semantics abstract.
    A concrete queue class can be created by subclassing :class:`!AbstractQueue`
    and filling in the :meth:`_size`, :meth:`_put`, :meth:`_get`, and :meth:`_peek` methods.

    Additionally, there are common implementations included in this module:
    :class:`Queue` (FIFO semantics), :class:`LifoQueue` (LIFO semantics), and :class:`PriorityQueue` (min-heap semantics).

    .. versionadded:: 0.2
    """

    class Lock:
        r"""A mutex lock specialized for use by asynchronous Queue.

        This class is not intended to be instantiated by the user,
        but rather be acquired via :attr:`.AbstractQueue.write_lock`
        or :attr:`.AbstractQueue.read_lock`.

        In addition to all the functionality available on :class:`~cocotb.triggers.Lock`,
        these locks are aware of the associated queue's *availability*,
        i.e. whether the read or write side has data or space available to either get or put, respectively.
        """

        def __init__(self) -> None:
            self._current_acquirer: Task[object] | None = None
            self._acquirers: deque[tuple[Event, Task]] = deque()

        def locked(self) -> bool:
            """Return ``True`` if the lock is *locked*."""
            return self._current_acquirer is not None

        @abstractmethod
        def _is_available(self) -> bool: ...

        def available(self) -> bool:
            """Return ``True`` if the lock is *available* for acquiring."""
            return self._is_available()

        async def acquire(self) -> None:
            """Acquire the lock.

            Waits until the lock is *available* and *unlocked*, sets it to *locked* and returns.
            Multiple Tasks may attempt to acquire the Lock, but only one will proceed.
            Tasks are guaranteed to acquire the Lock in the order each attempts to acquire it.
            """
            acquirer_task = current_task()
            if not self.available() or self.locked():
                if acquirer_task is self._current_acquirer:
                    # User tried acquiring a Lock they already acquired. Return to prevent deadlock.
                    return
                e = Event()
                self._acquirers.append((e, acquirer_task))
                await e.wait()
            else:
                self._current_acquirer = acquirer_task

        def release(self) -> None:
            """Release the lock.

            Sets the lock to *unlocked*.

            Raises:
                RuntimeError: If called when the lock is *unlocked*.
            """
            if not self.locked():
                raise RuntimeError("Lock is not acquired")
            self._current_acquirer = None
            self._wakeup_next()

        def _wakeup_next(self) -> None:
            if self.locked():
                # Already locked/pending, don't wake up another waiter.
                return
            if not self.available():
                # Nothing available, don't wake up a waiter.
                return
            # Find first living waiter and wake it.
            while self._acquirers:
                e, task = self._acquirers.popleft()
                if not task.done():
                    self._current_acquirer = task
                    return e.set()

        async def __aenter__(self) -> Self:
            await self.acquire()
            return self

        async def __aexit__(self, *_: object) -> None:
            self.release()

    class _WriteLock(Lock):
        def __init__(self, queue: AbstractQueue[Any]) -> None:
            super().__init__()
            self._queue = queue

        def _is_available(self) -> bool:
            return not self._queue.full()

    class _ReadLock(Lock):
        def __init__(self, queue: AbstractQueue[Any]) -> None:
            super().__init__()
            self._queue = queue

        def _is_available(self) -> bool:
            return not self._queue.empty()

    def __init__(self, maxsize: int = 0) -> None:
        """Construct a queue with the given *maxsize*.

        If *maxsize* is less than or equal to 0, the queue size is infinite. If it
        is an integer greater than 0, then :meth:`put` will block when the queue
        reaches *maxsize*, until an item is removed by :meth:`get`.
        """
        self._maxsize: int = maxsize

        self._write_lock = self._WriteLock(self)
        self._read_lock = self._ReadLock(self)

    @property
    def write_lock(self) -> Lock:
        """Lock for exclusive write access.

        After acquiring this lock, the user has exclusive access to the write-side of the queue until the lock is released.
        Calling :meth:`put` while the write lock is acquired will result in a :exc:`RuntimeError` to prevent deadlock.
        """
        return self._write_lock

    @property
    def read_lock(self) -> Lock:
        """Lock for exclusive read access.

        After acquiring this lock, the user has exclusive access to the read-side of the queue until the lock is released.
        Calling :meth:`get` while the read lock is acquired will result in a :exc:`RuntimeError` to prevent deadlock.
        """
        return self._read_lock

    @abstractmethod
    def _get(self) -> T:
        """Remove and return the next element from the queue."""

    @abstractmethod
    def _peek(self) -> T:
        """Return the next element from the queue without removing it."""

    @abstractmethod
    def _put(self, item: T) -> None:
        """Place a new element on the queue."""

    @abstractmethod
    def _size(self) -> int:
        """Return the number of elements in the queue."""

    def qsize(self) -> int:
        """Number of items in the queue."""
        return self._size()

    @property
    def maxsize(self) -> int:
        """Number of items allowed in the queue."""
        return self._maxsize

    def empty(self) -> bool:
        """Return ``True`` if the queue is empty, ``False`` otherwise."""
        return self._size() == 0

    def full(self) -> bool:
        """Return ``True`` if there are :meth:`maxsize` items in the queue.

        .. note::
            If the Queue was initialized with ``maxsize=0`` (the default), then
            :meth:`!full` is never ``True``.
        """
        if self._maxsize <= 0:
            return False
        else:
            return self.qsize() >= self._maxsize

    async def put(self, item: T) -> None:
        """Put an *item* into the queue.

        If the queue is full, wait until a free
        slot is available before adding the item.
        """
        async with self._write_lock:
            self.put_nowait(item)

    def put_nowait(self, item: T) -> None:
        """Put an *item* into the queue without blocking.

        If no free slot is immediately available, raise :exc:`~cocotb.queue.QueueFull`.
        """
        if self.full():
            raise QueueFull()
        self._put(item)
        # This changes availability, so inform the read side Lock.
        self._read_lock._wakeup_next()

    async def get(self) -> T:
        """Remove and return an item from the queue.

        If the queue is empty, wait until an item is available.
        """
        async with self._read_lock:
            return self.get_nowait()

    def get_nowait(self) -> T:
        """Remove and return an item from the queue.

        Returns an item if one is immediately available,
        otherwise raises :exc:`~cocotb.queue.QueueEmpty`.
        """
        if self.empty():
            raise QueueEmpty()
        item = self._get()
        # This changes availability, so inform the write side Lock.
        self._write_lock._wakeup_next()
        return item

    async def peek(self) -> T:
        """Return the next item from the queue without removing it.

        If the queue is empty, wait until an item is available.
        """
        async with self._read_lock:
            return self.peek_nowait()

    def peek_nowait(self) -> T:
        """Return the next item from the queue without removing it.

        Returns an item if one is immediately available,
        otherwise raises :exc:`~cocotb.queue.QueueEmpty`.
        """
        if self.empty():
            raise QueueEmpty()
        item = self._peek()
        return item


class Queue(AbstractQueue[T]):
    """A subclass of :class:`AbstractQueue`; retrieves oldest entries first (FIFO).

    .. versionadded:: 2.0
    """

    def __init__(self, maxsize: int = 0) -> None:
        super().__init__(maxsize)
        self._queue: deque[T] = deque()

    def _put(self, item: T) -> None:
        self._queue.append(item)

    def _get(self) -> T:
        return self._queue.popleft()

    def _peek(self) -> T:
        return self._queue[0]

    def _size(self) -> int:
        return len(self._queue)


class PriorityQueue(AbstractQueue[T]):
    r"""A subclass of :class:`AbstractQueue`; retrieves entries in priority order (smallest item first).

    Entries are typically tuples of the form ``(priority number, data)``.

    .. versionadded:: 2.0
    """

    def __init__(self, maxsize: int = 0) -> None:
        super().__init__(maxsize)
        self._queue: list[T] = []

    def _put(self, item: T) -> None:
        heapq.heappush(self._queue, item)

    def _get(self) -> T:
        return heapq.heappop(self._queue)

    def _peek(self) -> T:
        return self._queue[0]

    def _size(self) -> int:
        return len(self._queue)


class LifoQueue(AbstractQueue[T]):
    """A subclass of :class:`AbstractQueue`; retrieves most recently added entries first (LIFO).

    .. versionadded:: 2.0
    """

    def __init__(self, maxsize: int = 0) -> None:
        super().__init__(maxsize)
        self._queue: deque[T] = deque()

    def _put(self, item: T) -> None:
        self._queue.append(item)

    def _get(self) -> T:
        return self._queue.pop()

    def _peek(self) -> T:
        return self._queue[-1]

    def _size(self) -> int:
        return len(self._queue)

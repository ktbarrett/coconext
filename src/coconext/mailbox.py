"""Defines Mailbox and related objects."""

from __future__ import annotations

from typing import Deque, Generic, Sized, TypeVar

from cocotb.queue import QueueEmpty
from cocotb.triggers import NullTrigger, Trigger

from coconext.triggers import Notify

T = TypeVar("T")


class Mailbox(Sized, Generic[T]):
    """Unbounded UVM-esque mailbox."""

    def __init__(self) -> None:  # noqa: D107
        self._queue = Deque[T]()
        self._put_notify = Notify()

    def put_nowait(self, value: T) -> None:
        """Put a value on the queue without blocking."""
        self._queue.append(value)
        self._put_notify.notify()

    def __len__(self) -> int:
        """Return the number of elements in the Mailbox.

        Also ensures ``bool(mailbox)`` returns ``True`` if it is not empty, much like lists.
        """
        return len(self._queue)

    def get_nowait(self) -> T:
        """Get a value from the queue without blocking."""
        if not self._queue:
            raise QueueEmpty()
        return self._queue.popleft()

    async def get(self) -> T:
        """Get a value from the queue, blocking until a value becomes available."""
        while not self._queue:
            await self._put_notify.wait()
        return self._queue.popleft()

    def available(self) -> Trigger:
        """Return Trigger which blocks until data is available in the mailbox."""
        if self._queue:
            return NullTrigger()
        return self._put_notify.wait()

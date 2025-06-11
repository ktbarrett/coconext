"""Collection of types that would end up in :mod:`cocotb.triggers`."""

from __future__ import annotations

from collections import deque
from typing import TYPE_CHECKING

from cocotb.task import Task, current_task
from cocotb.triggers import Event

if TYPE_CHECKING:
    from typing import Self


class Notify:
    """Notify all waiters of an event.

    :class:`cocotb.triggers.Event` has state.
    It can be in the state where :meth:`.Event.is_set` is ``False`` and calling
    :meth:`.Event.wait` will return a Trigger that only fires after :meth:`.Event.set` is called.
    Or it can be in a state where :meth:`.Event.is_set` is ``True`` and calling
    :meth:`.Event.wait` will return a Trigger that fires immediately.

    This object does not have state.
    It behaves like :class:`.Event` if it were only ever in the :meth:`.Event.is_set` is ``False`` state.
    All calls to :meth:`wait` will block until the next call to :meth:`notify` occurs.

    .. versionadded:: 0.1
    """

    def __init__(self) -> None:
        self._event = Event()

    def notify(self) -> None:
        """Wake up all waiters."""
        self._event.set()
        self._event.clear()

    async def wait(self) -> None:
        """Wait until the notify() method is called."""
        await self._event.wait()


class Lock:
    """A fair mutual exclusion lock.

    This functions similarly to :class:`.Lock`, except it designed to be "fair".
    Tasks are guaranteed to acquire the Lock in the order each attempts to acquire it.

    .. versionadded:: 0.2
    """

    def __init__(self) -> None:
        self._locked: bool = False
        self._acquirers = deque[tuple[Event, Task]]()
        self._current_acquirer: Task[object]

    def locked(self) -> bool:
        """Return ``True`` if the lock is *locked*."""
        return self._locked

    async def acquire(self) -> None:
        """Acquire the lock.

        Waits until the lock is *unlocked*, sets it to *locked* and returns.
        Multiple Tasks may attempt to acquire the Lock, but only one will proceed.
        Tasks are guaranteed to acquire the Lock in the order each attempts to acquire it.

        Raises:
            RuntimeError: If the Task that has acquired the lock attempt to reacquire it.
        """
        if self._locked:
            acquirer_task = current_task()
            if acquirer_task is self._current_acquirer:
                raise RuntimeError(
                    "Acquiring Task while you already have it is not allowed"
                )
            e = Event()
            self._acquirers.append((e, acquirer_task))
            await e.wait()
        self._locked = True

    def release(self) -> None:
        """Release the lock.

        Sets the lock to *unlocked*.

        Raises:
            RuntimeError: If called when the lock is *unlocked*.
        """
        if not self._locked:
            raise RuntimeError("Lock is not acquired")
        self._locked = False
        while self._acquirers:
            e, task = self._acquirers.popleft()
            if not task.done():
                e.set()
                return

    async def __aenter__(self) -> Self:
        await self.acquire()
        return self

    async def __aexit__(self, *_: object) -> None:
        self.release()

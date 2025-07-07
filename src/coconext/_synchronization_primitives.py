from __future__ import annotations

from cocotb.triggers import Event


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

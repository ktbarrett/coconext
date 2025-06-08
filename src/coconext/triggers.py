"""Collection of types that would end up in :mod:`cocotb.triggers`."""

from __future__ import annotations

from cocotb.triggers import Event, Trigger


class Notify:
    """Notify all waiters of an event.

    :class:`cocotb.triggers.Event` has state.
    It can be in a state where :meth:`.Event.is_set` is ``True`` and calling
    :meth:`.Event.wait` will return a Trigger that fires immediately.

    This object odes not have a "set" state.
    All calls to :meth:`wait` will block until the next call to :meth:`notify` occurs.

    .. versionadded:: 0.1
    """

    def __init__(self) -> None:
        self._event = Event()

    def notify(self) -> None:
        """Wake up all waiters."""
        self._event.set()
        self._event.clear()

    def wait(self) -> Trigger:
        """Return Trigger that blocks until the notify() method is called."""
        return self._event.wait()

"""Collection of types that would end up in :mod:`cocotb.triggers`."""

from __future__ import annotations

from cocotb.triggers import Event, Trigger


class Notify:
    """Object which wakes up all waiters when notify() is called."""

    def __init__(self) -> None:  # noqa: D107
        self._event = Event()

    def notify(self) -> None:
        """Wake up all waiters."""
        self._event.set()
        self._event.clear()

    def wait(self) -> Trigger:
        """Return Trigger that blocks until the notify() method is called."""
        return self._event.wait()

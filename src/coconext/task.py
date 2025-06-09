"""Types and functions that would end up in :mod:`cocotb.task`."""

from __future__ import annotations

import cocotb
from cocotb.task import Task


def current_task() -> Task[object]:
    """Return the currently running Task.

    Raises:
        RuntimeError: If no Task is currently running.

    .. versionadded: 0.2
    """
    task = cocotb._scheduler_inst._current_task
    if task is None:
        raise RuntimeError("No Task is running")
    return task

"""Collection of tools for concurrency and synchronization in cocotb."""

from __future__ import annotations

from coconext._concurrent_waiters import gather, select, wait
from coconext._synchronization_primitives import Notify
from coconext._task_manager import TaskManager

__all__ = (
    "Notify",
    "TaskManager",
    "gather",
    "select",
    "wait",
)

# Fix up module reference
for obj_name in __all__:
    globals()[obj_name].__module__ = __name__

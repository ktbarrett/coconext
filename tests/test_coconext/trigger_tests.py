from __future__ import annotations

import cocotb
from cocotb.task import Task
from cocotb.triggers import Timer

from coconext.triggers import Notify


@cocotb.test
async def test_notify(_: object) -> None:
    n = Notify()

    # start up a bunch of waiters on the notify
    async def waiter() -> None:
        await n.wait()

    waiters: list[Task] = []
    for _ in range(100):
        waiters.append(cocotb.start_soon(waiter()))

    # ensure none have finished
    await Timer(1, "step")
    assert not any(w.done() for w in waiters)

    # ensure all finish after notify()
    n.notify()
    await Timer(1, "step")
    assert all(w.done() for w in waiters)

    # start up more wiaters
    waiters.clear()
    for _ in range(100):
        waiters.append(cocotb.start_soon(waiter()))

    # ensure none have finished just because previous notify happened
    await Timer(1, "step")
    assert not any(w.done() for w in waiters)

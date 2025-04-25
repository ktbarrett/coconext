from __future__ import annotations

import random
from typing import Any

import cocotb
import pytest
from cocotb.triggers import Combine, SimTimeoutError, Timer, with_timeout

from coconext.mailbox import Mailbox


@cocotb.test
async def test_mailbox(_: Any) -> None:
    m = Mailbox[int]()

    assert not m
    assert len(m) == 0

    for i in range(10):
        m.put_nowait(i)

    await with_timeout(m.available(), 1, "step")

    assert m
    assert len(m) == 10

    for i in range(10):
        assert m.get_nowait() == i

    with pytest.raises(SimTimeoutError):
        await with_timeout(m.available(), 10, "ns")

    async def waiter() -> None:
        for i in range(10, 20):
            assert await m.get() == i

    async def sender() -> None:
        for i in range(10, 20):
            await Timer(random.randint(1, 10), "ns")
            m.put_nowait(i)

    await with_timeout(
        Combine(
            cocotb.start_soon(waiter()),
            cocotb.start_soon(sender()),
        ),
        101,
        "ns",
    )

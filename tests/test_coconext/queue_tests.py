from __future__ import annotations

import heapq
import random
from asyncio import QueueEmpty, QueueFull
from collections.abc import Coroutine
from typing import Any

import cocotb
import pytest
from cocotb.task import Task
from cocotb.triggers import (
    Combine,
    NullTrigger,
    SimTimeoutError,
    Timer,
    Trigger,
    with_timeout,
)

from coconext.queue import AbstractQueue, LifoQueue, PriorityQueue, Queue


@cocotb.test(timeout_time=10, timeout_unit="ns")
@cocotb.parametrize(queue_type=(Queue, LifoQueue, PriorityQueue))
async def test_queue_basic(_: object, queue_type: type[AbstractQueue[int]]) -> None:
    q = queue_type(maxsize=2)

    assert q.maxsize == 2

    assert q.qsize() == 0
    assert not q.full()
    assert q.empty()

    with pytest.raises(QueueEmpty):
        q.get_nowait()
    with pytest.raises(QueueEmpty):
        q.peek_nowait()
    assert q.qsize() == 0

    q.put_nowait(1)
    assert q.qsize() == 1
    assert not q.full()
    assert not q.empty()

    assert q.peek_nowait() == 1

    q.put_nowait(2)
    assert q.qsize() == 2
    assert q.full()
    assert not q.empty()

    with pytest.raises(QueueFull):
        q.put_nowait(3)

    # Save outputs in unordered collection since each queue type has different behavior.
    outputs: set[int] = set()

    outputs.add(q.get_nowait())
    assert q.qsize() == 1
    assert not q.full()
    assert not q.empty()

    outputs.add(q.get_nowait())
    assert q.qsize() == 0
    assert not q.full()
    assert q.empty()

    with pytest.raises(QueueEmpty):
        q.get_nowait()
    assert q.qsize() == 0

    assert outputs == {1, 2}  # 3 was pushed when full and we shouldn't get it.


@cocotb.test(timeout_time=10, timeout_unit="ns")
@cocotb.parametrize(queue_type=(Queue, LifoQueue, PriorityQueue))
async def test_queue_async(_: object, queue_type: type[AbstractQueue[int]]) -> None:
    q = queue_type(maxsize=2)

    assert q.maxsize == 2

    await q.put(1)

    # We know it can only be `1`, while after 2 puts the behavior between the queue types differs.
    assert await q.peek() == 1

    await q.put(2)
    with pytest.raises(SimTimeoutError):
        await with_timeout(q.put(3), 1, "step")

    outputs: set[int] = set()
    outputs.add(await q.get())
    outputs.add(await q.get())
    with pytest.raises(SimTimeoutError):
        await with_timeout(q.get(), 1, "step")

    assert outputs == {1, 2}


@cocotb.test(timeout_time=10, timeout_unit="ns")
@cocotb.parametrize(queue_type=(Queue, LifoQueue, PriorityQueue))
async def test_queue_lock_direct(
    _: object, queue_type: type[AbstractQueue[int]]
) -> None:
    """Test Lock acquire/release."""
    q = queue_type(maxsize=2)

    assert not q.write_lock.locked()
    await q.write_lock.acquire()
    assert q.write_lock.locked()

    await q.write_lock.acquire()  # already acquired should no-op
    assert q.write_lock.locked()

    q.write_lock.release()
    assert not q.write_lock.locked()

    with pytest.raises(RuntimeError):
        q.write_lock.release()
    assert not q.write_lock.locked()

    async with q.write_lock:
        assert q.write_lock.locked()
        await NullTrigger()
        assert q.write_lock.locked()

    assert not q.write_lock.locked()


@cocotb.test(timeout_time=10, timeout_unit="ns")
@cocotb.parametrize(queue_type=(Queue, LifoQueue, PriorityQueue))
async def test_queue_lock_no_op(
    _: object, queue_type: type[AbstractQueue[int]]
) -> None:
    q = queue_type()

    async def writer(value: int) -> None:
        await q.put(value)

    task = cocotb.start_soon(writer(20))

    assert q.write_lock.available()
    assert not q.write_lock.locked()

    async with q.write_lock:
        assert q.write_lock.locked()
        await Timer(1)

    assert q.write_lock.available()
    # locked because of pending writer() Task
    assert not task.done()
    assert q.qsize() == 0

    await NullTrigger()
    assert task.done()
    assert q.qsize() == 1

    async def reader() -> int:
        return await q.get()

    task2 = cocotb.start_soon(reader())

    assert q.read_lock.available()
    assert not q.read_lock.locked()

    async with q.read_lock:
        assert q.read_lock.locked()
        await Timer(1)

    assert q.read_lock.available()
    # locked because of pending reader() Task
    assert not task2.done()
    assert q.qsize() == 1

    await NullTrigger()
    assert task2.done()
    assert q.qsize() == 0
    assert task2.result() == 20


@cocotb.test(timeout_time=10, timeout_unit="ns")
@cocotb.parametrize(queue_type=(Queue, LifoQueue, PriorityQueue))
async def test_queue_lock_multi_op(
    _: object, queue_type: type[AbstractQueue[int]]
) -> None:
    q = queue_type()

    async with q.write_lock:
        for i in range(10):
            q.put_nowait(i)

    outputs: set[int] = set()
    async with q.read_lock:
        for _ in range(10):
            outputs.add(q.get_nowait())

    assert outputs == set(range(10))


@cocotb.test(timeout_time=10, timeout_unit="ns")
@cocotb.parametrize(queue_type=(Queue, LifoQueue, PriorityQueue))
async def test_queue_lock_contenting(
    _: object, queue_type: type[AbstractQueue[int]]
) -> None:
    """Test Queue with multiple simultaneous readers/writers."""
    q = queue_type()

    async def writer(value: int) -> None:
        await q.put(value)

    task = cocotb.start_soon(writer(20))

    async with q.write_lock:
        await NullTrigger()
        assert not task.done()  # blocking
        q.put_nowait(1)
        q.put_nowait(2)
        q.put_nowait(3)

    assert not task.done()  # scheduled
    assert q.qsize() == 3

    await NullTrigger()
    assert task.done()  # acquired lock and finished
    assert q.qsize() == 4

    async def reader() -> None:
        await q.get()

    task = cocotb.start_soon(reader())

    async with q.read_lock:
        await NullTrigger()
        assert not task.done()  # blocking
        q.get_nowait()
        q.get_nowait()
        q.get_nowait()
        q.get_nowait()

    assert not task.done()  # scheduled
    assert q.qsize() == 0

    # This should block forever since we are empty, so ensure timeout
    with pytest.raises(SimTimeoutError):
        await with_timeout(task, 1, "step")


@cocotb.test(timeout_time=10, timeout_unit="ns")
@cocotb.parametrize(queue_type=(Queue, LifoQueue, PriorityQueue))
async def test_queue_random_contention(
    _: object, queue_type: type[AbstractQueue[int]]
) -> None:
    TEST_LEN = 5000
    MAX_WAIT = 10

    q = queue_type(10)

    class Checker:
        def __init__(self) -> None:
            self._last_seen = [0, 0]
            self._cancelleds: tuple[set[int], set[int]] = (set(), set())
            self._tasks: list[Task[Any]] = []
            self._task_info: dict[Task[Any], tuple[bool, int]] = {}
            self._task_info_reversed: dict[tuple[bool, int], Task[Any]] = {}

        @property
        def tasks(self) -> tuple[Task[Any], ...]:
            return tuple(self._tasks)

        def add(
            self, coro: Coroutine[Trigger, None, Any], idx: int, is_writer: bool
        ) -> None:
            cocotb.log.debug(f"added {is_writer=} {idx=}")
            task = cocotb.start_soon(coro)
            self._task_info[task] = (is_writer, idx)
            self._task_info_reversed[(is_writer, idx)] = task
            self._tasks.append(task)

        def acquired(self, idx: int, is_writer: bool) -> None:
            cocotb.log.debug(f"acquired {is_writer=} {idx=}")
            task = self._task_info_reversed[(is_writer, idx)]
            self._tasks.remove(task)

            for i in range(self._last_seen[is_writer] + 1, idx):
                if i not in self._cancelleds[is_writer]:
                    assert False, f"Out of order, expected {i}, got {idx}"
                else:
                    self._cancelleds[is_writer].remove(i)
            self._last_seen[is_writer] = idx

        def cancel(self, task: Task[Any]) -> None:
            task.cancel()
            is_writer, idx = self._task_info[task]
            self._cancelleds[is_writer].add(idx)
            cocotb.log.debug(f"cancelled {is_writer=} {idx=}")

    checker = Checker()

    async def start_writers() -> None:
        async def writer(idx: int) -> None:
            async with q.write_lock:
                checker.acquired(idx, is_writer=True)
                q.put_nowait(0)
                await Timer(random.randint(1, MAX_WAIT))

        for idx in range(TEST_LEN):
            checker.add(writer(idx), idx, is_writer=True)
            await Timer(random.randint(1, MAX_WAIT))

    async def start_readers() -> None:
        async def reader(idx: int) -> None:
            async with q.read_lock:
                checker.acquired(idx, is_writer=False)
                q.get_nowait()
                await Timer(random.randint(1, MAX_WAIT))

        for idx in range(TEST_LEN):
            checker.add(reader(idx), idx, is_writer=False)
            await Timer(random.randint(1, MAX_WAIT))

    async def random_canceller() -> None:
        while True:
            await Timer(random.randint(1, MAX_WAIT) * 20)
            if tasks := checker.tasks:
                task = random.choice(tasks)
                checker.cancel(task)

    canceller = cocotb.start_soon(random_canceller())
    await Combine(
        cocotb.start_soon(start_writers()), cocotb.start_soon(start_readers())
    )
    await Combine(*checker.tasks)
    canceller.cancel()
    await NullTrigger()


@cocotb.test
async def test_queue_behavior(_: object) -> None:
    q = Queue[int]()

    curr_in_val = 0
    curr_out_val = 0

    for _ in range(10):
        for _ in range(random.randrange(100)):
            q.put_nowait(curr_in_val)
            curr_in_val += 1

        for _ in range(random.randrange(100)):
            if q.empty():
                break
            assert q.get_nowait() == curr_out_val
            curr_out_val += 1

    while not q.empty():
        assert q.get_nowait() == curr_out_val
        curr_out_val += 1


@cocotb.test
async def test_lifo_queue_behavior(_: object) -> None:
    q = LifoQueue[int]()

    curr_in_val = 0
    stack: list[int] = []

    for _ in range(10):
        for _ in range(random.randrange(100)):
            q.put_nowait(curr_in_val)
            stack.append(curr_in_val)
            curr_in_val += 1

        for _ in range(random.randrange(100)):
            if q.empty():
                break
            expected_val = stack.pop()
            assert q.get_nowait() == expected_val

    while not q.empty():
        expected_val = stack.pop()
        assert q.get_nowait() == expected_val


@cocotb.test
async def test_priority_queue_behavior(_: object) -> None:
    q = PriorityQueue[int]()

    heap: list[int] = []

    for _ in range(10):
        for _ in range(random.randrange(100)):
            random_val = random.getrandbits(32)
            heapq.heappush(heap, random_val)

        for _ in range(random.randrange(100)):
            if q.empty():
                break
            expected_val = heapq.heappop(heap)
            assert q.get_nowait() == expected_val

    while not q.empty():
        expected_val = heapq.heappop(heap)
        assert q.get_nowait() == expected_val

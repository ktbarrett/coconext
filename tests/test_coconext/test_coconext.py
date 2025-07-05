from __future__ import annotations

import os
from pathlib import Path

import pytest
from cocotb_tools.runner import Runner, get_runner


@pytest.fixture
def runner() -> Runner:
    pwd = Path(__file__).parent.absolute()
    runner = get_runner(os.getenv("SIM", "nvc"))
    runner.build(sources=[pwd / "empty.vhd"])
    return runner


def test_triggers(runner: Runner) -> None:
    runner.test(
        test_module="trigger_tests",
        hdl_toplevel="top",
        test_dir=runner.build_dir / "trigger_tests",
    )


def test_simtime(runner: Runner) -> None:
    runner.test(
        test_module="simtime_tests",
        hdl_toplevel="top",
        test_dir=runner.build_dir / "simtime_tests",
    )


def test_queues(runner: Runner) -> None:
    runner.test(
        test_module="queue_tests",
        hdl_toplevel="top",
        test_dir=runner.build_dir / "queue_tests",
    )

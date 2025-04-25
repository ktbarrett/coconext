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


def test_mailbox(runner: Runner) -> None:
    runner.test(
        test_module="mailbox_tests",
        hdl_toplevel="top",
    )


def test_triggers(runner: Runner) -> None:
    runner.test(
        test_module="trigger_tests",
        hdl_toplevel="top",
    )

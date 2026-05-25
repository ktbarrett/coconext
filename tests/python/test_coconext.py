from __future__ import annotations

import os
from pathlib import Path
from typing import Any

import pytest
from cocotb_tools.runner import Runner, get_runner

# required for pytest
import coconext  # noqa: F401


@pytest.fixture
def runner() -> Runner:
    pwd = Path(__file__).parent.absolute()
    sim = os.getenv("SIM", "nvc").lower().strip()
    r = get_runner(sim)
    lang = os.getenv("TOPLEVEL_LANG", "vhdl").lower().strip()
    if lang == "vhdl":
        sources = [pwd / "empty.vhd"]
    else:
        sources = [pwd / "empty.sv"]
    addl_args: dict[str, Any] = {}
    if sim == "icarus":
        addl_args = {
            "timescale": ("1fs", "1fs"),
        }
    r.build(sources=sources, hdl_toplevel="top", **addl_args)
    return r


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

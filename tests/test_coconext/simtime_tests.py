from __future__ import annotations

import cocotb
import pytest
from cocotb.triggers import with_timeout

from coconext.simtime import SimTime


@cocotb.test
async def test_simtime(_: object) -> None:
    # constructing SimTime
    assert SimTime(1, "fs").fs == 1
    assert SimTime(5, "ps").ps == 5
    assert SimTime(47, "ns").ns == 47
    assert SimTime(25, "us").us == 25
    assert SimTime(9, "ms").ms == 9
    assert SimTime(5, "sec").sec == 5
    assert SimTime(1, "step").step == 1

    # construct with bad types
    with pytest.raises(TypeError):
        SimTime(object(), "ns")  # type: ignore
    with pytest.raises(TypeError):
        SimTime(1, {})  # type: ignore
    with pytest.raises(ValueError):
        SimTime(1, "ns", round_mode=1)  # type: ignore
    with pytest.raises(ValueError):
        SimTime(1, "ns", round_mode="nope")  # type: ignore

    # 0 steps
    assert SimTime(0, "ns").step == 0
    assert SimTime(0, "ns").ns == 0
    assert SimTime(0, "ns").sec == 0

    # negative steps
    assert SimTime(-10, "ns").ns == -10

    # unit conversion
    assert SimTime(2500, "ns").us == 2.5
    assert SimTime(1, "sec").ps == 1_000_000_000_000
    assert SimTime(25, "ps").ns == 0.025

    # comparison
    assert SimTime(1, "ns") < SimTime(0.1, "us")
    assert SimTime(1, "ns") >= SimTime(1, "ns")
    assert -SimTime(1, "ns") <= SimTime(0, "ns")
    assert SimTime(10, "ps") > SimTime(2, "ps")
    assert SimTime(1, "ps") == SimTime(1, "ps")
    assert SimTime(1, "ps") != SimTime(2, "ps")

    # comparison with bad types
    with pytest.raises(TypeError):
        SimTime(1, "ns") < object()  # type: ignore
    with pytest.raises(TypeError):
        SimTime(1, "ns") <= 9  # type: ignore
    with pytest.raises(TypeError):
        SimTime(1, "ns") > "lol"  # type: ignore
    with pytest.raises(TypeError):
        SimTime(1, "ns") >= set()  # type: ignore
    assert SimTime(1, "ns") != SimTime

    # arithmetic
    assert SimTime(1, "ns") + SimTime(1, "ps") == SimTime(1001, "ps")
    assert SimTime(1, "ns") - SimTime(2, "ns") == SimTime(-1, "ns")
    assert SimTime(500, "ns") * 2 == SimTime(1, "us")
    assert 5 * SimTime(200, "ns") == SimTime(1, "us")
    assert -SimTime(10, "ns") == SimTime(-0.01, "us")
    with pytest.raises(ValueError):
        SimTime(1, "ns") * 0.0000000001
    with pytest.raises(ValueError):
        SimTime(1, "ns") / 3
    assert SimTime(1, "ns") // 3 == SimTime(1 / 3, "ns", round_mode="floor")

    # arithmetic with bad types
    with pytest.raises(TypeError):
        SimTime(1, "ns") + "123"  # type: ignore
    with pytest.raises(TypeError):
        9 + SimTime(1, "ns")  # type: ignore
    with pytest.raises(TypeError):
        SimTime(1, "ns") - 10  # type: ignore
    with pytest.raises(TypeError):
        object() - SimTime(1, "ns")  # type: ignore
    with pytest.raises(TypeError):
        SimTime(1, "ns") * "123"  # type: ignore
    with pytest.raises(TypeError):
        "123" * SimTime(1, "ns")  # type: ignore
    with pytest.raises(TypeError):
        SimTime(1, "ns") / {}  # type: ignore
    with pytest.raises(TypeError):
        SimTime(1, "ns") // TypeError()  # type: ignore

    # repr
    assert repr(SimTime(1, "ns")) == "SimTime(1.0, 'ns')"
    assert repr(SimTime(2067, "ns")) == "SimTime(2.067, 'us')"
    assert repr(-SimTime(1, "ns")) == "-SimTime(1.0, 'ns')"
    assert repr(SimTime(100000, "sec")) == "SimTime(100000.0, 'sec')"

    # mul and div
    assert SimTime(1, "ns").mul(2.0000000000001, round_mode="floor") == SimTime(
        2.0, "ns"
    )
    assert SimTime(1, "step").mul(1.1, round_mode="ceil") == SimTime(2, "step")
    assert SimTime(1, "ps").div(3, round_mode="floor") == SimTime(333, "fs")

    await with_timeout(SimTime(20, "ns").wait(), 20.001, "ns")

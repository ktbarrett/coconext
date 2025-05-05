from __future__ import annotations

from typing import Any

import cocotb
import pytest

from coconext.simtime import SimFrequency, SimTime


@cocotb.test
async def test_simtime(_: Any) -> None:
    # constructing SimTime
    assert SimTime(1, "fs").fs == 1
    assert SimTime(5, "ps").ps == 5
    assert SimTime(47, "ns").ns == 47
    assert SimTime(25, "us").us == 25
    assert SimTime(9, "ms").ms == 9
    assert SimTime(5, "sec").sec == 5
    assert SimTime(1, "step").step == 1

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

    # arithmetic
    assert SimTime(1, "ns") + SimTime(1, "ps") == SimTime(1001, "ps")
    assert SimTime(1, "ns") - SimTime(2, "ns") == SimTime(-1, "ns")
    assert SimTime(500, "ns") * 2 == SimTime(1, "us")
    assert 5 * SimTime(200, "ns") == SimTime(1, "us")
    assert -SimTime(10, "ns") == SimTime(-0.01, "us")
    with pytest.raises(ValueError):
        assert SimTime(1, "ns") * 0.0000000001

    # repr
    assert repr(SimTime(1, "ns")) == "SimTime(1.0, 'ns')"
    assert repr(SimTime(2067, "ns")) == "SimTime(2.067, 'us')"
    assert repr(-SimTime(1, "ns")) == "-SimTime(1.0, 'ns')"

    # rounding mul
    assert SimTime(1, "ns").mul(2.0000000000001, round_mode="floor") == SimTime(
        2.0, "ns"
    )
    assert SimTime(1, "step").mul(1.1, round_mode="ceil") == SimTime(2, "step")

    # constructing SimFrequency
    assert SimFrequency(1, "Hz").Hz == 1
    assert SimFrequency(2, "kHz").kHz == 2
    assert SimFrequency(4, "MHz").MHz == 4
    assert SimFrequency(8, "GHz").GHz == 8
    with pytest.raises(ValueError):
        SimFrequency(-1, "GHz")

    # converting units
    assert SimFrequency(1, "GHz").Hz == pytest.approx(1_000_000_000)
    assert SimFrequency(1, "MHz").kHz == pytest.approx(1_000)
    assert SimFrequency(200, "kHz").GHz == pytest.approx(0.0002)

    # repr
    assert repr(SimFrequency(1, "GHz")) == "SimFrequency(1.0, 'GHz')"
    assert repr(SimFrequency(12000, "kHz")) == "SimFrequency(12.0, 'MHz')"

"""Collections of tools for dealing with simulation time in cocotb."""

from __future__ import annotations

import sys
from functools import cached_property
from typing import Literal

from cocotb.triggers import Timer
from cocotb.utils import get_sim_steps, get_time_from_sim_steps

if sys.version_info >= (3, 10):
    from typing import TypeAlias

TimeUnit: TypeAlias = Literal["fs", "ps", "ns", "us", "ms", "sec", "step"]
RoundMode: TypeAlias = Literal["error", "round", "ceil", "floor"]

_UNITS: list[TimeUnit] = ["fs", "ps", "ns", "us", "ms", "sec"]


class SimTime:
    """Type for dealing with simulated time.

    :class:`!SimTime` is time quantized to simulator steps.
    If you can create a :class:`!SimTime`, you have a time that can be represented by the simulator.
    The type also includes a collection of functionality commonly needed when dealing with time,
    such as comparison, scaling, and arithmetic.
    """

    def __init__(
        self, time: float, unit: TimeUnit, *, round_mode: RoundMode = "error"
    ) -> None:
        """Construct a SimTime of *time* value in *unit*.

        Args:
            time: The time value.
            unit: Unit for *time* argument.
            round_mode:
                How to round the time if it can't be accurately represented.
                One of ``"error"``, ``"round"``, ``"ceil"``, and ``"floor"``.
                See :func:`cocotb.utils.get_sim_steps` for details on these values.

        """
        self._steps = get_sim_steps(time, unit, round_mode=round_mode)

    def in_unit(self, unit: TimeUnit) -> float:
        """Return time in the given unit."""
        return get_time_from_sim_steps(self._steps, unit)

    @property
    def fs(self) -> float:
        """Time in femtoseconds."""
        return self.in_unit("fs")

    @property
    def ps(self) -> float:
        """Time in picoseconds."""
        return self.in_unit("ps")

    @property
    def ns(self) -> float:
        """Time in nanoseconds."""
        return self.in_unit("ns")

    @property
    def us(self) -> float:
        """Time in microseconds."""
        return self.in_unit("us")

    @property
    def ms(self) -> float:
        """Time in milliseconds."""
        return self.in_unit("ms")

    @property
    def sec(self) -> float:
        """Time in seconds."""
        return self.in_unit("sec")

    @property
    def step(self) -> float:
        """Time in simulator steps."""
        return self.in_unit("step")

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, SimTime):
            return NotImplemented
        return self._steps == other._steps

    def __lt__(self, other: SimTime) -> bool:
        if not isinstance(other, SimTime):
            return NotImplemented
        return self._steps < other._steps

    def __le__(self, other: SimTime) -> bool:
        if not isinstance(other, SimTime):
            return NotImplemented
        return self._steps <= other._steps

    def __gt__(self, other: SimTime) -> bool:
        if not isinstance(other, SimTime):
            return NotImplemented
        return self._steps > other._steps

    def __ge__(self, other: SimTime) -> bool:
        if not isinstance(other, SimTime):
            return NotImplemented
        return self._steps >= other._steps

    def __hash__(self) -> int:
        return hash(self._steps)

    def __repr__(self) -> str:
        return self._repr

    @cached_property
    def _repr(self) -> str:
        for unit in _UNITS:
            time = self.in_unit(unit)
            if abs(time) < 1000:
                break
        if time < 0:
            prefix = "-"
            time = -time
        else:
            prefix = ""
        return f"{prefix}{type(self).__qualname__}({time}, {unit!r})"

    def __neg__(self) -> SimTime:
        return SimTime(-self._steps, "step")

    def __add__(self, other: SimTime) -> SimTime:
        if not isinstance(other, SimTime):
            return NotImplemented
        return SimTime(self._steps + other._steps, "step")

    def __sub__(self, other: SimTime) -> SimTime:
        if not isinstance(other, SimTime):
            return NotImplemented
        return SimTime(self._steps - other._steps, "step")

    def __mul__(self, other: float) -> SimTime:
        if not isinstance(other, (float, int)):
            return NotImplemented
        return SimTime(self._steps * other, "step")

    def __rmul__(self, other: float) -> SimTime:
        return self * other

    def __truediv__(self, other: float) -> SimTime:
        if not isinstance(other, (float, int)):
            return NotImplemented
        return SimTime(self._steps / other, "step")

    def __floordiv__(self, other: float) -> SimTime:
        if not isinstance(other, (float, int)):
            return NotImplemented
        return SimTime(self._steps // other, "step")

    def mul(self, other: float, *, round_mode: RoundMode = "error") -> SimTime:
        """Scale the time via multiplication and supply a *round_mode*."""
        return SimTime(self._steps * other, "step", round_mode=round_mode)

    def div(self, other: float, *, round_mode: RoundMode = "error") -> SimTime:
        """Scale the time via division and supply a *round_mode*."""
        return SimTime(self._steps / other, "step", round_mode=round_mode)

    def wait(self) -> Timer:
        """Return a :class:`.Timer` Trigger of the time."""
        return Timer(self._steps, "step")

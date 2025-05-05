"""Types for dealing with simulation time."""

from __future__ import annotations

import sys
from typing import Any, Literal, overload

from cocotb.triggers import Timer
from cocotb.utils import get_sim_steps, get_time_from_sim_steps

from coconext._utils import cached_method

if sys.version_info >= (3, 11):
    from typing import Self, TypeAlias

TimeUnit: TypeAlias = Literal["fs", "ps", "ns", "us", "ms", "sec", "step"]
FreqUnit: TypeAlias = Literal["Hz", "kHz", "MHz", "GHz"]
RoundMode: TypeAlias = Literal["error", "round", "ceil", "floor"]

_UNITS: list[TimeUnit] = ["fs", "ps", "ns", "us", "ms", "sec"]
_FREQS: list[FreqUnit] = ["GHz", "MHz", "kHz", "Hz"]
_FREQ_TO_TIME: dict[FreqUnit, TimeUnit] = {
    "Hz": "sec",
    "kHz": "ms",
    "MHz": "us",
    "GHz": "ns",
}
_TIME_TO_FREQ: dict[TimeUnit, FreqUnit] = {
    val: key for key, val in _FREQ_TO_TIME.items()
}


class SimTime:
    """TODO."""

    def __init__(
        self, time: float, unit: TimeUnit, *, round_mode: str = "error"
    ) -> None:
        """TODO."""
        self._steps = get_sim_steps(time, unit, round_mode=round_mode)

    def in_unit(self, unit: TimeUnit) -> float:
        """Return time in the given unit."""
        return get_time_from_sim_steps(self._steps, unit)

    @property
    def fs(self) -> float:
        """Return time in femtoseconds."""
        return self.in_unit("fs")

    @property
    def ps(self) -> float:
        """Return time in picoseconds."""
        return self.in_unit("ps")

    @property
    def ns(self) -> float:
        """Return time in nanoseconds."""
        return self.in_unit("ns")

    @property
    def us(self) -> float:
        """Return time in microseconds."""
        return self.in_unit("us")

    @property
    def ms(self) -> float:
        """Return time in milliseconds."""
        return self.in_unit("ms")

    @property
    def sec(self) -> float:
        """Return time in seconds."""
        return self.in_unit("sec")

    @property
    def step(self) -> float:
        """Return time in simulator steps."""
        return self.in_unit("step")

    def __eq__(self, other: Any) -> bool:
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

    @cached_method
    def __repr__(self) -> str:  # noqa: D105
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

    def __rtruediv__(self, other: float) -> SimFrequency:
        if not isinstance(other, (float, int)):
            return NotImplemented
        return SimFrequency.from_period(self._steps * other, "step")

    def __floordiv__(self, other: float) -> SimTime:
        if not isinstance(other, (float, int)):
            return NotImplemented
        return SimTime(self._steps // other, "step")

    def mul(self, other: float, *, round_mode: RoundMode = "error") -> SimTime:
        return SimTime(self._steps * other, "step", round_mode=round_mode)

    def div(self, other: float, *, round_mode: RoundMode = "error") -> SimTime:
        return SimTime(self._steps / other, "step", round_mode=round_mode)

    def wait(self) -> Timer:
        return Timer(self._steps, "step")


class SimFrequency:
    def __init__(
        self, freq: float, unit: FreqUnit, *, round_mode: RoundMode = "round"
    ) -> None:
        if freq < 0:
            raise ValueError("Frequency can't be negative")
        self._period = get_sim_steps(
            1 / freq, _FREQ_TO_TIME[unit], round_mode=round_mode
        )

    @overload
    @classmethod
    def from_period(
        cls, time: float, unit: TimeUnit, *, round_mode: RoundMode = "error"
    ) -> Self: ...

    @overload
    @classmethod
    def from_period(cls, time: SimTime) -> Self: ...

    @classmethod
    def from_period(
        cls,
        time: float | SimTime,
        unit: TimeUnit | None = None,
        *,
        round_mode: RoundMode = "error",
    ) -> Self:
        self = cls.__new__(cls)
        if isinstance(time, SimTime):
            if time._steps < 0:
                raise ValueError("Period can't be negative")
            self._period = time._steps
        else:
            if unit is None:
                raise TypeError("Missing required argument: 'unit'")
            self._period = get_sim_steps(time, unit, round_mode=round_mode)
        return self

    def in_unit(self, unit: FreqUnit) -> float:
        time_unit = _FREQ_TO_TIME[unit]
        return 1 / get_time_from_sim_steps(self._period, time_unit)

    @property
    def Hz(self) -> float:
        """Return frequency in hertz."""
        return self.in_unit("Hz")

    @property
    def kHz(self) -> float:
        """Return frequency in kilohertz."""
        return self.in_unit("kHz")

    @property
    def MHz(self) -> float:
        """Return frequency in megahertz."""
        return self.in_unit("MHz")

    @property
    def GHz(self) -> float:
        """Return frequency in gigahertz."""
        return self.in_unit("GHz")

    @cached_method
    def __repr__(self) -> str:  # noqa: D105
        for unit in _FREQS:
            time = self.in_unit(unit)
            if time >= 1.0:
                break
        return f"{type(self).__qualname__}({time}, {unit!r})"

    def __mul__(self, other: float) -> SimFrequency:
        if not isinstance(other, (float, int)):
            return NotImplemented
        return SimFrequency.from_period(self._period / other, "step")

    def __rmul__(self, other: float) -> SimFrequency:
        return self * other

    def __truediv__(self, other: float) -> SimFrequency:
        if not isinstance(other, (float, int)):
            return NotImplemented
        return SimFrequency.from_period(self._period * other, "step")

    def __rtruediv__(self, other: float) -> SimTime:
        if not isinstance(other, (float, int)):
            return NotImplemented
        return SimTime(self._period * other, "step")

    def mul(self, other: float, *, round_mode: RoundMode = "error") -> SimFrequency:
        return SimFrequency.from_period(
            self._period / other, "step", round_mode=round_mode
        )

    def div(self, other: float, *, round_mode: RoundMode = "error") -> SimFrequency:
        return SimFrequency.from_period(
            self._period * other, "step", round_mode=round_mode
        )

    def period(self) -> SimTime:
        return SimTime(self._period, "step")

    def __eq__(self, other: Any) -> bool:
        if not isinstance(other, SimFrequency):
            return NotImplemented
        return self._period == other._period

    def __lt__(self, other: SimFrequency) -> bool:
        if not isinstance(other, SimFrequency):
            return NotImplemented
        return self._period >= other._period

    def __le__(self, other: SimFrequency) -> bool:
        if not isinstance(other, SimFrequency):
            return NotImplemented
        return self._period > other._period

    def __gt__(self, other: Any) -> bool:
        if not isinstance(SimFrequency, SimFrequency):
            return NotImplemented
        return self._period <= other._period

    def __ge__(self, other: SimFrequency) -> bool:
        if not isinstance(other, SimFrequency):
            return NotImplemented
        return self._period < other._period

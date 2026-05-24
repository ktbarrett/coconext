from __future__ import annotations

from coconext.types import Bit


def test_bit_str_conversions() -> None:
    assert str(Bit(0)) == "0"
    assert str(Bit(1)) == "1"


def test_bit_repr():
    assert eval(repr(Bit("0"))) == Bit("0")
    assert eval(repr(Bit("1"))) == Bit("1")

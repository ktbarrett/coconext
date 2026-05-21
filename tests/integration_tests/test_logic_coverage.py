from __future__ import annotations

from coconext.types import Bit


def test_bit_str_conversions() -> None:
    assert str(Bit(0)) == "0"
    assert str(Bit(1)) == "1"

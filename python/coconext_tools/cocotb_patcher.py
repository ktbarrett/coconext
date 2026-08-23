"""Monkey patch cocotb to test cocotb's regression on coconext's API."""

from __future__ import annotations

import coconext.types
import cocotb.types


def patch_cocotb() -> None:
    """Minimal patch based on current use."""
    setattr(cocotb.types, "Range", coconext.types.Range)
    setattr(cocotb.types, "Bit", coconext.types.Bit)
    setattr(cocotb.types, "Logic", coconext.types.Logic)
    setattr(cocotb.types, "LogicArray", coconext.types.LogicArray)
    setattr(cocotb.types, "BitArray", coconext.types.BitArray)

"""Collection of modeling types for coconext."""

from __future__ import annotations

from _pycoconext import (
    Bit,
    BitArray,
    Direction,
    Logic,
    LogicArray,
    Range,
    Sfixed,
    Signed,
    Ufixed,
    Unsigned,
)

__all__ = (
    "Bit",
    "BitArray",
    "Direction",
    "Logic",
    "LogicArray",
    "Range",
    "Sfixed",
    "Signed",
    "Ufixed",
    "Unsigned",
)

# fixup __module__
for obj_name in __all__:
    globals()[obj_name].__module__ = __name__

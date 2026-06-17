"""Collection of modeling types for coconext."""

from __future__ import annotations

from _pycoconext import Bit, Direction, Logic, Range

__all__ = (
    "Bit",
    "Direction",
    "Logic",
    "Range",
)

# fixup __module__
for obj_name in __all__:
    globals()[obj_name].__module__ = __name__

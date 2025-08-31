"""Collection of data types inspired by common types used in hardware design."""

from __future__ import annotations

from ._bit_array import BitArray

__all__ = ("BitArray",)

for obj in __all__:
    globals()[obj].__module__ = __name__

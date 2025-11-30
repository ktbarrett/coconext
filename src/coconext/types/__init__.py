"""Collection of types modelled after those common in HDLs."""

from __future__ import annotations

from ._bit_array import BitArray

__all__ = ("BitArray",)

for _name in __all__:
    globals()[_name].__module__ = __name__

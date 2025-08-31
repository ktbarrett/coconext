from __future__ import annotations

import copy
from math import ceil
from typing import TYPE_CHECKING, cast, overload

from cocotb.types import AbstractMutableArray, Logic, LogicArray, Range

if TYPE_CHECKING:
    from typing import Any, Iterable, Literal, Self


class BitArray(AbstractMutableArray[Logic]):
    _value: int
    _range: Range

    __slots__ = ("_range", "_value")

    def __init__(
        self,
        value: str | Iterable[str | int | bool | Logic] | BitArray | LogicArray,
        range: Range | None = None,
    ) -> None:
        if not isinstance(range, (Range, type(None))):
            raise TypeError("'range' must be a Range object or None")

        if isinstance(value, BitArray):
            # must normalize to unsigned in case repr is signed
            self._value = value._value % (1 << len(value))
            self._range = value.range
            if range is not None:
                self.range = range
        elif isinstance(value, LogicArray):
            try:
                self._value = int(value)
            except ValueError:
                raise ValueError("initializer contains non-0/1 elements") from None
            self._range = value.range
            if range is not None:
                self.range = range
        else:
            if isinstance(value, str):
                try:
                    self._value = int(value, 2)
                except ValueError:
                    raise ValueError("str literal contains non-0/1 elements") from None
            else:
                try:
                    value = "".join(str(Logic(v)) for v in value)
                    self._value = int(value, 2)
                except ValueError:
                    raise ValueError("initializer contains non-0/1 elements") from None
            if range is None:
                self._range = Range(len(value) - 1, "downto", 0)
            elif len(range) == len(value):
                self._range = range
            else:
                raise ValueError(
                    "'range' argument does not match length of 'value' argument"
                )

    @classmethod
    def _make(cls, value: int, range: Range) -> Self:
        self = cls.__new__(cls)
        self._value = value
        self._range = range
        return self

    @property
    def range(self) -> Range:
        return self._range

    @range.setter
    def range(self, new_range: Range) -> None:
        if len(new_range) != len(self._range):
            raise ValueError("new range must be same size old range")
        self._range = new_range

    def __str__(self) -> str:
        return f"{self._value:0{len(self)}b}"

    def __repr__(self) -> str:
        return f"{type(self).__qualname__}({str(self)!r}, {self._range!r})"

    def __eq__(self, other: object) -> bool:
        if isinstance(other, BitArray):
            return self._value == other._value
        elif isinstance(other, (str, LogicArray)):
            return str(self) == str(other)
        return NotImplemented

    __hash__: None  # type: ignore

    def _translate_index(self, idx: int) -> int:
        if self.range.direction == "to":
            if self.left <= idx <= self.right:
                return self.right - idx
        elif self.left >= idx >= self.right:
            return idx - self.right
        raise IndexError(f"index {idx} not in range")

    @overload
    def __getitem__(self, idx: int) -> Logic: ...

    @overload
    def __getitem__(self, idx: slice) -> BitArray: ...

    def __getitem__(self, idx: int | slice) -> Logic | BitArray:
        if isinstance(idx, int):
            idx = self._translate_index(idx)
            return Logic((self._value >> idx) & 1)
        elif isinstance(idx, slice):
            start = idx.start if idx.start is not None else self.left
            stop = idx.stop if idx.stop is not None else self.right
            if idx.step is not None:
                raise TypeError("do not specify step")
            start_i = self._translate_index(start) + 1  # +1 because it's inclusive
            stop_i = self._translate_index(stop)
            if stop_i >= start_i:
                raise IndexError(
                    f"slice [{start}:{stop}] direction does not match array direction [{self.left}:{self.right}]"
                )
            value = (self._value & ((1 << start_i) - 1)) >> stop_i
            range = Range(start, self.direction, stop)
            return BitArray._make(value=value, range=range)
        else:
            raise TypeError(f"Expected int or slice, not {type(idx).__qualname__}")

    @overload
    def __setitem__(self, idx: int, value: str | Logic) -> None: ...

    @overload
    def __setitem__(
        self,
        idx: slice,
        value: str | Iterable[str | Logic] | BitArray | LogicArray,
    ) -> None: ...

    def __setitem__(
        self,
        idx: int | slice,
        value: Logic | str | Iterable[str | Logic] | BitArray | LogicArray,
    ) -> None:
        if isinstance(idx, int):
            idx = self._translate_index(idx)
            if not isinstance(value, Logic):
                value = Logic(cast("str | Logic", value))
            self._value &= ~(1 << idx)
            self._value |= int(value) << idx
        elif isinstance(idx, slice):
            start = idx.start if idx.start is not None else self.left
            stop = idx.stop if idx.stop is not None else self.right
            if idx.step is not None:
                raise TypeError("do not specify step")
            start_i = self._translate_index(start) + 1  # +1 because it's inclusive
            stop_i = self._translate_index(stop)
            if stop_i >= start_i:
                raise IndexError(
                    f"slice [{start}:{stop}] direction does not match array direction [{self.left}:{self.right}]"
                )
            if not isinstance(value, BitArray):
                value = BitArray(
                    cast("str | Iterable[str | Logic] | LogicArray", value)
                )
            mask = ~(((1 << (start_i - stop_i)) - 1) << stop_i)
            self._value &= mask
            self._value |= value._value << stop_i
        else:
            raise TypeError(f"Expected int or slice, not {type(idx).__qualname__}")

    @classmethod
    def from_bytes(
        cls,
        value: bytes | bytearray,
        range: Range | None = None,
        *,
        byteorder: Literal["little", "big"],
    ) -> BitArray:
        """Construct a :class:`!BitArray` from :class:`bytes`.

        The :class:`bytes` is first converted to an unsigned integer using *byteorder*-endian representation,
        then is converted to a :class:`!BitArray`.

        Args:
            value: The bytes to convert.
            range: Indexing scheme for the BitArray.
            byteorder: The endianness used to construct the intermediate integer, either ``"big"`` or ``"little"``.

        Returns:
            A :class:`!BitArray` equivalent to the *value*.

        Raises:
            ValueError: When a :class:`!BitArray` of the given *range* can't hold the *value*.
        """
        if range is None:
            range = Range(len(value) * 8 - 1, "downto", 0)
        elif len(value) * 8 != len(range):
            raise ValueError(
                f"Value of length {len(value)} will not fit in a LogicArray with bounds: {range!r}"
            )
        value_as_int = int.from_bytes(value, byteorder=byteorder, signed=False)
        return cls._make(value_as_int, range)

    def to_bytes(
        self,
        *,
        byteorder: Literal["little", "big"],
    ) -> bytes:
        """Convert the value to bytes.

        The :class:`!BitArray` is converted to an unsigned integer,
        then is converted to :class:`bytes` using *byteorder*-endian representation
        with the minimum number of bytes which can store all the bits in the original :class:`!BitArray`.

        Args:
            byteorder: The endianness used to construct the intermediate integer, either ``"big"`` or ``"little"``.

        Returns:
            :class:`bytes` equivalent to the value.
        """
        return self._value.to_bytes(ceil(len(self) / 8), byteorder=byteorder)

    @overload
    def __and__(self, other: str | BitArray) -> BitArray: ...

    @overload
    def __and__(self, other: LogicArray) -> LogicArray: ...

    def __and__(self, other: str | BitArray | LogicArray) -> BitArray | LogicArray:
        res_type: type[BitArray | LogicArray]
        if isinstance(other, str):
            other = BitArray(other)
            res_type = BitArray
        elif isinstance(other, BitArray):
            res_type = BitArray
        elif isinstance(other, LogicArray):
            res_type = LogicArray
        else:
            return NotImplemented

        if len(self) != len(other):
            raise ValueError("bitwise & not possible between arrays of different sizes")
        return res_type(a & b for a, b in zip(self, other))

    @overload
    def __rand__(self, other: str) -> BitArray: ...

    @overload
    def __rand__(self, other: LogicArray) -> LogicArray: ...

    def __rand__(self, other: str | LogicArray) -> BitArray | LogicArray:
        return self & other

    @overload
    def __or__(self, other: str | BitArray) -> BitArray: ...

    @overload
    def __or__(self, other: LogicArray) -> LogicArray: ...

    def __or__(self, other: str | BitArray | LogicArray) -> BitArray | LogicArray:
        res_type: type[BitArray | LogicArray]
        if isinstance(other, str):
            other = BitArray(other)
            res_type = BitArray
        elif isinstance(other, BitArray):
            res_type = BitArray
        elif isinstance(other, LogicArray):
            res_type = LogicArray
        else:
            return NotImplemented

        if len(self) != len(other):
            raise ValueError("bitwise | not possible between arrays of different sizes")
        return res_type(a | b for a, b in zip(self, other))

    @overload
    def __ror__(self, other: str) -> BitArray: ...

    @overload
    def __ror__(self, other: LogicArray) -> LogicArray: ...

    def __ror__(self, other: str | LogicArray) -> BitArray | LogicArray:
        return self | other

    @overload
    def __xor__(self, other: str | BitArray) -> BitArray: ...

    @overload
    def __xor__(self, other: LogicArray) -> LogicArray: ...

    def __xor__(self, other: str | BitArray | LogicArray) -> BitArray | LogicArray:
        res_type: type[BitArray | LogicArray]
        if isinstance(other, str):
            other = BitArray(other)
            res_type = BitArray
        elif isinstance(other, BitArray):
            res_type = BitArray
        elif isinstance(other, LogicArray):
            res_type = LogicArray
        else:
            return NotImplemented

        if len(self) != len(other):
            raise ValueError("bitwise ^ not possible between arrays of different sizes")
        return res_type(a ^ b for a, b in zip(self, other))

    @overload
    def __rxor__(self, other: str) -> BitArray: ...

    @overload
    def __rxor__(self, other: LogicArray) -> LogicArray: ...

    def __rxor__(self, other: str | LogicArray) -> BitArray | LogicArray:
        return self ^ other

    def __invert__(self) -> Self:
        return type(self)._make(~self._value % (1 << len(self)), self._range)

    def __copy__(self) -> Self:
        return type(self)._make(self._value, self._range)

    def __deepcopy__(self, memo: dict[int, Any]) -> Self:
        return type(self)._make(self._value, copy.deepcopy(self._range, memo=memo))

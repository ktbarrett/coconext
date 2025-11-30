from __future__ import annotations

from collections.abc import Iterable, Iterator
from typing import cast, overload

from cocotb._utils import cached_method
from cocotb.types import AbstractMutableArray, Bit, Logic, LogicArray, Range


class BitArray(AbstractMutableArray[Bit]):
    """An Array of Bits."""

    __slots__ = ("_range", "_value")

    _value: int
    _range: Range

    @overload
    def __init__(self, value: int, range: Range | int) -> None: ...
    @overload
    def __init__(
        self,
        value: LogicArray | BitArray | str | Iterable[Logic | Bit | str | int],
        range: Range | int | None = None,
    ) -> None: ...
    def __init__(
        self,
        value: LogicArray | BitArray | str | Iterable[Logic | Bit | str | int] | int,
        range: Range | int | None = None,
    ) -> None:
        if isinstance(range, int):
            range = Range(range - 1, 0)
        if isinstance(value, int):
            if range is None:
                raise TypeError("Range must be provided when initializing from int.")
            self._range = range
            if 0 <= value < (1 << len(self)):
                self._value = value
            else:
                raise ValueError(
                    "Integer value does not fit in a BitArray for the given range."
                )
        elif isinstance(value, str):
            if range is not None:
                if len(range) != len(value):
                    raise ValueError("Provided range does not match value length.")
                self._range = range
            else:
                self._range = Range(len(value) - 1, 0)
            try:
                self._value = int(value, 2)
            except ValueError:
                raise ValueError(
                    "Cannot convert string to BitArray, non-0/1 characters are present."
                ) from None
        elif isinstance(value, (LogicArray, BitArray)):
            if range is not None:
                if len(range) != len(value):
                    raise ValueError("Provided range does not match value length.")
                self._range = range
            else:
                self._range = value.range
            try:
                self._value = int(value)
            except ValueError:
                raise ValueError(
                    "Cannot convert LogicArray to BitArray, non-0/1 values are present."
                ) from None
        else:
            bits = [Bit(v) for v in value]
            if range is not None:
                if len(range) != len(bits):
                    raise ValueError("Provided range does not match value length.")
                self._range = range
            else:
                self._range = Range(len(bits) - 1, 0)
            self._value = 0
            for bit in bits:
                self._value = (self._value << 1) | int(bit)

    @property
    def range(self) -> Range:
        return self._range

    @range.setter
    def range(self, value: Range) -> None:
        if len(value) != len(self):
            raise ValueError("New range must match the length of the BitArray.")
        self._range = value

    def __repr__(self) -> str:
        return f"BitArray({str(self)!r}, {self.range})"

    def __str__(self) -> str:
        return f"{self._value:0{len(self)}b}"

    def __int__(self) -> int:
        return self._value

    def __bool__(self) -> bool:
        return bool(self._value)

    def __eq__(self, other: object) -> bool:
        if isinstance(other, (BitArray, LogicArray, str)):
            return len(other) == len(self) and str(self) == str(other)
        elif isinstance(other, int):
            return self._value == other
        else:
            return NotImplemented

    def __iter__(self) -> Iterator[Bit]:
        val = self._value
        for i in range(len(self) - 1, -1, -1):
            yield Bit((val >> i) & 1)

    def __reversed__(self) -> Iterator[Bit]:
        val = self._value
        for _ in range(len(self)):
            yield Bit(val & 1)
            val >>= 1

    @cached_method
    def _get_bit_idx(self, user_idx: int) -> int:
        """Convert a user index to an internal bit index."""
        if user_idx not in self.range:
            raise IndexError("Index out of range.")
        return len(self) - 1 - self.range.index(user_idx)

    @overload
    def __getitem__(self, key: int) -> Bit: ...
    @overload
    def __getitem__(self, key: slice) -> BitArray: ...
    def __getitem__(self, key: int | slice) -> Bit | BitArray:
        if isinstance(key, int):
            idx = self._get_bit_idx(key)
            bit_value = (self._value >> idx) & 1
            return Bit(bit_value)
        elif isinstance(key, slice):
            if key.step is not None and key.step != 1:
                raise ValueError("Slicing with step is not supported.")
            if key.start is None:
                start_user = self.range.left
                msb = len(self) - 1
            else:
                start_user = key.start
                msb = self._get_bit_idx(key.start)
            if key.stop is None:
                stop_user = self.range.right
                lsb = 0
            else:
                stop_user = key.stop
                lsb = self._get_bit_idx(key.stop)
            if msb <= lsb:
                raise ValueError("Slice in opposite direction of the array.")
            slice_length = msb - lsb + 1
            slice_mask = (1 << slice_length) - 1
            sliced_value = (self._value >> lsb) & slice_mask
            return BitArray(sliced_value, Range(start_user, self.direction, stop_user))
        else:
            raise TypeError("BitArray indices must be ints or slices.")

    @overload
    def __setitem__(
        self,
        key: int,
        value: Bit | Logic | int | str,
    ) -> None: ...
    @overload
    def __setitem__(
        self,
        key: slice,
        value: int | str | LogicArray | BitArray | Iterable[Logic | Bit | str | int],
    ) -> None: ...
    def __setitem__(
        self,
        key: int | slice,
        value: Bit
        | Logic
        | int
        | str
        | LogicArray
        | BitArray
        | Iterable[Logic | Bit | str | int],
    ) -> None:
        if isinstance(key, int):
            if key not in self.range:
                raise IndexError("BitArray index out of range")
            idx = self._get_bit_idx(key)
            value = Bit(cast("Logic | Bit | int | str", value))
            if value:
                self._value |= 1 << idx
            else:
                self._value &= ~(1 << idx)
        elif isinstance(key, slice):
            if key.step is not None and key.step != 1:
                raise ValueError("Slicing with step is not supported.")
            msb = (
                self._get_bit_idx(key.start) if key.start is not None else len(self) - 1
            )
            lsb = self._get_bit_idx(key.stop) if key.stop is not None else 0
            if msb <= lsb:
                raise ValueError("Slice in opposite direction of the array.")
            slice_length = msb - lsb + 1
            value = BitArray(
                cast(
                    "int | str | LogicArray | Iterable[Logic | Bit | str | int]",
                    value,
                ),
                slice_length,
            )
            value_int = int(value)
            slice_mask = ((1 << slice_length) - 1) << lsb
            self._value = (self._value & ~slice_mask) | (value_int << lsb)
        else:
            raise TypeError("BitArray indices must be ints or slices.")

    @overload
    def __and__(self, other: BitArray | str) -> BitArray: ...
    @overload
    def __and__(self, other: LogicArray) -> LogicArray: ...
    @overload
    def __and__(self, other: int) -> int: ...
    def __and__(
        self, other: BitArray | LogicArray | str | int
    ) -> BitArray | int | LogicArray:
        if isinstance(other, str):
            other = BitArray(other)
        if isinstance(other, BitArray):
            if len(other) != len(self):
                raise ValueError(
                    "BitArrays must be the same length for bitwise operations."
                )
            return BitArray(int(self) & int(other), self.range)
        elif isinstance(other, int):
            return int(self) & other
        elif isinstance(other, LogicArray):
            return LogicArray(self) & other
        else:
            return NotImplemented

    @overload
    def __rand__(self, other: str) -> BitArray: ...
    @overload
    def __rand__(self, other: LogicArray) -> LogicArray: ...
    @overload
    def __rand__(self, other: int) -> int: ...
    def __rand__(self, other: LogicArray | str | int) -> BitArray | int | LogicArray:
        return self & other

    @overload
    def __or__(self, other: BitArray | str) -> BitArray: ...
    @overload
    def __or__(self, other: LogicArray) -> LogicArray: ...
    @overload
    def __or__(self, other: int) -> int: ...
    def __or__(
        self, other: BitArray | LogicArray | str | int
    ) -> BitArray | int | LogicArray:
        if isinstance(other, str):
            other = BitArray(other)
        if isinstance(other, BitArray):
            if len(other) != len(self):
                raise ValueError(
                    "BitArrays must be the same length for bitwise operations."
                )
            return BitArray(int(self) | int(other), self.range)
        elif isinstance(other, int):
            return int(self) | other
        elif isinstance(other, LogicArray):
            return LogicArray(self) | other
        else:
            return NotImplemented

    @overload
    def __ror__(self, other: str) -> BitArray: ...
    @overload
    def __ror__(self, other: LogicArray) -> LogicArray: ...
    @overload
    def __ror__(self, other: int) -> int: ...
    def __ror__(self, other: LogicArray | str | int) -> BitArray | int | LogicArray:
        return self | other

    @overload
    def __xor__(self, other: BitArray | str) -> BitArray: ...
    @overload
    def __xor__(self, other: LogicArray) -> LogicArray: ...
    @overload
    def __xor__(self, other: int) -> int: ...
    def __xor__(
        self, other: BitArray | LogicArray | str | int
    ) -> BitArray | int | LogicArray:
        if isinstance(other, str):
            other = BitArray(other)
        if isinstance(other, BitArray):
            if len(other) != len(self):
                raise ValueError(
                    "BitArrays must be the same length for bitwise operations."
                )
            return BitArray(int(self) ^ int(other), self.range)
        elif isinstance(other, int):
            return int(self) ^ other
        elif isinstance(other, LogicArray):
            return LogicArray(self) ^ other
        else:
            return NotImplemented

    @overload
    def __rxor__(self, other: str) -> BitArray: ...
    @overload
    def __rxor__(self, other: LogicArray) -> LogicArray: ...
    @overload
    def __rxor__(self, other: int) -> int: ...
    def __rxor__(self, other: LogicArray | str | int) -> BitArray | int | LogicArray:
        return self ^ other

    def __invert__(self) -> BitArray:
        return BitArray(~int(self) % (1 << len(self)), self.range)

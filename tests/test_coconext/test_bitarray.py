from __future__ import annotations

import copy

import pytest
from cocotb.types import Logic, LogicArray, Range

from coconext.types import BitArray


def test_construct() -> None:
    BitArray("10101010")
    BitArray("1010", Range(1, 4))
    BitArray([1, "0", Logic(0), False])
    BitArray([1, "0", Logic(0), False], Range(100, 97))
    BitArray(LogicArray("1111"))
    BitArray(LogicArray("1111"), Range(1, 4))
    BitArray(BitArray("0000"))
    BitArray(BitArray("0000"), Range(1, 4))
    with pytest.raises(ValueError):
        BitArray("1010", Range(7, 0))
    with pytest.raises(ValueError):
        BitArray([1, "0", Logic(0), False], Range(7, 0))
    with pytest.raises(ValueError):
        BitArray(LogicArray("1111"), Range(7, 0))
    with pytest.raises(ValueError):
        BitArray(BitArray("0000"), Range(7, 0))
    with pytest.raises(ValueError):
        BitArray("1234")
    with pytest.raises(ValueError):
        BitArray([Logic("X"), 0, 1, 0])
    with pytest.raises(ValueError):
        BitArray(LogicArray("01XZ"))
    with pytest.raises(TypeError):
        BitArray(0)  # type: ignore
    with pytest.raises(TypeError):
        BitArray("0101", object())  # type: ignore


def test_attributes() -> None:
    b = BitArray("0101")
    assert b.range == Range(3, "downto", 0)
    assert b.left == 3
    assert b.direction == "downto"
    assert b.right == 0

    b = BitArray("0101", Range(5, 8))
    assert b.range == Range(5, "to", 8)
    assert b.left == 5
    assert b.direction == "to"
    assert b.right == 8

    with pytest.raises(ValueError):
        b.range = Range(100, 0)
    b.range = Range(31, 28)
    assert b.range == Range(31, "downto", 28)


def test_convert_and_repr() -> None:
    b = BitArray("0101")
    assert str(b) == "0101"
    assert repr(b) == "BitArray('0101', Range(3, 'downto', 0))"


def test_eq() -> None:
    assert BitArray("0000") == "0000"
    assert BitArray("0000") != "0101"
    assert "1111" == BitArray("1111")
    assert "0000" != BitArray("1111")

    assert BitArray("0000") == BitArray("0000")
    assert BitArray("0000") != BitArray("0101")
    assert BitArray("1111") == BitArray("1111")
    assert BitArray("0000") != BitArray("1111")

    assert BitArray("0000") == LogicArray("0000")
    assert BitArray("0000") != LogicArray("0101")
    assert LogicArray("1111") == BitArray("1111")
    assert LogicArray("0000") != BitArray("1111")

    assert BitArray("0000") != object()
    assert object() != BitArray("0000")


def test_bytes() -> None:
    b = BitArray.from_bytes(b"12", byteorder="little")
    assert b == "0011001000110001"
    assert b.to_bytes(byteorder="little") == b"12"
    assert b.to_bytes(byteorder="big") == b"21"

    b = BitArray.from_bytes(b"12", byteorder="big")
    assert b == "0011000100110010"

    b = BitArray.from_bytes(b"12", Range(1, 16), byteorder="little")
    assert b == "0011001000110001"
    assert b.range == Range(1, "to", 16)

    b = BitArray.from_bytes(b"12", Range(1, 16), byteorder="big")
    assert b == "0011000100110010"
    assert b.range == Range(1, "to", 16)

    with pytest.raises(ValueError):
        b = BitArray.from_bytes(b"12", Range(0, 0), byteorder="big")


def test_indexing() -> None:
    b = BitArray("00110101")
    assert b[0] == "1"
    assert b[7] == "0"
    assert type(b[3]) is Logic
    with pytest.raises(IndexError):
        b[100]

    c = b[5:2]
    assert c == "1101"
    assert type(c) is BitArray
    assert b[3:] == "0101"
    assert b[:4] == "0011"
    with pytest.raises(IndexError):
        b[9:5]
    with pytest.raises(TypeError):
        b[3:0:-1]
    with pytest.raises(IndexError):
        b[0:1]
    with pytest.raises(TypeError):
        b[object()]  # type: ignore

    b[0] = "0"
    assert b == "00110100"
    b[4:1] = "0101"
    assert b == "00101010"
    b[3:] = "1111"
    assert b == "00101111"
    b[:7] = "1"
    assert b == "10101111"
    with pytest.raises(IndexError):
        b[10:9] = "11"
    assert b == "10101111"  # unchanged
    with pytest.raises(TypeError):
        b[7:6:-1] = "11"
    assert b == "10101111"  # unchanged
    with pytest.raises(IndexError):
        b[6:7] = "11"
    assert b == "10101111"  # unchanged
    with pytest.raises(IndexError):
        b[6:7] = "1101"
    assert b == "10101111"  # unchanged
    with pytest.raises(TypeError):
        b[:] = 100  # type: ignore
    with pytest.raises(TypeError):
        b[object()] = "1010"  # type: ignore


def test_bitwise_ops() -> None:
    assert BitArray("1010") & BitArray("1100") == BitArray("1000")
    assert LogicArray("1010") & BitArray("1100") == LogicArray("1000")
    assert BitArray("1010") & LogicArray("1100") == LogicArray("1000")
    assert BitArray("1010") & "1100" == BitArray("1000")
    assert "1010" & BitArray("1100") == BitArray("1000")
    assert BitArray("1010") & BitArray("1100", Range(1, 4)) == BitArray("1000")
    assert BitArray("1010", Range(1, 4)) & BitArray("1100") == BitArray("1000")
    with pytest.raises(ValueError):
        BitArray("0101") & "0000000"
    with pytest.raises(ValueError):
        "0000000" & BitArray("0101")
    with pytest.raises(TypeError):
        BitArray("0000") & object()  # type: ignore
    with pytest.raises(TypeError):
        object() & BitArray("0000")  # type: ignore

    assert BitArray("1010") | BitArray("1100") == BitArray("1110")
    assert LogicArray("1010") | BitArray("1100") == LogicArray("1110")
    assert BitArray("1010") | LogicArray("1100") == LogicArray("1110")
    assert BitArray("1010") | "1100" == BitArray("1110")
    assert "1010" | BitArray("1100") == BitArray("1110")
    assert BitArray("1010") | BitArray("1100", Range(1, 4)) == BitArray("1110")
    assert BitArray("1010", Range(1, 4)) | BitArray("1100") == BitArray("1110")
    with pytest.raises(ValueError):
        BitArray("0101") | "0000000"
    with pytest.raises(ValueError):
        "0000000" | BitArray("0101")
    with pytest.raises(TypeError):
        BitArray("0000") | object()  # type: ignore
    with pytest.raises(TypeError):
        object() | BitArray("0000")  # type: ignore

    assert BitArray("1010") ^ BitArray("1100") == BitArray("0110")
    assert LogicArray("1010") ^ BitArray("1100") == LogicArray("0110")
    assert BitArray("1010") ^ LogicArray("1100") == LogicArray("0110")
    assert BitArray("1010") ^ "1100" == BitArray("0110")
    assert "1010" ^ BitArray("1100") == BitArray("0110")
    assert BitArray("1010") ^ BitArray("1100", Range(1, 4)) == BitArray("0110")
    assert BitArray("1010", Range(1, 4)) ^ BitArray("1100") == BitArray("0110")
    with pytest.raises(ValueError):
        BitArray("0101") ^ "0000000"
    with pytest.raises(ValueError):
        "0000000" ^ BitArray("0101")
    with pytest.raises(TypeError):
        BitArray("0000") ^ object()  # type: ignore
    with pytest.raises(TypeError):
        object() ^ BitArray("0000")  # type: ignore

    assert ~BitArray("0000") == BitArray("1111")
    assert ~BitArray("0000", Range(1, 4)) == BitArray("1111")


def test_copy() -> None:
    b = BitArray("0000")
    assert copy.copy(b) == b
    assert copy.deepcopy(b) == b

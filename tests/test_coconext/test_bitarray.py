from __future__ import annotations

import copy

import pytest
from cocotb.types import Bit, Logic, LogicArray, Range

from coconext.types import BitArray


def test_bitarray() -> None:
    a = BitArray("1101")
    assert len(a) == 4
    assert a.range == Range(3, 0)
    assert int(a) == 13
    assert str(a) == "1101"
    assert repr(a) == "BitArray('1101', Range(3, 'downto', 0))"

    f = BitArray("1101", 4)
    assert len(f) == 4

    b = BitArray(5, 4)
    assert len(b) == 4
    assert b.range == Range(3, 0)
    assert int(b) == 5
    assert str(b) == "0101"
    assert repr(b) == "BitArray('0101', Range(3, 'downto', 0))"

    c = BitArray(LogicArray("1010"), 4)
    assert len(c) == 4
    assert c.range == Range(3, 0)
    assert int(c) == 10
    assert str(c) == "1010"

    g = BitArray(LogicArray("1010"))
    assert len(g) == 4

    d = BitArray([Bit(1), Logic("0"), "1", 0], Range(-1, -4))
    assert len(d) == 4
    assert d.range == Range(-1, -4)
    assert int(d) == 10
    assert str(d) == "1010"
    assert repr(d) == "BitArray('1010', Range(-1, 'downto', -4))"

    e = BitArray([1, 1, 1, 1, 0, 0])
    assert len(e) == 6
    assert e.range == Range(5, 0)

    e = BitArray(BitArray("111000", Range(9, 14)))
    assert len(e) == 6
    assert e.range == Range(9, 14)
    assert int(e) == 56
    assert str(e) == "111000"
    assert repr(e) == "BitArray('111000', Range(9, 'to', 14))"

    h = BitArray(0, 8)
    assert bool(h) is False
    i = BitArray(1, 8)
    assert bool(i) is True

    with pytest.raises(TypeError):
        BitArray(16)  # type: ignore

    with pytest.raises(ValueError):
        BitArray(16, 2)  # 16 does not fit in 2 bits

    with pytest.raises(ValueError):
        BitArray("1021")  # invalid character '2'

    with pytest.raises(ValueError):
        BitArray(LogicArray("10X1"))  # invalid Logic value 'X'

    with pytest.raises(ValueError):
        BitArray("1101", Range(1, 0))  # range length does not match value length

    with pytest.raises(ValueError):
        BitArray(LogicArray("1010"), 9)  # range length does not match value length

    with pytest.raises(ValueError):
        BitArray([1, 0, 1], Range(3, 0))  # range length does not match value length

    with pytest.raises(ValueError):
        BitArray(-1, 4)  # negative integer value


def test_bitarray_range() -> None:
    a = BitArray("1101", Range(0, 3))
    assert len(a) == 4
    assert a.range == Range(0, 3)
    assert a.left == 0
    assert a.right == 3
    assert a.direction == "to"

    b = BitArray("1101", Range(7, 4))
    assert len(b) == 4
    assert b.range == Range(7, 4)
    assert b.left == 7
    assert b.right == 4
    assert b.direction == "downto"

    b.range = Range(3, 0)
    assert b.range == Range(3, 0)
    assert b.left == 3
    assert b.right == 0
    assert b.direction == "downto"

    with pytest.raises(ValueError):
        b.range = Range(2, 0)  # new range length does not match BitArray length

    assert b.range == Range(3, 0)  # range should remain unchanged


def test_bitarray_equality() -> None:
    a = BitArray("1101")
    b = BitArray("1101")
    c = BitArray("1010")
    d = BitArray("110", 3)

    assert a == b
    assert a != c
    assert a != d

    assert a == "1101"
    assert a != "1010"

    assert a == LogicArray("1101")
    assert a != LogicArray("10X1")
    assert LogicArray("1101") == a
    assert LogicArray("10X1") != a

    assert a == 13
    assert a != 10

    assert a != object()
    assert object() != a


def test_bitarray_indexing() -> None:
    a = BitArray("1101001")

    assert a[0] == Bit(1)
    assert a[3] == Bit(1)
    assert a[6] == Bit(1)

    with pytest.raises(IndexError):
        _ = a[7]

    with pytest.raises(IndexError):
        _ = a[-2]

    assert a[6:1] == "110100"
    assert a[:3] == "1101"
    assert a[3:] == "1001"

    with pytest.raises(ValueError):
        _ = a[5:1:2]  # step slicing not supported

    with pytest.raises(IndexError):
        _ = a[10:5]  # invalid slice range

    with pytest.raises(IndexError):
        _ = a[4:-1]  # invalid slice range

    with pytest.raises(ValueError):
        _ = a[2:5]  # slice in opposite direction of the array

    with pytest.raises(TypeError):
        _ = a["1"]  # type: ignore # invalid index type


def test_bitarray_set_index() -> None:
    a = BitArray("1101001")

    a[0] = 0
    assert a == "1101000"
    a[2] = "1"
    assert a == "1101100"
    a[6] = Bit(0)
    assert a == "0101100"
    a[1] = Logic("0")
    assert a == "0101100"

    a[5:2] = "1111"
    assert a == "0111100"

    a[5:2] = BitArray("0000")
    assert a == "0000000"

    a[5:2] = LogicArray("1010")
    assert a == "0101000"

    a[5:2] = ["0", Bit(1), False, 1]
    assert a == "0010100"

    with pytest.raises(IndexError):
        a[7] = 1  # index out of range

    with pytest.raises(IndexError):
        a[-1] = 0  # index out of range

    with pytest.raises(ValueError):
        a[4:1:2] = "11"  # step slicing not supported

    with pytest.raises(IndexError):
        a[10:5] = "11111"  # invalid slice range

    with pytest.raises(IndexError):
        a[4:-1] = "11111"  # invalid slice range

    with pytest.raises(ValueError):
        a[2:5] = "111"  # slice in opposite direction of the array

    with pytest.raises(TypeError):
        a[object()] = 1  # type: ignore # invalid index type


def test_bitarray_sequence_methods() -> None:
    a = BitArray("1100")

    assert list(a) == [Bit(1), Bit(1), Bit(0), Bit(0)]
    assert list(reversed(a)) == [Bit(0), Bit(0), Bit(1), Bit(1)]
    assert "1" in a
    assert "0" in a
    assert Logic("X") not in a
    assert object() not in a
    assert 1 not in BitArray("0000")

    assert a.count(1) == 2
    assert a.count(0) == 2
    assert a.count(Bit(1)) == 2
    assert a.count(Bit(0)) == 2

    assert a.index(1) == 3
    assert a.index(0) == 1
    assert a.index(Bit(1)) == 3
    assert a.index(Bit(0)) == 1

    with pytest.raises(IndexError):
        assert BitArray("0000").index(1)


def test_bitarray_bitwise() -> None:
    a = BitArray("1100")
    b = BitArray("1010")

    assert a & b == BitArray("1000")
    assert a & 4 == 4
    assert 4 & a == 4
    assert a & "1010" == BitArray("1000")
    assert "1010" & a == BitArray("1000")
    assert a & LogicArray("1010") == LogicArray("1000")
    assert LogicArray("1010") & a == LogicArray("1000")

    with pytest.raises(ValueError):
        _ = a & BitArray("101")  # length mismatch

    with pytest.raises(TypeError):
        _ = a & object()  # type: ignore # invalid operand type

    with pytest.raises(TypeError):
        _ = object() & a  # type: ignore # invalid operand type

    assert a | b == BitArray("1110")
    assert a | 2 == 14
    assert 2 | a == 14
    assert a | "1010" == BitArray("1110")
    assert "1010" | a == BitArray("1110")
    assert a | LogicArray("1010") == LogicArray("1110")
    assert LogicArray("1010") | a == LogicArray("1110")

    with pytest.raises(ValueError):
        _ = a | BitArray("101")  # length mismatch

    with pytest.raises(TypeError):
        _ = a | object()  # type: ignore # invalid operand type

    with pytest.raises(TypeError):
        _ = object() | a  # type: ignore # invalid operand type

    assert a ^ b == BitArray("0110")
    assert a ^ 6 == 10
    assert 6 ^ a == 10
    assert a ^ "1010" == BitArray("0110")
    assert "1010" ^ a == BitArray("0110")
    assert a ^ LogicArray("1010") == LogicArray("0110")
    assert LogicArray("1010") ^ a == LogicArray("0110")

    with pytest.raises(ValueError):
        _ = a ^ BitArray("101")  # length mismatch

    with pytest.raises(TypeError):
        _ = a ^ object()  # type: ignore # invalid operand type

    with pytest.raises(TypeError):
        _ = object() ^ a  # type: ignore # invalid operand type

    assert ~a == BitArray("0011")


def test_copy() -> None:
    a = BitArray("1101", Range(3, 0))

    b = copy.copy(a)
    assert a == b
    assert a.range == b.range
    assert a is not b

    c = copy.deepcopy(a)
    assert a == c
    assert a.range == c.range
    assert a is not c
    assert a.range is not c.range

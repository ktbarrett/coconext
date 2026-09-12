from __future__ import annotations

import gc

import pytest

from coconext.types import Bit, BitArray, LogicArray, Signed, Unsigned


@pytest.mark.parametrize("integer_type", [Unsigned, Signed])
@pytest.mark.parametrize("width", [1, 8, 65, 129])
def test_iteration(integer_type, width):
    value = 0 if width == 1 else 10
    number = integer_type(value, width)
    expected = f"{value:0{width}b}"
    bits = list(number)
    assert all(isinstance(bit, Bit) for bit in bits)
    assert "".join(map(str, bits)) == expected
    assert "".join(map(str, reversed(number))) == expected[::-1]
    assert str(BitArray(number)) == expected
    assert str(LogicArray(number)) == expected
    assert int(number) == value


@pytest.mark.parametrize("width", [8, 65, 129])
def test_signed_iteration(width):
    number = Signed(-10, width)
    expected = "1" * (width - 4) + "0110"
    assert "".join(map(str, number)) == expected
    assert "".join(map(str, reversed(number))) == expected[::-1]
    assert str(BitArray(number)) == expected
    assert str(LogicArray(number)) == expected


@pytest.mark.parametrize("integer_type", [Unsigned, Signed])
@pytest.mark.parametrize("iterator_factory", [iter, reversed])
def test_iterator_keeps_source_alive(integer_type, iterator_factory):
    iterator = iterator_factory(integer_type(10, 129))
    gc.collect()
    expected = "0" * 125 + "1010"
    if iterator_factory is reversed:
        expected = expected[::-1]
    assert "".join(map(str, iterator)) == expected
    with pytest.raises(StopIteration):
        next(iterator)

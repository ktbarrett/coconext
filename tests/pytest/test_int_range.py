from __future__ import annotations

import operator

import pytest

from coconext.types import Range, Signed, Unsigned


@pytest.mark.parametrize("integer_type", [Unsigned, Signed])
@pytest.mark.parametrize("bounds", [Range(12, "downto", 5), Range(-4, "to", 3)])
def test_range_construction_and_indexing(integer_type, bounds):
    number = integer_type(10, bounds)
    assert number.range == bounds
    assert (number.left, number.direction, number.right) == (
        bounds.left,
        bounds.direction,
        bounds.right,
    )
    assert len(number) == 8
    assert int(number) == 10
    assert format(number, "b") == (
        f"{integer_type.__name__}[{bounds.left} {bounds.direction} {bounds.right}]"
        "{00001010}"
    )
    assert "".join(str(int(number[index])) for index in bounds) == "00001010"
    assert "".join(map(str, number)) == "00001010"
    assert "".join(map(str, reversed(number))) == "01010000"
    number[number.left] = 1
    number[number.right] = 1
    assert "".join(map(str, number)) == "10001011"
    with pytest.raises(IndexError):
        _ = number[100]
    with pytest.raises(IndexError):
        number[100] = 1
    assert integer_type(10, 8).range == Range(7, "downto", 0)
    assert integer_type(value=10, range=bounds).range == bounds


@pytest.mark.parametrize("integer_type", [Unsigned, Signed])
@pytest.mark.parametrize("bounds", [Range(70, "downto", -59), Range(-59, "to", 70)])
def test_wide_range(integer_type, bounds):
    value = (1 << 128) + 10
    if integer_type is Signed:
        value = -value
    number = integer_type(value, bounds)
    assert number.range == bounds
    assert int(number) == value
    expected = f"{value & ((1 << 130) - 1):0130b}"
    assert "".join(map(str, number)) == expected
    assert [number[index] for index in bounds] == [bool(int(bit)) for bit in expected]
    limit = 1 << (129 if integer_type is Signed else 130)
    with pytest.raises(OverflowError):
        integer_type(limit, bounds)
    with pytest.raises(OverflowError):
        integer_type(-limit - 1, bounds)


@pytest.mark.parametrize("integer_type", [Unsigned, Signed])
def test_operations_keep_ranges(integer_type):
    bounds = Range(-4, "to", 3)
    number = integer_type(10, bounds)
    rhs = integer_type(3, Range(12, "downto", 5))
    for result in (
        number << 1,
        number >> 1,
        number << 100,
        number >> 100,
        number | rhs,
        number & rhs,
        number ^ rhs,
        ~number,
    ):
        assert result.range == bounds
    assert (number + rhs).range == Range(8, "downto", 0)
    assert (number * rhs).range == Range(15, "downto", 0)
    assert (number % rhs).range == rhs.range
    for operation in (
        operator.iadd,
        operator.isub,
        operator.imul,
        operator.ifloordiv,
        operator.imod,
        operator.ilshift,
        operator.irshift,
    ):
        assert operation(integer_type(10, bounds), 1).range == bounds
        assert operation(integer_type(10, bounds), integer_type(1, 8)).range == bounds
    assert number == 10


def test_signed_modulo_keeps_divisor_range_and_inplace_keeps_dividend_range():
    number = Signed(-10, Range(-4, "to", 3))
    divisor = Signed(3, Range(12, "downto", 5))
    assert (number % divisor).range == divisor.range
    number %= divisor
    assert int(number) == 2
    assert number.range == Range(-4, "to", 3)

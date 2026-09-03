from __future__ import annotations

import math

import pytest

from coconext.types import Range, Sfixed, Ufixed


def test_sfixed_construction_and_shape() -> None:
    range_ = Range(3, "downto", -4)
    value = Sfixed(-5.0625, range_)

    assert value.range == range_
    assert value.left == 3
    assert value.right == -4
    assert value.direction == "downto"
    assert len(value) == 8
    assert float(value) == -5.0625
    assert int(value) == -5
    assert value[3]
    assert value[-4]

    value[3] = False
    value[-4] = False
    assert float(value) == 2.875


def test_sfixed_accepts_both_constructor_orders() -> None:
    range_ = Range(3, "downto", -4)
    assert Sfixed(-5.0625, range_) == Sfixed(range_, -5.0625)


def test_sfixed_float_rounding_and_saturation() -> None:
    range_ = Range(3, "downto", -4)

    assert float(Sfixed(5.55, range_)) == 5.5625
    assert float(Sfixed(-5.55, range_)) == -5.5625
    assert float(Sfixed(100.0, range_)) == 7.9375
    assert float(Sfixed(-100.0, range_)) == -8.0
    assert float(Sfixed(math.inf, range_)) == 7.9375
    assert float(Sfixed(-math.inf, range_)) == -8.0

    with pytest.raises(ValueError, match="NaN"):
        Sfixed(math.nan, range_)


def test_sfixed_arithmetic() -> None:
    a = Sfixed(-19.25, Range(5, "downto", -6))
    b = Sfixed(-5.5, Range(10, "downto", -10))

    sum_ = a + b
    assert sum_.range == Range(11, "downto", -10)
    assert float(sum_) == -24.75

    difference = a - b
    assert difference.range == Range(11, "downto", -10)
    assert float(difference) == -13.75

    product = a * b
    assert product.range == Range(16, "downto", -16)
    assert float(product) == 105.875

    quotient = Sfixed(-2, Range(2, "downto", 0)) / Sfixed(3, Range(2, "downto", 0))
    assert quotient.range == Range(3, "downto", -2)
    assert float(quotient) == -0.75

    remainder = Sfixed(-5, Range(3, "downto", 0)) % Sfixed(3, Range(2, "downto", 0))
    assert float(remainder) == -2.0


def test_sfixed_unary_shift_and_compound() -> None:
    range_ = Range(3, "downto", -4)
    value = Sfixed(-1, range_)

    assert float(value << 1) == -2.0
    assert float(value >> 1) == -0.5
    assert float(-value) == 1.0
    assert (-value).range == Range(4, "downto", -4)
    assert float(abs(value)) == 1.0

    wrapping = Sfixed(7, Range(3, "downto", 0))
    wrapping += Sfixed(1, Range(1, "downto", 0))
    assert int(wrapping) == -8
    assert wrapping.range == Range(3, "downto", 0)


def test_ufixed_construction_and_arithmetic() -> None:
    range_ = Range(3, "downto", -4)
    value = Ufixed(5.0625, range_)

    assert float(value) == 5.0625
    assert int(value) == 5

    sum_ = value + Ufixed(2.5, Range(2, "downto", -2))
    assert sum_.range == Range(4, "downto", -4)
    assert float(sum_) == 7.5625

    difference = Ufixed(5, Range(3, "downto", 0)) - Ufixed(7, Range(2, "downto", 0))
    assert isinstance(difference, Sfixed)
    assert float(difference) == -2.0

    quotient = Ufixed(2, Range(1, "downto", 0)) / Ufixed(3, Range(1, "downto", 0))
    assert quotient.range == Range(1, "downto", -2)
    assert float(quotient) == 0.75


def test_fixed_wide_values_and_validation() -> None:
    range_ = Range(100, "downto", -50)
    value = Sfixed(-5.5, range_)
    assert len(value) == 151
    assert float(value) == -5.5

    wide_integer = 1 << 150
    integer_range = Range(200, "downto", 0)
    assert int(Sfixed(wide_integer, integer_range)) == wide_integer
    assert int(Sfixed(-wide_integer, integer_range)) == -wide_integer
    assert int(Ufixed(wide_integer, integer_range)) == wide_integer

    coarse_range = Range(200, "downto", 100)
    assert int(Sfixed(wide_integer, coarse_range)) == wide_integer
    with pytest.raises(OverflowError, match="resolution"):
        Sfixed(wide_integer + 1, coarse_range)

    with pytest.raises(OverflowError):
        Sfixed(8, Range(3, "downto", 0))
    with pytest.raises(OverflowError):
        Ufixed(-1, Range(3, "downto", 0))
    with pytest.raises(ValueError, match="DOWNTO"):
        Sfixed(0, Range(0, "to", 3))
    with pytest.raises(ValueError, match="Division by zero"):
        value / Sfixed(0, range_)

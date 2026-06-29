from __future__ import annotations

import nanobind_tests
import pytest
from cocotb.types import Array, Range

ext = nanobind_tests.test_array_caster_ext


def test_array_element_wise_add():
    """Test successful translation Python -> C++ -> Python."""

    r = Range(3, "downto", 0)

    a: Array[int] = Array([1, 2, 3, 4], range=r)
    b: Array[int] = Array([100, 95, 89, 67], range=r)

    # call test cpp function (Python -> C++ -> Python)
    c = ext.element_wise_add_array(a, b)

    assert isinstance(c, Array)
    assert c == [101, 97, 92, 71]
    assert c.range == r


def test_array_element_wise_add_without_range():
    """Test successful translation from Python -> C++ -> Python(Unspecified Range)."""
    a: Array[int] = Array([1, 2, 3, 4])
    b: Array[int] = Array([100, 95, 89, 67])

    # call test cpp function (Python -> C++ -> Python)
    c = ext.element_wise_add_array(a, b)

    assert isinstance(c, Array)
    assert c == [101, 97, 92, 71]


def test_array_bounds_mismatch_throws():
    """Passing an Array with the wrong static size fails nanobind type casting."""

    r_wrong = Range(2, "downto", 0)

    a = Array([1, 2, 3], range=r_wrong)
    b = Array([1, 2, 3], range=r_wrong)

    with pytest.raises(TypeError):
        ext.element_wise_add_array(a, b)


def test_array_invalid_type_throws():
    """Only cocotb.types.Array is compatible with our test cpp function"""
    a = [1, 2, 3, 4]
    b = [1, 2, 3, 4]

    with pytest.raises(TypeError):
        ext.element_wise_add_array(a, b)


def test_array_element_invalid_type_throws():
    """Incompatible array element type must raise a type error"""
    a = Array([1, 2, 3, "4"])
    b = Array([1, 2, 3, 4])

    with pytest.raises(TypeError):
        ext.element_wise_add_array(a, b)

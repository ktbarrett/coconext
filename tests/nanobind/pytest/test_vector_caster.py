from __future__ import annotations

import nanobind_tests
import pytest
from cocotb.types import Array, Range

ext = nanobind_tests.test_vector_caster_ext


def test_vector_element_wise_add():
    """Test successful translation from Python -> C++ -> Python."""

    r = Range(3, "downto", 0)

    a: Array[int] = Array([1, 2, 3, 4], range=r)
    b: Array[int] = Array([100, 95, 89, 67], range=r)

    c = ext.element_wise_add(a, b)

    assert isinstance(c, Array), (
        "Return type should automatically cast to cocotb.types.Array"
    )

    assert c == [101, 97, 92, 71]
    assert c.range == r


def test_vector_element_wise_add_without_range():

    c: Array[int] = Array([1, 2, 3, 4])
    d: Array[int] = Array([100, 95, 89, 67])

    c = ext.element_wise_add(c, d)

    assert isinstance(c, Array), (
        "Return type should automatically cast to cocotb.types.Array"
    )

    assert c == [101, 97, 92, 71]


def test_vector_mismatched_ranges_throws():
    """Verify standard C++ exceptions map correctly to Python exceptions."""

    a = Array([1, 2, 3])  # Length 3
    b = Array([1, 2, 3, 4])  # Length 4

    with pytest.raises(ValueError):
        ext.element_wise_add(a, b)


def test_array_invalid_type_throws():
    """Only cocotb.types.Array is compatible with our test cpp function"""
    a = [1, 2, 3, 4]
    b = [1, 2, 3, 4]

    with pytest.raises(TypeError):
        ext.element_wise_add(a, b)

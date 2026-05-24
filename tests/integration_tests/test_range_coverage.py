from __future__ import annotations

import pytest

from coconext.types import Range


def test_range():
    r = Range(1, "to", 5)
    assert r.index(3, 1) == 2

    r = Range(1, "to", 5)
    with pytest.raises(ValueError):
        r.index(10, 0)

    r = Range(1, "to", 5)
    assert r.index(4, 1, 5) == 3


def test_from_range_to_branch():
    r = Range.from_range(range(1, 6))
    assert r == Range(1, "to", 5)


def test_slice_with_step_not_supported():
    r = Range(1, "to", 10)

    with pytest.raises(ValueError, match="Slicing with step is not supported"):
        r[1:5:2]


def test_index_single_argument_not_found():
    r = Range(1, "to", 5)

    with pytest.raises(ValueError, match="Value not found in range"):
        r.index(100)


def test_index_two_argument_start_equals_len():
    r = Range(1, "to", 5)

    with pytest.raises(ValueError, match="Value not found in range"):
        r.index(3, 5)


def test_index_two_argument_not_in_sliced_range():
    r = Range(1, "to", 5)

    with pytest.raises(ValueError, match="Value not found in range"):
        r.index(1, 1)


def test_index_three_argument_not_in_sliced_range():
    r = Range(1, "to", 5)

    with pytest.raises(ValueError, match="Value not found in range"):
        r.index(5, 0, 4)

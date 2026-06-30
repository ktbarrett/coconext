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


def test_range_hash_equality() -> None:
    # null range
    a = {
        Range(1, "to", 0): 1,
        Range(10, "downto", 20): 2,
    }
    assert len(a) == 1

    # single element range
    b = {
        Range(1, "to", 1): 1,
        Range(1, "downto", 1): 2,
    }
    assert len(b) == 1


def test_range_inequality() -> None:
    # left, direction, right, length
    # same, same, diff
    assert Range(1, "to", 5) != Range(1, "to", 4)
    # same, diff, same
    assert Range(1, "to", 5) != Range(1, "downto", 5)
    # diff, same, same
    assert Range(1, "to", 5) != Range(2, "to", 5)
    # same, diff, diff
    assert Range(1, "to", 5) != Range(1, "downto", 4)
    # diff, same, diff
    assert Range(1, "to", 5) != Range(2, "to", 4)
    # diff, diff, same
    assert Range(1, "to", 5) != Range(2, "downto", 5)
    # diff, diff, diff, different length
    assert Range(1, "to", 5) != Range(2, "downto", 4)
    # diff, diff, diff, same length
    assert Range(1, "to", 5) != Range(5, "downto", 1)

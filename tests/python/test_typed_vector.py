from __future__ import annotations

import _coconext
import pytest

Range = _coconext.Range

IntVector = _coconext.IntVector
StringVector = _coconext.StringVector


def test_IntVector_construction() -> None:
    a = IntVector([1, 2, 3, 4])
    assert len(a) == 4

    b = IntVector([1, 2, 3, 4], Range(-2, 1))
    assert b.left == -2
    assert b.right == 1


def test_IntVector_strict_typing() -> None:
    with pytest.raises(TypeError):
        IntVector([1, 2, "example", 4])  # type: ignore

    with pytest.raises(TypeError):
        IntVector(1)  # type: ignore


def test_IntVector_indexing() -> None:
    a = IntVector([10, 20, 30, 40], Range(8, "to", 11))

    assert a[8] == 10
    assert a[11] == 40

    with pytest.raises(IndexError):
        _ = a[0]

    a[10] = 99
    assert a[10] == 99


def test_IntVector_search_methods() -> None:
    a = IntVector([7, 8, 9, 10])

    assert 9 in a
    assert 99 not in a

    assert a.index(8) == 1
    with pytest.raises(ValueError):
        a.index(99)


def test_IntVector_equality() -> None:
    assert IntVector([1, 2, 3]) == IntVector([1, 2, 3])
    assert IntVector([1, 2, 3]) != IntVector([3, 2, 1])


def test_StringVector_construction() -> None:
    a = StringVector(["t1", "t2", "s3"])
    assert len(a) == 3


def test_StringVector_strict_typing() -> None:
    with pytest.raises(TypeError):
        StringVector(["clk", 1, "rst"])  # type: ignore


def test_StringVector_indexing_and_mutation() -> None:
    a = StringVector(["A", "B", "C"], Range(0, "to", 2))
    assert a[0] == "A"

    a[1] = "Z"
    assert a[1] == "Z"
    assert "Z" in a


def test_StringVector_iteration() -> None:
    original = ["s1", "s2", "t3"]
    a = StringVector(original)

    reconstructed = [item for item in a]
    assert reconstructed == original

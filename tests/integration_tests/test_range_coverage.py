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

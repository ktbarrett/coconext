from __future__ import annotations

import cocotb.types
import pytest
from coconext_tools.cocotb_patcher import patch_cocotb

import coconext.types


def test_cocotb() -> None:

    patch_cocotb()

    # Ensure Patched Functionality so far
    assert cocotb.types.Logic is coconext.types.Logic
    assert cocotb.types.Bit is coconext.types.Bit
    assert cocotb.types.Range is coconext.types.Range

    cocotb_pytest_dir_path: str | None = "cocotb/tests/pytest/"

    # TODO this would likely need generalization in future
    # For now maybe we can keep on adding more tests here manually
    cocotb_pytest_testcases: list[str] = ["test_logic.py", "test_range.py"]

    if cocotb_pytest_dir_path is not None:
        for cocotb_pytest_testcase in cocotb_pytest_testcases:
            result = pytest.main(
                [
                    f"{cocotb_pytest_dir_path}/{cocotb_pytest_testcase}",
                ]
            )

            assert result == 0

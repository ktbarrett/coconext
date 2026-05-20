from __future__ import annotations

import os

import cocotb.types
import pytest

import coconext.types
from coconext_tools.cocotb_patcher import patch_cocotb


def test_cocotb() -> None:

    patch_cocotb()

    # Ensure Patched Functionality so far
    assert cocotb.types.Logic is coconext.types.Logic
    assert cocotb.types.Bit is coconext.types.Bit
    assert cocotb.types.Range is coconext.types.Range

    cocotb_dir_path: str | None = os.environ.get("COCOTB_DIR_PATH")

    # TODO this would likely need generalization in future
    # For now maybe we can keep on adding more tests here manually
    cocotb_pytest_testcases: list[str] = ["test_logic.py", "test_range.py"]

    if cocotb_dir_path is not None:
        cocotb_pytest_dir_path = f"{cocotb_dir_path}/tests/pytest"

        for cocotb_pytest_testcase in cocotb_pytest_testcases:
            result = pytest.main(
                [
                    f"{cocotb_pytest_dir_path}/{cocotb_pytest_testcase}",
                ]
            )

            assert result == 0

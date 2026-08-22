"""Configuration for the simulator-free Python tests."""

from __future__ import annotations

import os
import sys

import coconext.types  # noqa: F401 -- register production nanobind types first

test_module_directory = os.environ.get("PYTHON_TESTS_MODULE_DIR")

if test_module_directory is not None and test_module_directory not in sys.path:
    sys.path.insert(0, test_module_directory)

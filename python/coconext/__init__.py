"""Extensions for cocotb.

A staging area for new cocotb features.
This project will move faster and include more experimental features than may end up in the main repo.
"""

from __future__ import annotations

import sys
from pathlib import Path

__version__ = "0.3.dev"


def cmake_prefix_path() -> Path:
    """Prefix dir to pass to CMAKE_PREFIX_PATH for ``find_package(coconext)``."""
    config_rel = Path("share") / "cmake" / "coconext" / "coconextConfig.cmake"
    candidates = [Path(__file__).resolve().parent]
    candidates += [Path(p) / "coconext" for p in sys.path if p]
    for prefix in candidates:
        if (prefix / config_rel).is_file():
            return prefix
    msg = f"could not locate {config_rel} in coconext install"
    raise RuntimeError(msg)

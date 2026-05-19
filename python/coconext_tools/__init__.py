"""Tools for coconext."""

from __future__ import annotations

import sys
from pathlib import Path


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

#!/usr/bin/env python

"""Module for querying coconext configuration."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import coconext_tools


def cmake_prefix_path() -> Path:
    """Prefix dir to pass to CMAKE_PREFIX_PATH for ``find_package(coconext)``."""
    config_rel = Path("share") / "cmake" / "coconext" / "coconextConfig.cmake"
    candidates = [Path(coconext_tools.__file__).resolve().parent]
    candidates += [Path(p) / "coconext" for p in sys.path if p]
    for prefix in candidates:
        if (prefix / config_rel).is_file():
            return prefix
    msg = f"could not locate {config_rel} in coconext install"
    raise RuntimeError(msg)


def _get_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()

    group = parser.add_mutually_exclusive_group()

    group.add_argument(
        "--cmake-prefix",
        action="store_true",
        help=(
            "Print the prefix path to use with "
            "CMAKE_PREFIX_PATH for find_package(coconext)"
        ),
    )

    return parser


def main() -> None:  # noqa: D103
    parser = _get_parser()
    args = parser.parse_args()

    if args.cmake_prefix:
        print(cmake_prefix_path().as_posix())


if __name__ == "__main__":
    main()

from __future__ import annotations

import argparse
import os
import shlex
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from packaging.utils import canonicalize_name
from scikit_build_core._compat import tomllib
from scikit_build_core.build.metadata import get_standard_metadata
from scikit_build_core.builder.builder import archs_to_tags, get_archs
from scikit_build_core.builder.wheel_tag import WheelTag
from scikit_build_core.settings.skbuild_read_settings import SettingsReader


@dataclass(frozen=True)
class ReleasePaths:
    version: str
    wheel_tag: str
    root: Path
    dist: Path
    build: Path
    venv: Path
    wheel: Path
    sdist: Path

    def get(self, key: str) -> str:
        value = getattr(self, key.replace("-", "_"))
        return str(value)

    def shell_exports(self) -> str:
        return " ".join(
            f"{key}={shlex.quote(value)}"
            for key, value in {
                "VERSION": self.version,
                "WHEEL_TAG": self.wheel_tag,
                "RELEASE_ROOT": str(self.root),
                "RELEASE_DIST_DIR": str(self.dist),
                "RELEASE_BUILD_DIR": str(self.build),
                "RELEASE_VENV": str(self.venv),
                "RELEASE_WHEEL": str(self.wheel),
                "RELEASE_SDIST": str(self.sdist),
            }.items()
        )


def _load_metadata(project_dir: Path) -> tuple[Any, Any]:
    with (project_dir / "pyproject.toml").open("rb") as f:
        pyproject = tomllib.load(f)

    settings = SettingsReader(pyproject, {}, state="wheel").settings
    metadata = get_standard_metadata(pyproject, settings)
    return metadata, settings


def _root_is_purelib(settings: Any) -> bool:
    wheel_settings = settings.wheel
    if wheel_settings.platlib is None:
        return not wheel_settings.cmake
    return not wheel_settings.platlib


def compute_release_paths(
    *,
    project_dir: Path | None = None,
    release_dir: Path = Path("release"),
) -> ReleasePaths:
    project_dir = (project_dir or Path.cwd()).resolve()
    metadata, settings = _load_metadata(project_dir)

    if metadata.version is None:
        msg = "project.version could not be resolved"
        raise RuntimeError(msg)

    wheel_tag = str(
        WheelTag.compute_best(
            archs_to_tags(get_archs(os.environ)),
            settings.wheel.py_api,
            expand_macos=settings.wheel.expand_macos_universal_tags,
            root_is_purelib=_root_is_purelib(settings),
            build_tag=settings.wheel.build_tag,
        )
    )
    version = str(metadata.version)
    root = release_dir / f"{version}-{wheel_tag}"
    dist = root / "dist"
    build = root / "build"
    venv = root / ".venv"

    wheel_name = (
        f"{metadata.canonical_name.replace('-', '_')}-"
        f"{str(metadata.version).replace('-', '_')}-{wheel_tag}.whl"
    )
    sdist_name = (
        f"{canonicalize_name(metadata.name).replace('-', '_')}-"
        f"{metadata.version}.tar.gz"
    )

    return ReleasePaths(
        version=version,
        wheel_tag=wheel_tag,
        root=root,
        dist=dist,
        build=build,
        venv=venv,
        wheel=dist / wheel_name,
        sdist=dist / sdist_name,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compute release build paths from the current project metadata."
    )
    parser.add_argument(
        "value",
        choices=[
            "version",
            "wheel-tag",
            "root",
            "dist",
            "build",
            "venv",
            "wheel",
            "sdist",
            "shell",
        ],
        help="The value to print.",
    )
    parser.add_argument(
        "--project-dir",
        type=Path,
        default=Path.cwd(),
        help="Project directory containing pyproject.toml.",
    )
    parser.add_argument(
        "--release-dir",
        type=Path,
        default=Path("release"),
        help="Base directory for release artifacts.",
    )
    args = parser.parse_args()

    paths = compute_release_paths(
        project_dir=args.project_dir,
        release_dir=args.release_dir,
    )
    if args.value == "shell":
        print(paths.shell_exports())
    else:
        print(paths.get(args.value))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

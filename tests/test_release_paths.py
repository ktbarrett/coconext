from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

from packaging.utils import canonicalize_name
from scikit_build_core._compat import tomllib
from scikit_build_core.build.metadata import get_standard_metadata
from scikit_build_core.builder.builder import archs_to_tags, get_archs
from scikit_build_core.builder.wheel_tag import WheelTag
from scikit_build_core.settings.skbuild_read_settings import SettingsReader

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "tools" / "release_paths.py"


def _load_release_paths_module() -> Any:
    spec = importlib.util.spec_from_file_location("release_paths", SCRIPT_PATH)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _expected_artifacts() -> tuple[str, str, str, str]:
    with (REPO_ROOT / "pyproject.toml").open("rb") as f:
        pyproject = tomllib.load(f)

    settings = SettingsReader(pyproject, {}, state="wheel").settings
    metadata = get_standard_metadata(pyproject, settings)
    assert metadata.version is not None

    if settings.wheel.platlib is None:
        root_is_purelib = not settings.wheel.cmake
    else:
        root_is_purelib = not settings.wheel.platlib

    wheel_tag = str(
        WheelTag.compute_best(
            archs_to_tags(get_archs(os.environ)),
            settings.wheel.py_api,
            expand_macos=settings.wheel.expand_macos_universal_tags,
            root_is_purelib=root_is_purelib,
            build_tag=settings.wheel.build_tag,
        )
    )
    version = str(metadata.version)
    wheel = (
        f"{metadata.canonical_name.replace('-', '_')}-"
        f"{str(metadata.version).replace('-', '_')}-{wheel_tag}.whl"
    )
    sdist = (
        f"{canonicalize_name(metadata.name).replace('-', '_')}-"
        f"{metadata.version}.tar.gz"
    )
    return version, wheel_tag, wheel, sdist


def test_compute_release_paths_matches_backend() -> None:
    release_paths = _load_release_paths_module()
    paths = release_paths.compute_release_paths(project_dir=REPO_ROOT)
    version, wheel_tag, wheel, sdist = _expected_artifacts()

    assert paths.version == version
    assert paths.wheel_tag == wheel_tag
    assert paths.root == Path("release") / f"{version}-{wheel_tag}"
    assert paths.dist == paths.root / "dist"
    assert paths.build == paths.root / "build"
    assert paths.venv == paths.root / ".venv"
    assert paths.wheel == paths.dist / wheel
    assert paths.sdist == paths.dist / sdist


def test_cli_returns_requested_path() -> None:
    expected = str(
        _load_release_paths_module().compute_release_paths(project_dir=REPO_ROOT).dist
    )
    result = subprocess.run(
        [sys.executable, str(SCRIPT_PATH), "dist"],
        check=True,
        capture_output=True,
        cwd=REPO_ROOT,
        text=True,
    )

    assert result.stdout.strip() == expected


def test_cli_shell_output_contains_release_vars() -> None:
    paths = _load_release_paths_module().compute_release_paths(project_dir=REPO_ROOT)
    result = subprocess.run(
        [sys.executable, str(SCRIPT_PATH), "shell"],
        check=True,
        capture_output=True,
        cwd=REPO_ROOT,
        text=True,
    )

    exports = result.stdout.strip()
    assert f"VERSION={paths.version}" in exports
    assert f"WHEEL_TAG={paths.wheel_tag}" in exports
    assert f"RELEASE_ROOT={paths.root}" in exports
    assert f"RELEASE_DIST_DIR={paths.dist}" in exports

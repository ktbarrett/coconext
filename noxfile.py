from __future__ import annotations

import glob

import nox
from nox_uv import session

nox.options.default_venv_backend = "uv"


@session(
    uv_groups=["docs"],
    uv_sync_locked=False,
)
def docs(s: nox.Session) -> None:
    outdir = s.cache_dir / "docs_out"
    s.run(
        "sphinx-build",
        "./docs",
        str(outdir),
        "--color",
        "-b",
        "html",
        *s.posargs,
    )
    index = (outdir / "index.html").resolve().as_uri()
    s.log(f"Documentation is available at {index}")


@session(
    uv_groups=["tests"],
    uv_sync_locked=False,
)
def tests(s: nox.Session) -> None:
    s.run("pytest", "--cov=coconext", *s.posargs, env={"COCOTB_USER_COVERAGE": "1"})
    coverage_files = glob.glob("**/.coverage", recursive=True)
    s.run("coverage", "combine", *coverage_files)
    s.run("coverage", "xml")
    s.run("coverage", "report")

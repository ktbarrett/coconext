from __future__ import annotations

import nox


@nox.session(reuse_venv=True)
def docs(session: nox.Session) -> None:
    session.install("-r", "docs/requirements.txt")
    session.install(".")

    outdir = session.cache_dir / "docs_out"
    session.run(
        "sphinx-build",
        "./docs",
        str(outdir),
        "--color",
        "-b",
        "html",
        *session.posargs,
    )
    index = (outdir / "index.html").resolve().as_uri()
    session.log(f"Documentation is available at {index}")


@nox.session(reuse_venv=True)
def tests(session: nox.Session) -> None:
    session.install("-r", "tests/requirements.txt")
    session.install("-e", ".")
    session.run("pytest")

Build Issues?
=============

Since we set `build-dir` to a non-temp directory there can be issues with the build reusing existing outputs.
If you run into issues just `rm -rf build/`.

Release Builds
==============

Release builds are staged under `release/{version}-{wheel-tag}/`.
There are the `dist/`, `build/`, and `.venv/` directories used during the build.
Use `python tools/release_paths.py` to inspect the exact paths for the current metadata and interpreter.

Generating Compilation DB
=========================

Build the project by running ``make dev_build``.
The compilation DB should now be under `build/{your python version triplet}/compile_commands.json`.

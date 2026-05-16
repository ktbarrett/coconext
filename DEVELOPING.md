Build Issues?
=============

Since we set `build-dir` to a non-temp directory there can be issues with the build reusing existing outputs.
If you run into issues just `rm -rf build/`.

Release Builds
==============

Release wheels and sdists are produced by the `Release` GitHub Actions workflow
(see `.github/workflows/release.yaml`), which runs cibuildwheel, executes the
full test matrix against the produced artifacts, and publishes to PyPI on tag
pushes.

Generating Compilation DB
=========================

Build the project by running ``make dev_build``.
The compilation DB should now be under `build/compile_commands.json`.

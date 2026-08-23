Build Issues?
=============

Since we set `build-dir` to a non-temp directory there can be issues with the build reusing existing outputs.
If you run into issues just `rm -rf build/`.

Running Tests
=============

Run ``make`` (or ``make dev_tests``) for the complete developer test suite. It
builds with coverage, compiler warnings, and warnings-as-errors enabled, then
runs the C++, Python, cocotb compatibility, and simulator-backed suites.
The Makefile enables ``COCONEXT_DEVELOPER_MODE`` for developer builds; the
option defaults to ``OFF`` for release tests and downstream CMake builds.
Select a simulator environment with, for example::

    make SIM=icarus TOPLEVEL_LANG=verilog CXX_STANDARD=23

After ``make dev_build``, the simulator-free suites can be run independently
with ``make cpp_tests``, ``make python_tests``, or ``make integration_tests``.
The Python suite covers both the reusable C++ nanobind adapters and the Python
extension's direct bindings. Run ``make simulator_tests`` for tests that
launch an HDL simulator. The upstream cocotb regression portion of the
compatibility suite runs when ``COCOTB_DIR_PATH`` points to a cocotb checkout.

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

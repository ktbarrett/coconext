Build Issues?
=============

Since we set `build-dir` to a non-temp directory there can be issues with the build reusing existing outputs.
If you run into issues just `rm -rf build/`.

Generating Compilation DB
=========================

Build the project by running ``make dev_build``.
The compilation DB should now be under `build/{your python version triplet}/compile_commands.json`.

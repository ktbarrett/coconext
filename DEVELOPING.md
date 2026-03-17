Build Issues?
=============

Since we set `build-dir` to a non-temp directory there can be issues with the build reusing existing outputs.
If you run into issues just `rm -rf build/`.

Generating Compilation DB
=========================

First install all build requirements manually:

```sh
> uv pip install scikit-build-core cmake nanobind setuptools
```

Next build the project with additional config options.
```sh
> uv pip install -e . --no-build-isolation --config-settings=cmake.define.CMAKE_EXPORT_COMPILE_COMMANDS=ON
```

The compilation DB should now be under `build/{your python version triplet}/compile_commands.json`.

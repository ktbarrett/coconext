Build Issues?
=============

Since we set `build-dir` to a non-temp directory there can be issues with the build reusing existing outputs.
If you run into issues just `rm -rf build/`.

Generating Compilation DB
=========================

First install all build requirements manually:

```sh
> pip install scikit-build-core cmake nanobind
```

Next build the project with additional config options.
```sh
> pip install . --no-build-isolation --config-settings=cmake.define.CMAKE_EXPORT_COMPILE_COMMANDS=ON
```

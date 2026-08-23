This directory contains the C++ libraries:

- `coconext/` builds `coconext` and `coconext_gpi`, which implement the native
  coconext API.
- `coconext_nb/` builds the header-only nanobind adapter for that API.
- `gpi/` builds cocotb's simulator interface libraries.

The `_pycoconext` Python extension is kept separately under `python/`.

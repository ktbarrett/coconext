"""Extensions for cocotb.

A staging area for new cocotb features.
This project will move faster and include more experimental features than may end up in the main repo.
"""

from __future__ import annotations

__version__ = "0.3.dev"


def _preload_libcoconext() -> None:
    """Make ``libcoconext`` resolvable for the ``_pycoconext`` extension.

    ``_pycoconext`` has a ``NEEDED libcoconext`` dependency but carries no
    RPATH (by design -- see cpp/CMakeLists.txt). Rather than teaching the
    loader where to search, we ensure the library is already loaded before the
    extension is imported, matching cocotb's loading model.

    On Linux/macOS the library is ``dlopen``-ed with ``RTLD_GLOBAL`` so the
    extension's dependency resolves against the in-memory image -- and, just as
    importantly, so the process-wide globals coconext owns (the RNG today,
    handle/scheduler state once GPI lands) are a single shared instance visible
    to every later consumer. On Windows the containing directory is added to
    the DLL search path instead.

    The library lives at ``<dir of _pycoconext>/coconext/lib/`` in both wheel
    and editable installs, so we anchor on the extension's resolved location
    rather than this package's ``__file__`` (which points at the source tree in
    editable mode).
    """
    import importlib.util
    import sys
    from pathlib import Path

    spec = importlib.util.find_spec("_pycoconext")
    if spec is None or spec.origin is None:
        # Let the subsequent `import _pycoconext` raise the informative error.
        return
    lib_dir = Path(spec.origin).parent / "coconext" / "lib"

    if sys.platform == "win32":
        import os

        if lib_dir.is_dir():
            os.add_dll_directory(str(lib_dir))
        return

    import ctypes

    leaf = "libcoconext.dylib" if sys.platform == "darwin" else "libcoconext.so"
    anchored = lib_dir / leaf
    if anchored.exists():
        # Wheel layout: the library ships next to the extension.
        ctypes.CDLL(str(anchored), mode=ctypes.RTLD_GLOBAL)
    else:
        # Editable / alternative layouts: the library isn't co-located. Try
        # using the bare name so sharing still holds when the loader can find
        # it.
        try:
            ctypes.CDLL(leaf, mode=ctypes.RTLD_GLOBAL)
        except OSError:
            pass


_preload_libcoconext()
del _preload_libcoconext

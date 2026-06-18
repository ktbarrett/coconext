"""Extensions for cocotb.

A staging area for new cocotb features.
This project will move faster and include more experimental features than may end up in the main repo.
"""

from __future__ import annotations

__version__ = "0.3.dev"


def _preload_libcoconext() -> None:
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

    from cocotb_tools.config import libs_dir as _cocotb_libs_dir

    is_darwin = sys.platform == "darwin"
    ext = ".dylib" if is_darwin else ".so"

    # libgpi is always suffixed with .so
    ctypes.CDLL(str(_cocotb_libs_dir / "libgpi.so"), mode=ctypes.RTLD_GLOBAL)

    anchored_core = lib_dir / f"libcoconext{ext}"
    anchored_sim = lib_dir / f"libcoconext_gpi{ext}"
    if anchored_core.exists():
        # Wheel layout: the libraries ship next to the extension.
        ctypes.CDLL(str(anchored_core), mode=ctypes.RTLD_GLOBAL)
        ctypes.CDLL(str(anchored_sim), mode=ctypes.RTLD_GLOBAL)
    else:
        # Editable / alternative layouts: the libraries aren't co-located.
        # Use bare names to let the dynamic linker find them if they have
        # already been loaded (e.g. via GPI_USERS).
        for leaf in (f"libcoconext{ext}", f"libcoconext_gpi{ext}"):
            try:
                ctypes.CDLL(leaf, mode=ctypes.RTLD_GLOBAL)
            except OSError:
                pass


_preload_libcoconext()
del _preload_libcoconext

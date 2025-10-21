"""Helper to safely unload a ctypes CDLL in tests.

This provides a single place to tweak platform-specific behavior
for releasing DLL handles on Windows (FreeLibrary) and for
POSIX (dropping references and forcing a garbage collection).
"""
import ctypes
import os
import gc
from typing import Any


def unload_cdll(lib: Any) -> None:
    """Attempt to unload a ctypes library handle.

    Notes:
    - On Windows, we call FreeLibrary on the internal handle. This is
      best-effort and may still fail if other references exist.
    - On POSIX, deleting the reference and running GC normally lets the
      OS unload the shared object.
    """
    try:
        if lib is None:
            return
        # prefer explicit Windows unload
        if os.name == 'nt':
            # lib._handle is implementation detail but commonly present
            handle = getattr(lib, '_handle', None)
            if handle:
                ctypes.windll.kernel32.FreeLibrary(handle)
        else:
            # remove references and run GC to encourage dlclose
            try:
                del lib
            except Exception:
                pass
            gc.collect()
    except Exception:
        # best-effort only; don't raise in tests
        pass

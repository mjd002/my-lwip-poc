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
            # lib._handle is an implementation detail but commonly present
            handle = getattr(lib, '_handle', None)
            if handle:
                try:
                    # Ensure handle is an integer (ctypes.c_void_p may be present)
                    if hasattr(handle, 'value'):
                        h = int(handle.value)
                    else:
                        h = int(handle)
                    ctypes.windll.kernel32.FreeLibrary(ctypes.c_void_p(h))
                except Exception:
                    # ignore failures; best-effort only
                    pass
            # try to drop references and force finalizers in the running process
            try:
                del lib
            except Exception:
                pass
            gc.collect()
            # small pause to let the OS catch up with handle closure
            try:
                import time

                time.sleep(0.05)
            except Exception:
                pass
        else:
            # remove references and run GC to encourage dlclose
            try:
                del lib
            except Exception:
                pass
            gc.collect()
            try:
                import time

                time.sleep(0.05)
            except Exception:
                pass
    except Exception:
        # best-effort only; don't raise in tests
        pass

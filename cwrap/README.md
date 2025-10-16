This folder contains a tiny C wrapper and a Makefile to produce `lwip_wrapper.dll` for testing from Python.

Build:

        cd cwrap
        # Use a mingw-w64 toolchain (msys2) on Windows and make sure gcc is on PATH
        mingw32-make

Run tests:

    pytest tests/test_lwip_ctypes.py -q

Note: On Windows use a MinGW make (mingw32-make) or run the gcc command directly.

CI notes:
- The repository's GitHub Actions workflow installs MSYS2 (via Chocolatey) and the
    mingw-w64 packages on the Windows runner, then adds the mingw64/bin directory
    to PATH so the PowerShell build script can locate `x86_64-w64-mingw32-gcc.exe`.
- If you customize the toolchain location locally, update `cwrap/build_lwip.ps1` to point
    to your mingw installation or set `%LOCALAPPDATA%` appropriately.
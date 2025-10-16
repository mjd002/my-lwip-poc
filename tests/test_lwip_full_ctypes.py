import os
import ctypes
import subprocess

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
CWRAP = os.path.join(ROOT, 'cwrap')
# The build now produces a minimal 'lwip_small.dll'
DLL = os.path.join(CWRAP, 'lwip_small.dll')


def build_lwip():
    script = os.path.join(CWRAP, 'build_lwip.ps1')
    # Run PowerShell script to build
    subprocess.check_call(['powershell', '-ExecutionPolicy', 'Bypass', '-File', script], cwd=CWRAP)


def test_build_and_load_lwip():
    build_lwip()
    assert os.path.exists(DLL), f"Built DLL not found at {DLL}"

    lib = ctypes.CDLL(DLL)

    # Try to find lwip_init and inet_chksum
    try:
        init = lib.lwip_init_wrapper
        init.restype = None
        init()
    except AttributeError:
        raise AssertionError('lwip_init_wrapper not found in DLL')

    try:
        chksum = lib.lwip_inet_chksum_wrapper
        chksum.argtypes = (ctypes.c_void_p, ctypes.c_int)
        chksum.restype = ctypes.c_uint16
        data = (ctypes.c_ubyte * 4)(1, 2, 3, 4)
        s = chksum(data, 4)
        assert isinstance(s, int)
    except AttributeError:
        raise AssertionError('lwip_inet_chksum_wrapper not found in DLL')

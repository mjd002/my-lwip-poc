import ctypes
import os
import sys

HERE = os.path.dirname(__file__)
DLL_PATH = os.path.join(HERE, '..', 'cwrap', 'lwip_wrapper.dll')
DLL_PATH = os.path.abspath(DLL_PATH)


def test_lwip_get_version_and_add():
    assert os.path.exists(DLL_PATH), f"DLL not found at {DLL_PATH}. Build it first with the Makefile."
    lib = ctypes.CDLL(DLL_PATH)

    lib.lwip_get_version.restype = ctypes.c_char_p
    ver = lib.lwip_get_version()
    assert isinstance(ver, bytes)
    assert b"lwip-1.4.1" in ver

    lib.lwip_add_ints.argtypes = (ctypes.c_int, ctypes.c_int)
    lib.lwip_add_ints.restype = ctypes.c_int
    assert lib.lwip_add_ints(2, 3) == 5

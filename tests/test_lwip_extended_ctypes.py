import os
import subprocess
import ctypes

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
CWRAP = os.path.join(ROOT, 'cwrap')
DLL = os.path.join(CWRAP, 'lwip_extended.dll')


def build_lwip():
    script = os.path.join(CWRAP, 'build_lwip.ps1')
    subprocess.check_call(['powershell', '-ExecutionPolicy', 'Bypass', '-File', script], cwd=CWRAP)


def test_build_and_load_lwip_extended():
    # build
    build_lwip()
    assert os.path.exists(DLL), f"Built DLL not found at {DLL}"

    # load
    lib = ctypes.CDLL(DLL)

    # call init if present
    if hasattr(lib, 'lwip_init_wrapper'):
        lib.lwip_init_wrapper()

    # call checksum wrapper
    assert hasattr(lib, 'lwip_inet_chksum_wrapper')
    chksum = lib.lwip_inet_chksum_wrapper
    chksum.argtypes = (ctypes.c_void_p, ctypes.c_int)
    chksum.restype = ctypes.c_uint16
    data = (ctypes.c_ubyte * 4)(1, 2, 3, 4)
    s = chksum(data, 4)
    assert isinstance(s, int)

    # call tcp/udp init wrappers if present
    if hasattr(lib, 'tcp_init_wrapper'):
        lib.tcp_init_wrapper()
    if hasattr(lib, 'udp_init_wrapper'):
        lib.udp_init_wrapper()

    # call sys_now wrapper and check it returns an integer-like value
    if hasattr(lib, 'sys_now_wrapper'):
        lib.sys_now_wrapper.restype = ctypes.c_uint32
        now = lib.sys_now_wrapper()
        assert isinstance(now, int)

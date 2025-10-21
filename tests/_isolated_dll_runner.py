#!/usr/bin/env python3
"""Small helper that loads a DLL in a separate process and exercises a few wrappers.

This runner is intended to be invoked from pytest via subprocess to guarantee the
process that loaded the DLL exits, ensuring the OS releases file handles so the
DLL can be re-linked during subsequent builds.

Usage examples:
  python tests/_isolated_dll_runner.py --dll path/to/lwip_extended.dll --mode basic
  python tests/_isolated_dll_runner.py --dll path/to/lwip_extended.dll --mode extended
  python tests/_isolated_dll_runner.py --dll path/to/lwip_extended.dll --mode full
"""
import argparse
import ctypes
import sys
import os
import gc


def run_basic(dll_path: str) -> None:
    lib = ctypes.CDLL(dll_path)
    try:
        # Simple smoke: version + add
        if hasattr(lib, 'lwip_get_version'):
            lib.lwip_get_version.restype = ctypes.c_char_p
            v = lib.lwip_get_version()
            if not isinstance(v, (bytes, type(None))):
                raise RuntimeError('lwip_get_version returned non-bytes')
        if hasattr(lib, 'lwip_add_ints'):
            lib.lwip_add_ints.argtypes = (ctypes.c_int, ctypes.c_int)
            lib.lwip_add_ints.restype = ctypes.c_int
            if lib.lwip_add_ints(2, 3) != 5:
                raise RuntimeError('lwip_add_ints returned wrong value')
    finally:
        try:
            del lib
        except Exception:
            pass
        gc.collect()


def run_extended(dll_path: str) -> None:
    lib = ctypes.CDLL(dll_path)
    try:
        # call init if present
        if hasattr(lib, 'lwip_init_wrapper'):
            try:
                lib.lwip_init_wrapper()
            except Exception:
                # some builds may have no-op init; ignore runtime error here
                pass

        # checksum
        if not hasattr(lib, 'lwip_inet_chksum_wrapper'):
            raise RuntimeError('lwip_inet_chksum_wrapper missing')
        chksum = lib.lwip_inet_chksum_wrapper
        chksum.argtypes = (ctypes.c_void_p, ctypes.c_int)
        chksum.restype = ctypes.c_uint16
        data = (ctypes.c_ubyte * 4)(1, 2, 3, 4)
        s = chksum(data, 4)
        if not isinstance(s, int):
            raise RuntimeError('checksum returned non-int')

        # optional sys_now
        if hasattr(lib, 'sys_now_wrapper'):
            lib.sys_now_wrapper.restype = ctypes.c_uint32
            now = lib.sys_now_wrapper()
            if not isinstance(now, int):
                raise RuntimeError('sys_now_wrapper returned non-int')
    finally:
        try:
            del lib
        except Exception:
            pass
        gc.collect()


def run_full(dll_path: str) -> None:
    # For now full == extended; keep separate for future expansion
    run_extended(dll_path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--dll', required=True, help='Path to the DLL to load')
    parser.add_argument('--mode', choices=('basic', 'extended', 'full'), default='basic')
    args = parser.parse_args()

    dll_path = os.path.abspath(args.dll)
    if not os.path.exists(dll_path):
        print(f'DLL not found: {dll_path}', file=sys.stderr)
        return 2

    try:
        if args.mode == 'basic':
            run_basic(dll_path)
        elif args.mode == 'extended':
            run_extended(dll_path)
        elif args.mode == 'full':
            run_full(dll_path)
    except Exception:
        import traceback

        traceback.print_exc()
        return 3

    return 0


if __name__ == '__main__':
    raise SystemExit(main())

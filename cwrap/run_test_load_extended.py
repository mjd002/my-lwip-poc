import ctypes
import os
import sys

# Path to the extended DLL produced by the build script
dll = os.path.abspath(os.path.join(os.path.dirname(__file__), 'lwip_extended.dll'))
print('DLL path:', dll)

if not os.path.exists(dll):
    print('ERROR: DLL not found')
    sys.exit(2)

lib = ctypes.CDLL(dll)

# Call lwip_init_wrapper if present
if hasattr(lib, 'lwip_init_wrapper'):
    lib.lwip_init_wrapper()
    print('Called lwip_init_wrapper')
else:
    print('lwip_init_wrapper not present')

# Call checksum wrapper
if hasattr(lib, 'lwip_inet_chksum_wrapper'):
    chksum = lib.lwip_inet_chksum_wrapper
    chksum.argtypes = (ctypes.c_void_p, ctypes.c_int)
    chksum.restype = ctypes.c_uint16
    data = (ctypes.c_ubyte * 4)(1, 2, 3, 4)
    result = chksum(data, 4)
    print('inet_chksum result:', int(result))
else:
    print('lwip_inet_chksum_wrapper not present')

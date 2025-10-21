import ctypes
import os
import sys

# Path to the minimal DLL produced by the build script
dll = os.path.abspath(os.path.join(os.path.dirname(__file__), 'lwip_small.dll'))
print('DLL path:', dll)

if not os.path.exists(dll):
    print('ERROR: DLL not found')
    sys.exit(2)

try:
    lib = ctypes.CDLL(dll)
except Exception as e:
    print('ERROR: loading DLL failed:', e)
    sys.exit(3)

try:
    chksum = lib.lwip_inet_chksum_wrapper
    chksum.argtypes = (ctypes.c_void_p, ctypes.c_int)
    chksum.restype = ctypes.c_uint16
    data = (ctypes.c_ubyte * 4)(1, 2, 3, 4)
    result = chksum(data, 4)
    print('inet_chksum result:', int(result))
        # unload and exit
        try:
            if os.name == 'nt':
                ctypes.windll.kernel32.FreeLibrary(lib._handle)
        except Exception:
            pass
        # exit
        sys.exit(0)
except AttributeError as e:
    print('ERROR: symbol missing:', e)
    sys.exit(4)
except Exception as e:
    print('ERROR: call failed:', e)
    sys.exit(5)

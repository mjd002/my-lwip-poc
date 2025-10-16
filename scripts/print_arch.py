import platform, sys
print(platform.architecture())
print(platform.machine())
print('is_64bit_python=', sys.maxsize > 2**32)

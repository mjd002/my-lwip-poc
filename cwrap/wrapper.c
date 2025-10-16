#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT
#endif

// Simple stub wrapper to demonstrate calling native code from Python.
// This does not currently initialize or call lwIP internals — it's a small test hook.

EXPORT const char* lwip_get_version(void) {
    return "lwip-1.4.1 (stub)";
}

EXPORT int lwip_add_ints(int a, int b) {
    return a + b;
}

/* Backup of wrapper_lwip.c */
#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

/* Declarations for lwIP symbols we will link in from the lwip sources. */
extern unsigned short inet_chksum(const void *dataptr, int len);

EXPORT const char* lwip_get_version(void) {
    return "lwip-1.4.1";
}

EXPORT unsigned short lwip_inet_chksum_wrapper(const void* data, int len) {
    return inet_chksum(data, len);
}

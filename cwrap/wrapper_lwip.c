#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

/* Declarations for lwIP symbols we will link in from the lwip sources. */
extern void lwip_init(void);
extern unsigned short inet_chksum(const void *dataptr, int len);
extern void tcp_init(void);
extern void udp_init(void);
extern unsigned long sys_now(void);

EXPORT void lwip_init_wrapper(void) {
    lwip_init();
}

EXPORT void tcp_init_wrapper(void) {
    tcp_init();
}

EXPORT void udp_init_wrapper(void) {
    udp_init();
}

EXPORT unsigned long sys_now_wrapper(void) {
    return sys_now();
}

EXPORT const char* lwip_get_version(void) {
    return "lwip-1.4.1";
}

EXPORT unsigned short lwip_inet_chksum_wrapper(const void* data, int len) {
    return inet_chksum(data, len);
}

/* Minimal perf.h stub for building on Windows with mingw-w64 */
#ifndef LWIP_ARCH_PERF_H
#define LWIP_ARCH_PERF_H

/* No-op perf macros for POC build (support both PERF_START; and PERF_START("x"); forms) */
#define PERF_INIT() ((void)0)
/* Allow both 'PERF_START;' and 'PERF_STOP("tag");' usage patterns */
#define PERF_START ((void)0)
#define PERF_STOP(x)  ((void)0)

#endif /* LWIP_ARCH_PERF_H */

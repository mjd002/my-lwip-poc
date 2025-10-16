/* Minimal lwipopts.h for building lwIP core for tests
 * This is intentionally small and comes from test/unit/lwipopts.h
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/* Prevent having to link sys_arch.c (we don't test the API layers in unit tests) */
#define NO_SYS                          1
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0

/* Minimal changes to opt.h required for tcp unit tests: */
#define MEM_SIZE                        16000
#define TCP_SND_QUEUELEN                40
#define MEMP_NUM_TCP_SEG                TCP_SND_QUEUELEN
#define TCP_SND_BUF                     (12 * TCP_MSS)
#define TCP_WND                         (10 * TCP_MSS)

/* Minimal changes to opt.h required for etharp unit tests: */
#define ETHARP_SUPPORT_STATIC_ENTRIES   1

#endif /* __LWIPOPTS_H__ */

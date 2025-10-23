/* Minimal sys_arch.h for the lwIP POC when NO_SYS==0
 * This file defines pointer-based types for semaphores, mailboxes and threads
 * so that the platform sys_arch.c can implement them as pointers to OS objects.
 */
#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "lwip/arch.h"

typedef void* sys_sem_t;
typedef void* sys_mutex_t;
typedef void* sys_mbox_t;
typedef void* sys_thread_t;
typedef u32_t  sys_prot_t;

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL NULL

#endif /* LWIP_ARCH_SYS_ARCH_H */

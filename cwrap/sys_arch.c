/* Minimal sys_arch implementation for the lwIP POC.
 * This provides sys_now(), returning milliseconds.
 * It's intentionally minimal: good enough for tests that call sys_now_wrapper().
 */
#include <stdint.h>
#include <time.h>
#include "lwip/opt.h"
#include "lwip/arch.h"

/* Provide a simple sys_prot_t for the POC so sys.h prototypes compile.
 * In a full port this would live in sys_arch.h. */
typedef u32_t sys_prot_t;

#include "lwip/sys.h"

#if defined(_WIN32) || defined(__MINGW32__)
#include <windows.h>

/* Lightweight protection using a critical section */
static CRITICAL_SECTION g_sys_lock;
static int g_sys_lock_inited = 0;

sys_prot_t sys_arch_protect(void) {
    if (!g_sys_lock_inited) { InitializeCriticalSection(&g_sys_lock); g_sys_lock_inited = 1; }
    EnterCriticalSection(&g_sys_lock);
    return (sys_prot_t)1;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void)pval;
    LeaveCriticalSection(&g_sys_lock);
}

u32_t sys_jiffies(void) {
    return (u32_t) GetTickCount64();
}

/* lwIP's lwipopts.h may define sys_msleep as a macro when NO_SYS==1; undef it
    so we can provide an implementation for the POC. */
#ifdef sys_msleep
#undef sys_msleep
#endif

void sys_msleep(u32_t ms) {
     Sleep((DWORD)ms);
}

u32_t sys_now(void) {
    /* GetTickCount64 returns milliseconds since system start. */
    return (u32_t) GetTickCount64();
}


#else
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

/* Lightweight protection using a pthread mutex */
static pthread_mutex_t g_sys_lock = PTHREAD_MUTEX_INITIALIZER;

sys_prot_t sys_arch_protect(void) {
    pthread_mutex_lock(&g_sys_lock);
    return (sys_prot_t)1;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void)pval;
    pthread_mutex_unlock(&g_sys_lock);
}

u32_t sys_jiffies(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return (u32_t)(tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
    }
    return (u32_t)(time(NULL) * 1000UL);
}

void sys_msleep(u32_t ms) {
    usleep(ms * 1000);
}

u32_t sys_now(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return (u32_t)(tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
    }
    /* fallback to time() if gettimeofday is unavailable */
    return (u32_t)(time(NULL) * 1000UL);
}

#endif

/* Minimal timeout API used by parts of lwIP: we implement a no-op registry
 * because the focused subset and unit tests in this repo don't rely on
 * asynchronous timeouts. The functions are present so linking succeeds. */

void sys_timeouts_init(void) { }

void sys_timeout(u32_t msecs, void (*handler)(void *), void *arg) {
    (void)msecs; (void)handler; (void)arg;
}

void sys_untimeout(void (*handler)(void *), void *arg) {
    (void)handler; (void)arg;
}

void sys_check_timeouts(void) { }

/* If NO_SYS==0 (i.e., lwIP expects OS primitives), provide minimal
 * semaphore and mailbox implementations. These are pointer-based so
 * they match the typedefs in arch/sys_arch.h above. */
#if !NO_SYS

/* ---- Semaphores ---- */
#if defined(_WIN32) || defined(__MINGW32__)
err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
    HANDLE h = CreateSemaphoreA(NULL, (LONG)count, LONG_MAX, NULL);
    if (!h) return ERR_MEM;
    *sem = (sys_sem_t)h;
    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem) {
    if (sem && *sem) { CloseHandle((HANDLE)*sem); *sem = NULL; }
}

void sys_sem_signal(sys_sem_t *sem) {
    if (sem && *sem) { ReleaseSemaphore((HANDLE)*sem, 1, NULL); }
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
    if (!sem || !*sem) return SYS_ARCH_TIMEOUT;
    DWORD to = (timeout == 0) ? INFINITE : (DWORD)timeout;
    DWORD r = WaitForSingleObject((HANDLE)*sem, to);
    if (r == WAIT_OBJECT_0) return 0;
    return SYS_ARCH_TIMEOUT;
}

#else
/* POSIX semaphores */
err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
    sem_t *s = (sem_t*)malloc(sizeof(sem_t));
    if (!s) return ERR_MEM;
    if (sem_init(s, 0, count) != 0) { free(s); return ERR_MEM; }
    *sem = (sys_sem_t)s;
    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem) {
    if (sem && *sem) { sem_destroy((sem_t*)*sem); free(*sem); *sem = NULL; }
}

void sys_sem_signal(sys_sem_t *sem) {
    if (sem && *sem) { sem_post((sem_t*)*sem); }
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
    if (!sem || !*sem) return SYS_ARCH_TIMEOUT;
    if (timeout == 0) {
        while (sem_wait((sem_t*)*sem) != 0) { if (errno != EINTR) return SYS_ARCH_TIMEOUT; }
        return 0;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (timeout % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000; }
    if (sem_timedwait((sem_t*)*sem, &ts) == 0) return 0;
    return SYS_ARCH_TIMEOUT;
}
#endif

/* ---- Mailboxes ---- */
/* Minimal mbox: FIFO queue of void* with mutex+cond. */
typedef struct mbox_node { struct mbox_node *next; void *msg; } mbox_node_t;
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    mbox_node_t *head, *tail;
    int valid;
} mbox_t;

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    mbox_t *m = (mbox_t*)malloc(sizeof(mbox_t));
    if (!m) return ERR_MEM;
    pthread_mutex_init(&m->lock, NULL);
    pthread_cond_init(&m->cond, NULL);
    m->head = m->tail = NULL;
    m->valid = 1;
    *mbox = (sys_mbox_t)m;
    (void)size;
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox) {
    if (!mbox || !*mbox) return;
    mbox_t *m = (mbox_t*)*mbox;
    pthread_mutex_lock(&m->lock);
    m->valid = 0;
    mbox_node_t *n = m->head;
    while (n) { mbox_node_t *next = n->next; free(n); n = next; }
    m->head = m->tail = NULL;
    pthread_mutex_unlock(&m->lock);
    pthread_mutex_destroy(&m->lock);
    pthread_cond_destroy(&m->cond);
    free(m);
    *mbox = NULL;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
    if (!mbox || !*mbox) return;
    mbox_t *m = (mbox_t*)*mbox;
    mbox_node_t *n = (mbox_node_t*)malloc(sizeof(mbox_node_t));
    n->next = NULL; n->msg = msg;
    pthread_mutex_lock(&m->lock);
    if (m->tail) m->tail->next = n; else m->head = n;
    m->tail = n;
    pthread_cond_signal(&m->cond);
    pthread_mutex_unlock(&m->lock);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout) {
    if (!mbox || !*mbox) return SYS_ARCH_TIMEOUT;
    mbox_t *m = (mbox_t*)*mbox;
    pthread_mutex_lock(&m->lock);
    while (!m->head && m->valid) {
        if (timeout == 0) {
            pthread_cond_wait(&m->cond, &m->lock);
        } else {
            struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout / 1000; ts.tv_nsec += (timeout % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000; }
            int rc = pthread_cond_timedwait(&m->cond, &m->lock, &ts);
            if (rc == ETIMEDOUT) { pthread_mutex_unlock(&m->lock); return SYS_ARCH_TIMEOUT; }
        }
    }
    if (!m->valid) { pthread_mutex_unlock(&m->lock); return SYS_ARCH_TIMEOUT; }
    mbox_node_t *n = m->head; m->head = n->next; if (!m->head) m->tail = NULL;
    *msg = n->msg; free(n);
    pthread_mutex_unlock(&m->lock);
    return 0;
}

#endif /* !NO_SYS */


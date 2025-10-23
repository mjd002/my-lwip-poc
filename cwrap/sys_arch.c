/* Consolidated minimal sys_arch implementation for the lwIP POC.
 * Single, non-duplicated implementation. Provides:
 *  - sys_arch_protect / sys_arch_unprotect
 *  - sys_jiffies, sys_now, sys_msleep
 *  - timer scheduler (sys_timeouts_init / sys_timeout / sys_untimeout)
 *  - sys_thread_new (Win32 / POSIX)
 *  - semaphores and minimal mailboxes when NO_SYS==0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/sys.h"

#if defined(_WIN32) || defined(__MINGW32__)
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/time.h>
#endif

/* Provide a sys_prot_t type expected by lwIP */
typedef u32_t sys_prot_t;

/* ---------- Protection primitives ---------- */
#if defined(_WIN32) || defined(__MINGW32__)
static CRITICAL_SECTION g_sys_lock;
static int g_sys_lock_inited = 0;

sys_prot_t sys_arch_protect(void) {
    if (!g_sys_lock_inited) { InitializeCriticalSection(&g_sys_lock); g_sys_lock_inited = 1; }
    EnterCriticalSection(&g_sys_lock);
    return (sys_prot_t)1;
}

void sys_arch_unprotect(sys_prot_t pval) { (void)pval; LeaveCriticalSection(&g_sys_lock); }

u32_t sys_jiffies(void) { return (u32_t)GetTickCount64(); }

/* lwipopts.h may define sys_msleep as a macro; ensure we can provide it */
#ifdef sys_msleep
#undef sys_msleep
#endif
void sys_msleep(u32_t ms) { Sleep((DWORD)ms); }

#else
static pthread_mutex_t g_sys_lock = PTHREAD_MUTEX_INITIALIZER;

sys_prot_t sys_arch_protect(void) { pthread_mutex_lock(&g_sys_lock); return (sys_prot_t)1; }
void sys_arch_unprotect(sys_prot_t pval) { (void)pval; pthread_mutex_unlock(&g_sys_lock); }

u32_t sys_jiffies(void) {
    struct timeval tv; if (gettimeofday(&tv, NULL) == 0) return (u32_t)(tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
    return (u32_t)(time(NULL) * 1000UL);
}

void sys_msleep(u32_t ms) { usleep(ms * 1000); }
#endif

u32_t sys_now(void) { return sys_jiffies(); }

/* ---------- Thread creation helper ---------- */
#if defined(_WIN32) || defined(__MINGW32__)
static unsigned __stdcall thread_adapter_win(void *p) {
    void (**pkg)(void*) = (void(**)(void*))p;
    void (*fn)(void*) = pkg[0];
    void *arg = (void*)pkg[1];
    free(p);
    fn(arg);
    return 0;
}

sys_thread_t sys_thread_new(const char *name, void (*thread)(void*), void *arg, int stacksize, int prio) {
    (void)name; (void)stacksize; (void)prio;
    void **pkg = malloc(2 * sizeof(void*)); if (!pkg) return NULL;
    pkg[0] = (void*)thread; pkg[1] = arg;
    uintptr_t h = _beginthreadex(NULL, 0, (unsigned (__stdcall *)(void *))thread_adapter_win, pkg, 0, NULL);
    if (h == 0) { free(pkg); return NULL; }
    return (sys_thread_t)(void*)h;
}

#else
static void *thread_adapter_posix(void *p) {
    void (**pkg)(void*) = p; void (*fn)(void*) = pkg[0]; void *arg = (void*)pkg[1]; free(p); fn(arg); return NULL;
}

sys_thread_t sys_thread_new(const char *name, void (*thread)(void*), void *arg, int stacksize, int prio) {
    (void)stacksize; (void)prio;
    void **pkg = malloc(2 * sizeof(void*)); if (!pkg) return 0;
    pkg[0] = (void*)thread; pkg[1] = arg;
    pthread_t tid; pthread_attr_t attr; pthread_attr_init(&attr);
    if (pthread_create(&tid, &attr, thread_adapter_posix, pkg) != 0) { free(pkg); return 0; }
    /* best-effort name */
#if defined(__APPLE__)
    if (name) pthread_setname_np(name);
#else
    if (name) pthread_setname_np(tid, name);
#endif
    return (sys_thread_t)tid;
}
#endif

/* ---------- Timer scheduler ---------- */
typedef struct time_entry { u32_t when_ms; void (*handler)(void*); void *arg; struct time_entry *next; } time_entry_t;
static time_entry_t *g_time_head = NULL;
#if defined(_WIN32) || defined(__MINGW32__)
static CONDITION_VARIABLE g_time_cond; static CRITICAL_SECTION g_time_lock; static int g_time_lock_inited = 0;
#else
static pthread_cond_t g_time_cond = PTHREAD_COND_INITIALIZER; static pthread_mutex_t g_time_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
static int g_time_thread_started = 0;

static void timer_thread_fn(void *arg) {
    (void)arg;
    for (;;) {
        u32_t now = sys_now(); u32_t wait_ms = 1000;
#if defined(_WIN32) || defined(__MINGW32__)
        EnterCriticalSection(&g_time_lock);
        if (g_time_head) {
            if ((s32_t)(g_time_head->when_ms - now) <= 0) { LeaveCriticalSection(&g_time_lock); }
            else wait_ms = g_time_head->when_ms - now;
        }
        SleepConditionVariableCS(&g_time_cond, &g_time_lock, wait_ms);
        LeaveCriticalSection(&g_time_lock);
#else
        pthread_mutex_lock(&g_time_lock);
        if (g_time_head) {
            if ((s32_t)(g_time_head->when_ms - now) <= 0) { pthread_mutex_unlock(&g_time_lock); }
            else wait_ms = g_time_head->when_ms - now;
        }
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += wait_ms / 1000; ts.tv_nsec += (wait_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000; }
        pthread_cond_timedwait(&g_time_cond, &g_time_lock, &ts);
        pthread_mutex_unlock(&g_time_lock);
#endif
        /* execute expired handlers */
        for (;;) {
            now = sys_now();
#if defined(_WIN32) || defined(__MINGW32__)
            EnterCriticalSection(&g_time_lock);
#else
            pthread_mutex_lock(&g_time_lock);
#endif
            if (!g_time_head || (s32_t)(g_time_head->when_ms - now) > 0) {
#if defined(_WIN32) || defined(__MINGW32__)
                LeaveCriticalSection(&g_time_lock);
#else
                pthread_mutex_unlock(&g_time_lock);
#endif
                break;
            }
            time_entry_t *e = g_time_head; g_time_head = e->next;
#if defined(_WIN32) || defined(__MINGW32__)
            LeaveCriticalSection(&g_time_lock);
#else
            pthread_mutex_unlock(&g_time_lock);
#endif
            e->handler(e->arg);
            free(e);
        }
    }
}

void sys_timeouts_init(void) {
#if defined(_WIN32) || defined(__MINGW32__)
    if (!g_time_lock_inited) { InitializeCriticalSection(&g_time_lock); InitializeConditionVariable(&g_time_cond); g_time_lock_inited = 1; }
#endif
    if (!g_time_thread_started) { g_time_thread_started = 1; sys_thread_new("lwip_timer", (void (*)(void*))timer_thread_fn, NULL, 0, 1); }
}

void sys_timeout(u32_t msecs, void (*handler)(void *), void *arg) {
    if (!handler) return; time_entry_t *e = malloc(sizeof(*e)); if (!e) return;
    e->when_ms = sys_now() + msecs; e->handler = handler; e->arg = arg; e->next = NULL;
#if defined(_WIN32) || defined(__MINGW32__)
    EnterCriticalSection(&g_time_lock);
#else
    pthread_mutex_lock(&g_time_lock);
#endif
    time_entry_t **pp = &g_time_head; while (*pp && (*pp)->when_ms <= e->when_ms) pp = &(*pp)->next; e->next = *pp; *pp = e;
#if defined(_WIN32) || defined(__MINGW32__)
    WakeConditionVariable(&g_time_cond); LeaveCriticalSection(&g_time_lock);
#else
    pthread_cond_signal(&g_time_cond); pthread_mutex_unlock(&g_time_lock);
#endif
}

void sys_untimeout(void (*handler)(void *), void *arg) {
    if (!handler) return;
#if defined(_WIN32) || defined(__MINGW32__)
    EnterCriticalSection(&g_time_lock);
#else
    pthread_mutex_lock(&g_time_lock);
#endif
    time_entry_t **pp = &g_time_head; while (*pp) { if ((*pp)->handler == handler && (*pp)->arg == arg) { time_entry_t *d = *pp; *pp = d->next; free(d); continue; } pp = &(*pp)->next; }
#if defined(_WIN32) || defined(__MINGW32__)
    LeaveCriticalSection(&g_time_lock);
#else
    pthread_mutex_unlock(&g_time_lock);
#endif
}

void sys_check_timeouts(void) { /* timer thread executes handlers */ }

/* ---------- Semaphores and mailboxes (when NO_SYS==0) ---------- */
#if !NO_SYS

/* Semaphores */
#if defined(_WIN32) || defined(__MINGW32__)
err_t sys_sem_new(sys_sem_t *sem, u8_t count) { HANDLE h = CreateSemaphoreA(NULL, (LONG)count, LONG_MAX, NULL); if (!h) return ERR_MEM; *sem = (sys_sem_t)h; return ERR_OK; }
void sys_sem_free(sys_sem_t *sem) { if (sem && *sem) { CloseHandle((HANDLE)*sem); *sem = NULL; } }
void sys_sem_signal(sys_sem_t *sem) { if (sem && *sem) ReleaseSemaphore((HANDLE)*sem, 1, NULL); }
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) { if (!sem || !*sem) return SYS_ARCH_TIMEOUT; DWORD to = (timeout==0)?INFINITE:(DWORD)timeout; DWORD r = WaitForSingleObject((HANDLE)*sem, to); return (r==WAIT_OBJECT_0)?0:SYS_ARCH_TIMEOUT; }
#else
err_t sys_sem_new(sys_sem_t *sem, u8_t count) { sem_t *s = malloc(sizeof(sem_t)); if (!s) return ERR_MEM; if (sem_init(s,0,count)!=0) { free(s); return ERR_MEM; } *sem = (sys_sem_t)s; return ERR_OK; }
void sys_sem_free(sys_sem_t *sem) { if (sem && *sem) { sem_destroy((sem_t*)*sem); free(*sem); *sem = NULL; } }
void sys_sem_signal(sys_sem_t *sem) { if (sem && *sem) sem_post((sem_t*)*sem); }
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) { if (!sem || !*sem) return SYS_ARCH_TIMEOUT; if (timeout==0) { while (sem_wait((sem_t*)*sem)!=0) { if (errno!=EINTR) return SYS_ARCH_TIMEOUT; } return 0; } struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts); ts.tv_sec += timeout/1000; ts.tv_nsec += (timeout%1000)*1000000; if (ts.tv_nsec>=1000000000) { ts.tv_sec+=1; ts.tv_nsec-=1000000000; } if (sem_timedwait((sem_t*)*sem,&ts)==0) return 0; return SYS_ARCH_TIMEOUT; }
#endif

/* Simple FIFO mailbox (pointer based) */
typedef struct mbox_node { struct mbox_node *next; void *msg; } mbox_node_t;
typedef struct { int capacity; int count;
#if defined(_WIN32) || defined(__MINGW32__)
    CRITICAL_SECTION lock; CONDITION_VARIABLE cond_nonempty; CONDITION_VARIABLE cond_nonfull;
#else
    pthread_mutex_t lock; pthread_cond_t cond_nonempty; pthread_cond_t cond_nonfull;
#endif
    mbox_node_t *head, *tail; int valid; } mbox_t;

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    if (!mbox) return ERR_ARG; mbox_t *m = malloc(sizeof(mbox_t)); if (!m) return ERR_MEM; m->capacity = (size>0)?size:32; m->count = 0; m->head = m->tail = NULL; m->valid = 1;
#if defined(_WIN32) || defined(__MINGW32__)
    InitializeCriticalSection(&m->lock); InitializeConditionVariable(&m->cond_nonempty); InitializeConditionVariable(&m->cond_nonfull);
#else
    pthread_mutex_init(&m->lock,NULL); pthread_cond_init(&m->cond_nonempty,NULL); pthread_cond_init(&m->cond_nonfull,NULL);
#endif
    *mbox = (sys_mbox_t)m; return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox) { if (!mbox || !*mbox) return; mbox_t *m = (mbox_t*)*mbox; /* drain and destroy */
#if defined(_WIN32) || defined(__MINGW32__)
    EnterCriticalSection(&m->lock);
#else
    pthread_mutex_lock(&m->lock);
#endif
    m->valid = 0; mbox_node_t *n = m->head; while (n) { mbox_node_t *next = n->next; free(n); n = next; }
    m->head = m->tail = NULL; m->count = 0;
#if defined(_WIN32) || defined(__MINGW32__)
    LeaveCriticalSection(&m->lock); DeleteCriticalSection(&m->lock);
#else
    pthread_mutex_unlock(&m->lock); pthread_mutex_destroy(&m->lock); pthread_cond_destroy(&m->cond_nonempty); pthread_cond_destroy(&m->cond_nonfull);
#endif
    free(m); *mbox = NULL;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
    if (!mbox || !*mbox) return; mbox_t *m = (mbox_t*)*mbox;
#if defined(_WIN32) || defined(__MINGW32__)
    EnterCriticalSection(&m->lock); while (m->count >= m->capacity) { SleepConditionVariableCS(&m->cond_nonfull, &m->lock, INFINITE); }
#else
    pthread_mutex_lock(&m->lock); while (m->count >= m->capacity) { pthread_cond_wait(&m->cond_nonfull, &m->lock); }
#endif
    mbox_node_t *n = malloc(sizeof(mbox_node_t)); n->msg = msg; n->next = NULL; if (m->tail) m->tail->next = n; else m->head = n; m->tail = n; m->count++;
#if defined(_WIN32) || defined(__MINGW32__)
    WakeConditionVariable(&m->cond_nonempty); LeaveCriticalSection(&m->lock);
#else
    pthread_cond_signal(&m->cond_nonempty); pthread_mutex_unlock(&m->lock);
#endif
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) {
    if (!mbox || !*mbox) return ERR_ARG; mbox_t *m = (mbox_t*)*mbox;
#if defined(_WIN32) || defined(__MINGW32__)
    EnterCriticalSection(&m->lock); if (m->count >= m->capacity) { LeaveCriticalSection(&m->lock); return ERR_MEM; }
#else
    pthread_mutex_lock(&m->lock); if (m->count >= m->capacity) { pthread_mutex_unlock(&m->lock); return ERR_MEM; }
#endif
    mbox_node_t *n = malloc(sizeof(mbox_node_t)); n->msg = msg; n->next = NULL; if (m->tail) m->tail->next = n; else m->head = n; m->tail = n; m->count++;
#if defined(_WIN32) || defined(__MINGW32__)
    WakeConditionVariable(&m->cond_nonempty); LeaveCriticalSection(&m->lock);
#else
    pthread_cond_signal(&m->cond_nonempty); pthread_mutex_unlock(&m->lock);
#endif
    return ERR_OK;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout) {
    if (!mbox || !*mbox) return SYS_ARCH_TIMEOUT; mbox_t *m = (mbox_t*)*mbox; *msg = NULL;
#if defined(_WIN32) || defined(__MINGW32__)
    EnterCriticalSection(&m->lock);
    while (m->count == 0 && m->valid) { SleepConditionVariableCS(&m->cond_nonempty, &m->lock, (timeout==0)?INFINITE:timeout); if (timeout!=0) break; }
    if (m->count == 0) { LeaveCriticalSection(&m->lock); return SYS_ARCH_TIMEOUT; }
    mbox_node_t *n = m->head; m->head = n->next; if (!m->head) m->tail = NULL; m->count--; *msg = n->msg; free(n);
    WakeConditionVariable(&m->cond_nonfull); LeaveCriticalSection(&m->lock); return 0;
#else
    pthread_mutex_lock(&m->lock);
    if (timeout==0) {
        while (m->count==0 && m->valid) pthread_cond_wait(&m->cond_nonempty, &m->lock);
    } else {
        struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts); ts.tv_sec += timeout/1000; ts.tv_nsec += (timeout%1000)*1000000; if (ts.tv_nsec>=1000000000){ ts.tv_sec+=1; ts.tv_nsec-=1000000000; }
        int rc = 0; while (m->count==0 && m->valid && rc==0) rc = pthread_cond_timedwait(&m->cond_nonempty, &m->lock, &ts);
        if (m->count==0) { pthread_mutex_unlock(&m->lock); return SYS_ARCH_TIMEOUT; }
    }
    mbox_node_t *n = m->head; m->head = n->next; if (!m->head) m->tail = NULL; m->count--; *msg = n->msg; free(n);
    pthread_cond_signal(&m->cond_nonfull); pthread_mutex_unlock(&m->lock); return 0;
#endif
}

int sys_mbox_valid(sys_mbox_t *mbox) { return (mbox && *mbox) ? 1 : 0; }
void sys_mbox_set_invalid(sys_mbox_t *mbox) { if (mbox) *mbox = NULL; }

int sys_sem_valid(sys_sem_t *sem) { return (sem && *sem) ? 1 : 0; }
void sys_sem_set_invalid(sys_sem_t *sem) { if (sem) *sem = NULL; }

#endif /* !NO_SYS */


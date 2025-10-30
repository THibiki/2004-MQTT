#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/time.h"
#include "lwip/err.h"
#include "lwip/sys.h"

// System architecture definitions for lwIP on Pico W

// Semaphore type - wrap Pico semaphore
typedef struct {
    semaphore_t sem;
} sys_sem_t;

// Mutex type - wrap Pico mutex  
typedef struct {
    mutex_t mtx;
} sys_mutex_t;

// Mailbox type
#define SYS_MBOX_SIZE 32
typedef struct {
    void *msg[SYS_MBOX_SIZE];
    int head;
    int tail;
    int size;
    semaphore_t not_empty;
    semaphore_t not_full;
    mutex_t lock;
} sys_mbox_t;

// Thread type - placeholder since Pico doesn't have proper threading
typedef struct {
    int dummy;
} sys_thread_t;

// Protection type for critical sections
typedef uint32_t sys_prot_t;

// Constants
#define SYS_MBOX_NULL ((sys_mbox_t *)0)
#define SYS_SEM_NULL ((sys_sem_t *)0)
#define SYS_MUTEX_NULL ((sys_mutex_t *)0)
#define SYS_ARCH_TIMEOUT 0xffffffff

// Function declarations - implemented in sys_arch.c
err_t sys_sem_new(sys_sem_t *sem, u8_t count);
void sys_sem_signal(sys_sem_t *sem);
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout);
void sys_sem_free(sys_sem_t *sem);
int sys_sem_valid(sys_sem_t *sem);
void sys_sem_set_invalid(sys_sem_t *sem);

err_t sys_mutex_new(sys_mutex_t *mutex);
void sys_mutex_lock(sys_mutex_t *mutex);
void sys_mutex_unlock(sys_mutex_t *mutex);
void sys_mutex_free(sys_mutex_t *mutex);
int sys_mutex_valid(sys_mutex_t *mutex);
void sys_mutex_set_invalid(sys_mutex_t *mutex);

err_t sys_mbox_new(sys_mbox_t *mbox, int size);
void sys_mbox_post(sys_mbox_t *mbox, void *msg);
err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg);
u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout);
u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg);
void sys_mbox_free(sys_mbox_t *mbox);
int sys_mbox_valid(sys_mbox_t *mbox);
void sys_mbox_set_invalid(sys_mbox_t *mbox);

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio);

sys_prot_t sys_arch_protect(void);
void sys_arch_unprotect(sys_prot_t pval);

// Time function implemented as macro
#define sys_now() to_ms_since_boot(get_absolute_time())

#endif // LWIP_ARCH_SYS_ARCH_H

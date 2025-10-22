#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "pico/stdlib.h"
#include "pico/sync.h"

// System architecture definitions for lwIP on Pico W
// This file provides the system-specific implementations for lwIP

// Semaphore type
typedef semaphore_t sys_sem_t;

// Mutex type  
typedef mutex_t sys_mutex_t;

// Mailbox type
typedef struct {
    void **items;
    int head;
    int tail;
    int size;
    int count;
    semaphore_t sem;
} sys_mbox_t;

// Thread type
typedef struct {
    // Pico doesn't have threads, so this is just a placeholder
    int dummy;
} sys_thread_t;

// Function prototypes for system functions
// These would need to be implemented if actually used
// For now, we'll just provide stubs

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL NULL
#define SYS_MUTEX_NULL NULL

// Semaphore functions
#define sys_sem_new(sem, count) 0
#define sys_sem_signal(sem) 
#define sys_sem_wait(sem) 0
#define sys_sem_free(sem)

// Mutex functions
#define sys_mutex_new(mutex) 0
#define sys_mutex_lock(mutex) 0
#define sys_mutex_unlock(mutex)
#define sys_mutex_free(mutex)

// Mailbox functions
#define sys_mbox_new(mbox, size) 0
#define sys_mbox_post(mbox, msg) 0
#define sys_mbox_trypost(mbox, msg) 0
#define sys_mbox_tryfetch(mbox, msg) 0
#define sys_mbox_free(mbox)

// Thread functions
#define sys_thread_new(name, thread, arg, stacksize, prio) 0

// Time functions
#define sys_now() to_ms_since_boot(get_absolute_time())

// Critical section protection
#define SYS_ARCH_DECL_PROTECT(lev) 
#define SYS_ARCH_PROTECT(lev)
#define SYS_ARCH_UNPROTECT(lev)

#endif // LWIP_ARCH_SYS_ARCH_H

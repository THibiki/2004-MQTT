#include "sys_arch.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/time.h"
#include "pico/multicore.h"

// Mutex functions
err_t sys_mutex_new(sys_mutex_t *mutex) {
    if (!mutex) return ERR_ARG;
    mutex_init(&mutex->mtx);
    return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex) {
    if (mutex) {
        mutex_enter_blocking(&mutex->mtx);
    }
}

void sys_mutex_unlock(sys_mutex_t *mutex) {
    if (mutex) {
        mutex_exit(&mutex->mtx);
    }
}

void sys_mutex_free(sys_mutex_t *mutex) {
    // Pico SDK doesn't have explicit mutex free
    (void)mutex;
}

int sys_mutex_valid(sys_mutex_t *mutex) {
    return (mutex != NULL) ? 1 : 0;
}

void sys_mutex_set_invalid(sys_mutex_t *mutex) {
    // Pico SDK doesn't have explicit invalidation
    (void)mutex;
}

// Semaphore functions
err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
    if (!sem) return ERR_ARG;
    sem_init(&sem->sem, count, 255);
    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem) {
    if (sem) {
        sem_release(&sem->sem);
    }
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
    if (!sem) return SYS_ARCH_TIMEOUT;
    
    if (timeout == 0) {
        // Non-blocking wait
        if (sem_try_acquire(&sem->sem, 1)) {
            return 0;
        } else {
            return SYS_ARCH_TIMEOUT;
        }
    } else if (timeout == SYS_ARCH_TIMEOUT) {
        // Blocking wait
        sem_acquire_blocking(&sem->sem);
        return 0;
    } else {
        // Timeout wait
        absolute_time_t target = make_timeout_time_ms(timeout);
        if (sem_acquire_timeout_ms(&sem->sem, timeout)) {
            return 0;
        } else {
            return SYS_ARCH_TIMEOUT;
        }
    }
}

void sys_sem_free(sys_sem_t *sem) {
    // Pico SDK doesn't have explicit semaphore free
    (void)sem;
}

int sys_sem_valid(sys_sem_t *sem) {
    return (sem != NULL) ? 1 : 0;
}

void sys_sem_set_invalid(sys_sem_t *sem) {
    // Pico SDK doesn't have explicit invalidation
    (void)sem;
}

// Mailbox functions
err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    if (!mbox) return ERR_ARG;
    
    mbox->head = 0;
    mbox->tail = 0;
    
    // Initialize semaphores
    if (sys_sem_new(&mbox->not_empty, 0) != ERR_OK) return ERR_MEM;
    if (sys_sem_new(&mbox->not_full, size) != ERR_OK) {
        sys_sem_free(&mbox->not_empty);
        return ERR_MEM;
    }
    
    // Initialize mutex
    if (sys_mutex_new(&mbox->mutex) != ERR_OK) {
        sys_sem_free(&mbox->not_empty);
        sys_sem_free(&mbox->not_full);
        return ERR_MEM;
    }
    
    return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
    if (!mbox || !msg) return;
    
    sys_mutex_lock(&mbox->mutex);
    mbox->msg[mbox->tail] = msg;
    mbox->tail = (mbox->tail + 1) % 10;
    sys_mutex_unlock(&mbox->mutex);
    
    sys_sem_signal(&mbox->not_empty);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) {
    if (!mbox || !msg) return ERR_ARG;
    
    sys_mutex_lock(&mbox->mutex);
    if ((mbox->tail + 1) % 10 == mbox->head) {
        sys_mutex_unlock(&mbox->mutex);
        return ERR_MEM; // Full
    }
    
    mbox->msg[mbox->tail] = msg;
    mbox->tail = (mbox->tail + 1) % 10;
    sys_mutex_unlock(&mbox->mutex);
    
    sys_sem_signal(&mbox->not_empty);
    return ERR_OK;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout) {
    if (!mbox || !msg) return SYS_ARCH_TIMEOUT;
    
    if (sys_arch_sem_wait(&mbox->not_empty, timeout) == SYS_ARCH_TIMEOUT) {
        return SYS_ARCH_TIMEOUT;
    }
    
    sys_mutex_lock(&mbox->mutex);
    *msg = mbox->msg[mbox->head];
    mbox->head = (mbox->head + 1) % 10;
    sys_mutex_unlock(&mbox->mutex);
    
    sys_sem_signal(&mbox->not_full);
    return 0;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg) {
    if (!mbox || !msg) return SYS_ARCH_TIMEOUT;
    
    if (sys_arch_sem_wait(&mbox->not_empty, 0) == SYS_ARCH_TIMEOUT) {
        return SYS_ARCH_TIMEOUT;
    }
    
    sys_mutex_lock(&mbox->mutex);
    *msg = mbox->msg[mbox->head];
    mbox->head = (mbox->head + 1) % 10;
    sys_mutex_unlock(&mbox->mutex);
    
    sys_sem_signal(&mbox->not_full);
    return 0;
}

void sys_mbox_free(sys_mbox_t *mbox) {
    if (!mbox) return;
    
    sys_sem_free(&mbox->not_empty);
    sys_sem_free(&mbox->not_full);
    sys_mutex_free(&mbox->mutex);
}

int sys_mbox_valid(sys_mbox_t *mbox) {
    return (mbox != NULL) ? 1 : 0;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox) {
    // Pico SDK doesn't have explicit invalidation
    (void)mbox;
}

// Thread functions
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio) {
    // Pico SDK doesn't have traditional threads, return placeholder
    (void)name;
    (void)thread;
    (void)arg;
    (void)stacksize;
    (void)prio;
    return 0;
}

// Protection functions
sys_prot_t sys_arch_protect(void) {
    // Simple implementation - just return 0
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval) {
    // Simple implementation - do nothing
    (void)pval;
}

// Time function
u32_t sys_now(void) {
    return to_ms_since_boot(get_absolute_time());
}

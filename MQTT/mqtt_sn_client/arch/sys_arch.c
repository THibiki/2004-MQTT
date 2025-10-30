#include "sys_arch.h"
#include "lwip/err.h"
#include "lwip/sys.h"

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
    mbox->size = (size > SYS_MBOX_SIZE) ? SYS_MBOX_SIZE : size;
    
    // Initialize semaphores
    sem_init(&mbox->not_empty, 0, mbox->size);
    sem_init(&mbox->not_full, mbox->size, mbox->size);
    
    // Initialize mutex
    mutex_init(&mbox->lock);
    
    return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
    if (!mbox) return;
    
    // Wait for space to be available
    sem_acquire_blocking(&mbox->not_full);
    
    mutex_enter_blocking(&mbox->lock);
    mbox->msg[mbox->tail] = msg;
    mbox->tail = (mbox->tail + 1) % SYS_MBOX_SIZE;
    mutex_exit(&mbox->lock);
    
    sem_release(&mbox->not_empty);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) {
    if (!mbox) return ERR_ARG;
    
    // Try to acquire space without blocking
    if (!sem_try_acquire(&mbox->not_full, 1)) {
        return ERR_MEM; // Full
    }
    
    mutex_enter_blocking(&mbox->lock);
    mbox->msg[mbox->tail] = msg;
    mbox->tail = (mbox->tail + 1) % SYS_MBOX_SIZE;
    mutex_exit(&mbox->lock);
    
    sem_release(&mbox->not_empty);
    return ERR_OK;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout) {
    if (!mbox || !msg) return SYS_ARCH_TIMEOUT;
    
    // Wait for a message to be available
    if (timeout == 0) {
        if (!sem_try_acquire(&mbox->not_empty, 1)) {
            return SYS_ARCH_TIMEOUT;
        }
    } else if (timeout == SYS_ARCH_TIMEOUT) {
        sem_acquire_blocking(&mbox->not_empty);
    } else {
        if (!sem_acquire_timeout_ms(&mbox->not_empty, timeout)) {
            return SYS_ARCH_TIMEOUT;
        }
    }
    
    mutex_enter_blocking(&mbox->lock);
    *msg = mbox->msg[mbox->head];
    mbox->head = (mbox->head + 1) % SYS_MBOX_SIZE;
    mutex_exit(&mbox->lock);
    
    sem_release(&mbox->not_full);
    return 0;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg) {
    if (!mbox || !msg) return SYS_ARCH_TIMEOUT;
    
    if (!sem_try_acquire(&mbox->not_empty, 1)) {
        return SYS_ARCH_TIMEOUT;
    }
    
    mutex_enter_blocking(&mbox->lock);
    *msg = mbox->msg[mbox->head];
    mbox->head = (mbox->head + 1) % SYS_MBOX_SIZE;
    mutex_exit(&mbox->lock);
    
    sem_release(&mbox->not_full);
    return 0;
}

void sys_mbox_free(sys_mbox_t *mbox) {
    if (!mbox) return;
    
    // Pico SDK doesn't have explicit free functions for sync primitives
    // The memory will be freed when the struct is freed
    (void)mbox;
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

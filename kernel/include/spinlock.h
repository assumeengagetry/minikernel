#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "types.h"

/*
 * Spinlock structure (simplified for microkernel)
 */
typedef struct spinlock {
    volatile u32 lock;
} spinlock_t;

/*
 * Read-write lock structure
 */
typedef struct rwlock {
    volatile u32 lock;
    volatile u32 readers;
} rwlock_t;

/* Spinlock states */
#define SPINLOCK_UNLOCKED   0
#define SPINLOCK_LOCKED     1

/* Initialization macros */
#define SPIN_LOCK_INIT { .lock = 0 }
#define DEFINE_SPINLOCK(name) spinlock_t name = SPIN_LOCK_INIT

#define RW_LOCK_INIT { .lock = 0, .readers = 0 }
#define DEFINE_RWLOCK(name) rwlock_t name = RW_LOCK_INIT

/*
 * Forward declarations for external functions
 */
extern u32 smp_processor_id(void);
extern void cpu_relax(void);
extern unsigned long local_irq_save(void);
extern void local_irq_restore(unsigned long flags);
extern void local_irq_disable(void);
extern void local_irq_enable(void);
extern void local_bh_disable(void);
extern void local_bh_enable(void);

/*
 * Spinlock initialization
 */
static inline void spin_lock_init(spinlock_t *lock)
{
    lock->lock = SPINLOCK_UNLOCKED;
}

/*
 * Acquire spinlock
 */
static inline void spin_lock(spinlock_t *lock)
{
    while (1) {
        if (__sync_bool_compare_and_swap(&lock->lock, 
                                         SPINLOCK_UNLOCKED, 
                                         SPINLOCK_LOCKED)) {
            break;
        }
        while (lock->lock == SPINLOCK_LOCKED) {
            cpu_relax();
        }
    }
}

/*
 * Try to acquire spinlock
 */
static inline int spin_trylock(spinlock_t *lock)
{
    return __sync_bool_compare_and_swap(&lock->lock, 
                                        SPINLOCK_UNLOCKED, 
                                        SPINLOCK_LOCKED);
}

/*
 * Release spinlock
 */
static inline void spin_unlock(spinlock_t *lock)
{
    __sync_synchronize();
    lock->lock = SPINLOCK_UNLOCKED;
}

/*
 * Check if spinlock is locked
 */
static inline int spin_is_locked(spinlock_t *lock)
{
    return lock->lock == SPINLOCK_LOCKED;
}

/*
 * Spinlock with IRQ save
 */
static inline void spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
    *flags = local_irq_save();
    spin_lock(lock);
}

/*
 * Spinlock unlock with IRQ restore
 */
static inline void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
    spin_unlock(lock);
    local_irq_restore(flags);
}

/*
 * Spinlock with IRQ disable
 */
static inline void spin_lock_irq(spinlock_t *lock)
{
    local_irq_disable();
    spin_lock(lock);
}

/*
 * Spinlock unlock with IRQ enable
 */
static inline void spin_unlock_irq(spinlock_t *lock)
{
    spin_unlock(lock);
    local_irq_enable();
}

/*
 * Read-write lock initialization
 */
static inline void rwlock_init(rwlock_t *lock)
{
    lock->lock = 0;
    lock->readers = 0;
}

/*
 * Acquire read lock
 */
static inline void read_lock(rwlock_t *lock)
{
    while (1) {
        while (lock->lock) {
            cpu_relax();
        }
        __sync_add_and_fetch(&lock->readers, 1);
        if (!lock->lock) {
            break;
        }
        __sync_sub_and_fetch(&lock->readers, 1);
    }
}

/*
 * Release read lock
 */
static inline void read_unlock(rwlock_t *lock)
{
    __sync_sub_and_fetch(&lock->readers, 1);
}

/*
 * Acquire write lock
 */
static inline void write_lock(rwlock_t *lock)
{
    while (!__sync_bool_compare_and_swap(&lock->lock, 0, 1)) {
        cpu_relax();
    }
    while (lock->readers > 0) {
        cpu_relax();
    }
}

/*
 * Release write lock
 */
static inline void write_unlock(rwlock_t *lock)
{
    __sync_synchronize();
    lock->lock = 0;
}

/*
 * Try to acquire read lock
 */
static inline int read_trylock(rwlock_t *lock)
{
    if (lock->lock) {
        return 0;
    }
    __sync_add_and_fetch(&lock->readers, 1);
    if (lock->lock) {
        __sync_sub_and_fetch(&lock->readers, 1);
        return 0;
    }
    return 1;
}

/*
 * Try to acquire write lock
 */
static inline int write_trylock(rwlock_t *lock)
{
    if (!__sync_bool_compare_and_swap(&lock->lock, 0, 1)) {
        return 0;
    }
    if (lock->readers > 0) {
        lock->lock = 0;
        return 0;
    }
    return 1;
}

/*
 * Read lock with IRQ save
 */
static inline void read_lock_irqsave(rwlock_t *lock, unsigned long *flags)
{
    *flags = local_irq_save();
    read_lock(lock);
}

/*
 * Read unlock with IRQ restore
 */
static inline void read_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
{
    read_unlock(lock);
    local_irq_restore(flags);
}

/*
 * Write lock with IRQ save
 */
static inline void write_lock_irqsave(rwlock_t *lock, unsigned long *flags)
{
    *flags = local_irq_save();
    write_lock(lock);
}

/*
 * Write unlock with IRQ restore
 */
static inline void write_unlock_irqrestore(rwlock_t *lock, unsigned long flags)
{
    write_unlock(lock);
    local_irq_restore(flags);
}

#endif /* SPINLOCK_H */
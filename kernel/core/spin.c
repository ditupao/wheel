#include <wheel.h>

void raw_spin_take(spin_t * lock) {
    u32 tkt = atomic32_inc(&lock->tkt);
    while (atomic32_get(&lock->svc) != tkt) {
        cpu_relax();
    }
}

void raw_spin_give(spin_t * lock) {
    atomic32_inc(&lock->svc);
}

// if task and isr use the very same spinlock,
// then use the `irq` version of take and give.

u32 irq_spin_take(spin_t * lock) {
    u32 key = int_lock();
    u32 tkt = atomic32_inc(&lock->tkt);
    while (atomic32_get(&lock->svc) != tkt) {
        // int_unlock(key);
        cpu_relax();
        // key = int_lock();
    }
    return key;
}

void irq_spin_give(spin_t * lock, u32 key) {
    atomic32_inc(&lock->svc);
    int_unlock(key);
}

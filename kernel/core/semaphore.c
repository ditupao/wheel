#include <wheel.h>

// total count is limited, `count` must be larger or equal to 0
// if count == 0, that means no more free resource
// P (proberen), a.k.a. take / down
// V (verhogen), a.k.a. give / up

void semaphore_init(semaphore_t * sem, int limit, int count) {
    dbg_assert(count <= limit);

    sem->lock   = SPIN_INIT;
    sem->limit  = limit;
    sem->count  = count;
    sem->pend_q = DLLIST_INIT;
}

// return OK if successfully taken the semaphore
// return ERROR if failed (might block)
int semaphore_take(semaphore_t * sem, int timeout) {
    u32 key = irq_spin_take(&sem->lock);
    if (sem->count) {
        --sem->count;
        irq_spin_give(&sem->lock, key);
        return OK;
    } else {
        task_t * self = thiscpu_var(tid_prev);

        u32 key = int_lock();
        task_stop(self, TS_PEND);
        dl_push_tail(&sem->pend_q, &self->node);
        irq_spin_give(&sem->lock, key);
        int_unlock(key);

        // hand over cpu to other task
        task_switch();

        return OK;
    }
}

int semaphore_trytake(semaphore_t * sem) {
    u32 key = irq_spin_take(&sem->lock);
    if (sem->count) {
        --sem->count;
        irq_spin_give(&sem->lock, key);
        return OK;
    }
    return ERROR;
}

void semaphore_give(semaphore_t * sem) {
    u32 key = irq_spin_take(&sem->lock);
    dlnode_t * dl = dl_pop_head(&sem->pend_q);
    if (NULL == dl) {
        if (sem->count < sem->limit) {
            ++sem->count;
        }
        irq_spin_give(&sem->lock, key);
        return;
    }

    task_t * next = PARENT(dl, task_t, node);
    task_cont(next, TS_PEND);
    irq_spin_give(&sem->lock, key);

    if (cpu_index() == next->cpu_idx) {
        task_switch();
    } else {
        smp_reschedule(next->cpu_idx);
    }
}

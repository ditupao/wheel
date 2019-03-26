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

void semaphore_destroy(semaphore_t * sem) {
    u32 key = irq_spin_take(&sem->lock);

    for (dlnode_t * dl = sem->pend_q.head; NULL != dl; dl = dl->next) {
        task_t * tid = PARENT(dl, task_t, node);

        raw_spin_take(&tid->lock);
        tid->ret_val = ERROR;
        task_cont(tid, TS_PEND);
        raw_spin_give(&tid->lock);
    }
}

// executed in ISR
static void semaphore_timeout(task_t * tid, semaphore_t * sem) {
    u32 key = irq_spin_take(&tid->lock);
    raw_spin_take(&sem->lock);

    task_cont(tid, TS_PEND);
    dl_remove(&sem->pend_q, &tid->node);

    raw_spin_give(&sem->lock);
    irq_spin_give(&tid->lock, key);

    // we're running in ISR, no need to `task_switch()`
}

// return OK if successfully taken the semaphore
// return ERROR if failed (might block)
int semaphore_take(semaphore_t * sem, int timeout) {
    task_t * tid = thiscpu_var(tid_prev);
    u32 key = irq_spin_take(&tid->lock);
    raw_spin_take(&sem->lock);
    if (sem->count) {
        --sem->count;
        raw_spin_give(&sem->lock);
        irq_spin_give(&tid->lock, key);
        return OK;
    } else {
        // resource not available, pend current task
        tid->ret_val = OK;
        task_stop(tid, TS_PEND);
        // TODO: priority-based or FIFO?
        dl_push_tail(&sem->pend_q, &tid->node);
        tid->queue = &sem->pend_q;

        // create timeout watchdog
        if (0 != timeout) {
            wdog_cancel(&tid->wdog);
            wdog_start(&tid->wdog, timeout, semaphore_timeout, tid, sem, 0,0);
        }

        // hand over control to other task
        raw_spin_give(&sem->lock);
        irq_spin_give(&tid->lock, key);
        task_switch();

        // return value set by others
        return tid->ret_val;
    }
}

int semaphore_trytake(semaphore_t * sem) {
    task_t * tid = thiscpu_var(tid_prev);
    u32 key = irq_spin_take(&tid->lock);
    raw_spin_take(&sem->lock);

    if (sem->count) {
        --sem->count;
        raw_spin_give(&sem->lock);
        irq_spin_give(&tid->lock, key);
        return OK;
    }

    raw_spin_give(&sem->lock);
    irq_spin_give(&tid->lock, key);
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

    task_t * tid = PARENT(dl, task_t, node);
    u32      cpu = tid->cpu_idx;
    raw_spin_take(&tid->lock);

    task_cont(tid, TS_PEND);
    raw_spin_give(&tid->lock);
    irq_spin_give(&sem->lock, key);

    if (cpu_index() == cpu) {
        task_switch();
    } else {
        smp_reschedule(cpu);
    }
}

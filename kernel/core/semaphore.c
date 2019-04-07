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

// resume all pending tasks on this semaphore
void semaphore_destroy(semaphore_t * sem) {
    u32 key = irq_spin_take(&sem->lock);

    for (dlnode_t * dl = sem->pend_q.head; NULL != dl; dl = dl->next) {
        task_t * tid = PARENT(dl, task_t, dl_sched);
        u32      cpu = tid->cpu_idx;

        raw_spin_take(&tid->lock);
        sched_cont(tid, TS_PEND);
        tid->ret_val = ERROR;
        raw_spin_give(&tid->lock);

        if (cpu_index() != cpu) {
            smp_reschedule(cpu);
        }
    }

    irq_spin_give(&sem->lock, key);
    task_switch();
}

// executed in ISR
static void semaphore_timeout(semaphore_t * sem, task_t * tid) {
    u32 key = irq_spin_take(&sem->lock);
    raw_spin_take(&tid->lock);

    // check whether task is pending
    if (OK == sched_cont(tid, TS_PEND)) {
        dl_remove(&sem->pend_q, &tid->dl_sched);
    }

    raw_spin_give(&tid->lock);
    irq_spin_give(&sem->lock, key);

    // we're running in ISR, no need to `task_switch()`
}

// return OK if successfully taken the semaphore
// return ERROR if failed (might block)
// this function cannot be called inside ISR
int semaphore_take(semaphore_t * sem, int timeout) {
    u32 key = irq_spin_take(&sem->lock);

    if (sem->count) {
        --sem->count;
        irq_spin_give(&sem->lock, key);
        return OK;
    }

    // resource not available, pend current task
    task_t * tid = thiscpu_var(tid_prev);
    raw_spin_take(&tid->lock);

    sched_stop(tid, TS_PEND);
    tid->ret_val = OK;
    dl_push_tail(&sem->pend_q, &tid->dl_sched); // TODO: priority-based or FIFO?
    tid->queue = &sem->pend_q;

    wdog_t wd;
    wdog_init(&wd);

    // create timeout watchdog
    if (SEM_WAIT_FOREVER != timeout) {
        wdog_start(&wd, timeout, semaphore_timeout, sem, tid, 0,0);
    }

    // release locks
    raw_spin_give(&tid->lock);
    irq_spin_give(&sem->lock, key);

    // pend here
    task_switch();

    // possible reasons for unpending:
    // - successfully taken the semaphore
    // - semaphore got destroyed
    // - timeout

    // cancel watch dog for safety
    wdog_cancel(&wd);

    // in linux, we have to remove current tid from pend_q if timed out
    // in wheel, tid got auto removed from pend_q during sched_cont
    return tid->ret_val;
}

// this function can be called inside ISR
int semaphore_trytake(semaphore_t * sem) {
    task_t * tid = thiscpu_var(tid_prev);

    u32 key = irq_spin_take(&sem->lock);
    raw_spin_take(&tid->lock);

    if (sem->count) {
        --sem->count;
        raw_spin_give(&tid->lock);
        irq_spin_give(&sem->lock, key);
        return OK;
    }

    raw_spin_give(&tid->lock);
    irq_spin_give(&sem->lock, key);
    return ERROR;
}

// this function can be called inside ISR
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

    task_t * tid = PARENT(dl, task_t, dl_sched);
    u32      cpu = tid->cpu_idx;
    raw_spin_take(&tid->lock);
    sched_cont(tid, TS_PEND);
    raw_spin_give(&tid->lock);
    irq_spin_give(&sem->lock, key);

    if (cpu_index() == cpu) {
        task_switch();
    } else {
        smp_reschedule(cpu);
    }
}

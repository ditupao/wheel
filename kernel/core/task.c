#include <wheel.h>

// `tid_next` is actually the head of ready queue, so it's also
// under the protection of `ready_q.lock`.

typedef struct ready_q {
    spin_t   lock;
    u32      priorities;
    dllist_t q[PRIORITY_COUNT];
} ready_q_t;

__PERCPU task_t  * tid_prev;
__PERCPU task_t  * tid_next;     // also protected by ready_q.lock
__PERCPU u32       no_preempt;
__PERCPU ready_q_t ready_q;

//------------------------------------------------------------------------------
// task entry point, all task start from here

typedef int (* task_proc_t) (void * a1, void * a2, void * a3, void * a4);

__NORETURN void task_entry(void * proc, void * a1, void * a2, void * a3, void * a4) {
    ((task_proc_t) proc) (a1, a2, a3, a4);
    while (1) {}
}

//------------------------------------------------------------------------------
// task creation and destroy

// create new task
// TODO: kernel stack size should be configurable
// TODO: also save tid in page_array (kernel object always in higher half,
//       8-bytes aligned? 47-3=45 bits, so only 45 bits is enough. If we
//       want to use less bits, we can allocate tcb only from one pool)
void task_init(task_t * tid, process_t * pid, u32 priority, u32 cpu_idx,
               void * proc, void * a1, void * a2, void * a3, void * a4) {
    dbg_assert(NULL != tid);
    dbg_assert(priority < PRIORITY_COUNT);
    dbg_assert(cpu_idx < cpu_installed);

    pfn_t pn_stk = page_block_alloc(ZONE_DMA|ZONE_NORMAL, 4);
    if (NO_PAGE == pn_stk) {
        // memory not enough, cannot create task
        return;
    }
    for (pfn_t i = 0; i < 16; ++i) {
        page_array[pn_stk + i].type = PT_KSTACK;
    }

    usize va_stk = (usize) phys_to_virt((usize) pn_stk << PAGE_SHIFT);
    regs_init(&tid->regs, pid->ctx, va_stk + PAGE_SIZE * 16, proc, a1, a2, a3, a4);

    tid->lock     = SPIN_INIT;
    tid->pid      = pid;
    tid->priority = priority;
    tid->cpu_idx  = cpu_idx;
    tid->state    = TS_SUSPEND;
    tid->stack    = pn_stk;
    tid->node     = DLNODE_INIT;
    tid->queue    = NULL;
    wdog_init(&tid->wdog);
}

// remove a task, mark it as deleted
void task_destroy(task_t * tid) {
    dbg_assert(NULL != tid);

    u32 key = int_lock();
    task_stop(tid, TS_DELETE);
    int_unlock(key);

    // TODO: register work function to delete TCB

    if (cpu_index() == tid->cpu_idx) {
        task_switch();
    } else {
        smp_reschedule(tid->cpu_idx);
    }
}

//------------------------------------------------------------------------------
// task state switching
// caller need to lock interrupt, or risk being switched-out
// following operations need to be carried out when interrupts locked
// after unlocking interrupts, call `task_switch` or `smp_reschedule` manually

// add bits to `tid->state`, possibly stopping the task.
// return ERROR if already stopped.
// this function only updates `tid`, `ready_q`, and `tid_next` only,
int task_stop(task_t * tid, u32 state) {
    // lock cpu interrupt and target task
    // u32 key = int_lock();
    raw_spin_take(&tid->lock);

    // change state, return if already stopped
    u32 old_state = tid->state;
    tid->state |= state;
    if (TS_READY != old_state) {
        raw_spin_give(&tid->lock);
        // int_unlock(key);
        return ERROR;
    }

    // lock ready queue of target cpu
    u32         cpu = tid->cpu_idx;
    u32         pri = tid->priority;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // remove task from ready queue
    dl_remove(&rdy->q[pri], &tid->node);
    if ((NULL == rdy->q[pri].head) && (NULL == rdy->q[pri].tail)) {
        rdy->priorities &= ~(1U << pri);
    }

    // if the task is running, pick a new one
    if (tid == percpu_var(cpu, tid_next)) {
        dbg_assert(0 != rdy->priorities);
        u32 pri = CTZ32(rdy->priorities);
        task_t * cand = PARENT(rdy->q[pri].head, task_t, node);
        percpu_var(cpu, tid_next) = cand;
    }

    // release all locks
    raw_spin_give(&rdy->lock);  // after this point, `tid_next` might be changed again
    raw_spin_give(&tid->lock);
    // int_unlock(key);            // after this point, this task might be switched out
    return OK;
}

// remove bits from `tid->state`, possibly resuming the task.
// return ERROR if already running.
// this function only updates `tid`, `ready_q`, and `tid_next` only,
// caller should call `task_switch` or `smp_reschedule` manually.
int task_cont(task_t * tid, u32 state) {
    // lock cpu interrupt and target task
    // u32 key = int_lock();
    raw_spin_take(&tid->lock);

    // change state, return if already running
    u32 old_state = tid->state;
    tid->state &= ~state;
    if ((TS_READY == old_state) || (TS_READY != tid->state)) {
        raw_spin_give(&tid->lock);
        // int_unlock(key);
        return ERROR;
    }

    // lock ready queue
    u32         cpu = tid->cpu_idx;
    u32         pri = tid->priority;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // put task back into ready queue
    rdy->priorities |= 1U << pri;
    tid->queue = &rdy->q[pri];
    dl_push_tail(tid->queue, &tid->node);

    // check whether we can preempt
    task_t * old = percpu_var(cpu, tid_next);
    if (pri < old->priority) {
        percpu_var(cpu, tid_next) = tid;
    }

    // release all locks
    raw_spin_give(&rdy->lock);
    raw_spin_give(&tid->lock);
    // int_unlock(key);
    return OK;
}

// suspend the task execution
void task_suspend(task_t * tid) {
    dbg_assert(NULL != tid);

    u32 key = int_lock();
    task_stop(tid, TS_SUSPEND);
    int_unlock(key);

    if (cpu_index() == tid->cpu_idx) {
        task_switch();
    } else {
        smp_reschedule(tid->cpu_idx);
    }
}

// resume the execution of a task
void task_resume(task_t * tid) {
    dbg_assert(NULL != tid);

    u32 key = int_lock();
    task_cont(tid, TS_SUSPEND);
    int_unlock(key);

    if (cpu_index() == tid->cpu_idx) {
        task_switch();
    } else {
        smp_reschedule(tid->cpu_idx);
    }
}

// put the task into sleep for a few ticks
void task_delay(task_t * tid, int ticks) {
    dbg_assert(NULL != tid);

    u32 key = int_lock();
    task_stop(tid, TS_DELAY);
    wdog_cancel(&tid->wdog);
    wdog_start(&tid->wdog, ticks, task_wakeup, tid, 0,0,0);
    int_unlock(key);

    if (cpu_index() == tid->cpu_idx) {
        task_switch();
    } else {
        smp_reschedule(tid->cpu_idx);
    }
}

void task_wakeup(task_t * tid) {
    task_cont(tid, TS_DELAY);
}

//------------------------------------------------------------------------------

__INIT void task_lib_init() {
    for (u32 i = 0; i < cpu_installed; ++i) {
        percpu_var(i, tid_prev) = NULL;
        percpu_var(i, tid_prev) = NULL;
        percpu_var(i, no_preempt) = 0;

        ready_q_t * rdy = percpu_ptr(i, ready_q);
        rdy->lock = SPIN_INIT;
        rdy->priorities = 0;
        for (u32 p = 0; p < PRIORITY_COUNT; ++p) {
            rdy->q[p] = DLLIST_INIT;
        }
    }
}

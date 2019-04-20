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
static   pool_t    tcb_pool;

//------------------------------------------------------------------------------
// low level scheduling, task state switching
// caller need to lock interrupt, or risk being switched-out
// caller also need to lock target tid, or might be deleted by others
// following operations need to be carried out when interrupts locked
// after unlocking interrupts, call `task_switch` or `smp_reschedule` manually

// add bits to `tid->state`, possibly stopping the task.
// return previous task state.
// this function only updates `tid`, `ready_q`, and `tid_next` only,
u32 sched_stop(task_t * tid, u32 state) {
    // change state, return if already stopped
    u32 old_state = tid->state;
    tid->state |= state;
    if (TS_READY != old_state) {
        return old_state;
    }

    // lock ready queue of target cpu
    u32         cpu = tid->cpu_idx;
    u32         pri = tid->priority;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // remove task from ready queue
    dl_remove(&rdy->q[pri], &tid->dl_sched);
    if ((NULL == rdy->q[pri].head) && (NULL == rdy->q[pri].tail)) {
        rdy->priorities &= ~(1U << pri);
    }

    // if the task is running, pick a new one
    if (tid == percpu_var(cpu, tid_next)) {
        dbg_assert(0 != rdy->priorities);
        u32 pri = CTZ32(rdy->priorities);
        task_t * cand = PARENT(rdy->q[pri].head, task_t, dl_sched);
        percpu_var(cpu, tid_next) = cand;
    }

    // after this point, `tid_next` might be changed again
    raw_spin_give(&rdy->lock);
    return old_state;
}

// remove bits from `tid->state`, possibly resuming the task.
// return previous task state.
// this function only updates `tid`, `ready_q`, and `tid_next` only,
// caller should call `task_switch` or `smp_reschedule` manually.
u32 sched_cont(task_t * tid, u32 state) {
    // change state, return if already running
    u32 old_state = tid->state;
    tid->state &= ~state;
    if ((TS_READY == old_state) || (TS_READY != tid->state)) {
        return old_state;
    }

    // lock ready queue
    int         cpu = tid->cpu_idx;
    int         pri = tid->priority;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // put task back into ready queue
    rdy->priorities |= 1U << pri;
    tid->queue = &rdy->q[pri];
    dl_push_tail(tid->queue, &tid->dl_sched);

    // check whether we can preempt
    task_t * old = percpu_var(cpu, tid_next);
    if (pri < old->priority) {
        percpu_var(cpu, tid_next) = tid;
    }

    // after this point, `tid_next` might be changed again
    raw_spin_give(&rdy->lock);
    return old_state;
}

//------------------------------------------------------------------------------
// task operations

// create new task
// TODO: kernel stack size should be configurable
// TODO: also save tid in page_array (kernel object always in higher half,
//       8-bytes aligned? 47-3=45 bits, so only 45 bits is enough. If we
//       want to use less bits, we can allocate tcb only from one pool)
task_t * task_create(process_t * pid, int priority, int cpu_idx,
                     void * proc, void * a1, void * a2, void * a3, void * a4) {
    dbg_assert((0 <= priority) && (priority < PRIORITY_COUNT));
    dbg_assert((0 <= cpu_idx)  && (cpu_idx  < cpu_installed));

    // allocate
    task_t * tid = pool_obj_alloc(&tcb_pool);

    // allocate space for kernel stack, must be continuous
    pfn_t pstk = page_block_alloc(ZONE_DMA|ZONE_NORMAL, 4);
    if (NO_PAGE == pstk) {
        // memory not enough, cannot create task
        return NULL;
    }

    // mark allocated page as kstack type
    for (pfn_t i = 0; i < 16; ++i) {
        page_array[pstk + i].type  = PT_KSTACK;
        page_array[pstk + i].block = 0;
    }

    usize vstk = (usize) phys_to_virt((usize) pstk << PAGE_SHIFT);
    regs_init(&tid->regs, pid->vm.ctx, vstk + PAGE_SIZE * 16, proc, a1, a2, a3, a4);

    tid->lock      = SPIN_INIT;
    tid->state     = TS_SUSPEND;
    tid->ret_val   = 0;
    tid->priority  = priority;
    tid->cpu_idx   = cpu_idx;
    tid->timeslice = 200;   // TODO: put default timeslice in config
    tid->remaining = 200;
    tid->kstack    = PGLIST_INIT;
    tid->ustack    = NULL;
    tid->dl_sched  = DLNODE_INIT;
    tid->queue     = NULL;
    tid->dl_proc   = DLNODE_INIT;
    tid->process   = pid;

    page_array[pstk].block = 1;
    page_array[pstk].order = 4;
    pglist_push_head(&tid->kstack, pstk);

    // put the task into process, and update resource list
    u32 key = irq_spin_take(&pid->lock);
    dl_push_tail(&pid->tasks, &tid->dl_proc);
    irq_spin_give(&pid->lock, key);

    return tid;
}

// work function to be called after task_exit
static void task_cleanup(task_t * tid) {
    dbg_assert(TS_ZOMBIE == tid->state);

    // unmap and remove vm region for user stack
    vmspace_unmap(&tid->process->vm, tid->ustack);
    vmspace_free (&tid->process->vm, tid->ustack);
    tid->ustack = NULL;

    // free all pages in kernel stack
    pglist_free_all(&tid->kstack);

    dl_remove(&tid->process->tasks, &tid->dl_proc);
    if ((NULL == tid->process->tasks.head) &&
        (NULL == tid->process->tasks.tail)) {
        // we can only create new thread from the same process
        // if this is the last thread, meaning the process is finished
        process_delete(tid->process);
    }

    // TODO: signal parent for finish
    //       and wait for parent task to release the tcb
    pool_obj_free(&tcb_pool, tid);
}

// mark current task as deleted
void task_exit() {
    task_t * tid = thiscpu_var(tid_prev);

    u32 key = irq_spin_take(&tid->lock);
    sched_stop(tid, TS_ZOMBIE);
    irq_spin_give(&tid->lock, key);

    // register work function to free kernel stack pages
    work_enqueue(task_cleanup, tid, 0,0,0);

    task_switch();
}

// this function might be called during tick_advance
// task state not changed, no need to lock current tid
void task_yield() {
    task_t    * tid = thiscpu_var(tid_prev);
    ready_q_t * rdy = thiscpu_ptr(ready_q);

    u32 key = irq_spin_take(&rdy->lock);

    dl_remove(tid->queue, &tid->dl_sched);
    dl_push_tail(tid->queue, &tid->dl_sched);

    dbg_assert(0 != rdy->priorities);
    u32 pri = CTZ32(rdy->priorities);
    task_t * cand = PARENT(rdy->q[pri].head, task_t, dl_sched);
    thiscpu_var(tid_next) = cand;

    // dbg_print("<%p->%p>", tid, cand);
    irq_spin_give(&rdy->lock, key);

    task_switch();
}

// suspend current task
void task_suspend() {
    task_t * tid = thiscpu_var(tid_prev);

    u32 key = irq_spin_take(&tid->lock);
    sched_stop(tid, TS_SUSPEND);
    irq_spin_give(&tid->lock, key);

    task_switch();
}

// resume the execution of a suspended task
void task_resume(task_t * tid) {
    dbg_assert(NULL != tid);

    u32 key = irq_spin_take(&tid->lock);
    u32 cpu = tid->cpu_idx;
    u32 ts  = sched_cont(tid, TS_SUSPEND);
    irq_spin_give(&tid->lock, key);

    if (TS_READY == ts) {
        return;
    }

    if (cpu_index() == cpu) {
        task_switch();
    } else {
        smp_reschedule(cpu);
    }
}

void task_delay(int ticks) {
    wdog_t   wd;
    task_t * tid = thiscpu_var(tid_prev);
    u32      key = irq_spin_take(&tid->lock);

    wdog_init(&wd);
    sched_stop(tid, TS_DELAY);
    wdog_start(&wd, ticks, task_wakeup, tid, 0,0,0);
    irq_spin_give(&tid->lock, key);

    task_switch();
    wdog_cancel(&wd);
}

void task_wakeup(task_t * tid) {
    dbg_assert(NULL != tid);

    u32 key = irq_spin_take(&tid->lock);
    u32 cpu = tid->cpu_idx;
    u32 ts  = sched_cont(tid, TS_DELAY);
    irq_spin_give(&tid->lock, key);

    if (TS_READY == ts) {
        return;
    }

    if (cpu_index() == cpu) {
        task_switch();
    } else {
        smp_reschedule(cpu);
    }
}

//------------------------------------------------------------------------------
// module setup

__INIT void task_lib_init() {
    for (int i = 0; i < cpu_installed; ++i) {
        percpu_var(i, tid_prev) = NULL;
        percpu_var(i, tid_prev) = NULL;
        percpu_var(i, no_preempt) = 0;

        ready_q_t * rdy = percpu_ptr(i, ready_q);
        rdy->lock = SPIN_INIT;
        rdy->priorities = 0;
        for (int p = 0; p < PRIORITY_COUNT; ++p) {
            rdy->q[p] = DLLIST_INIT;
        }
    }

    pool_init(&tcb_pool, sizeof(task_t));
}

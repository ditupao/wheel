#include <wheel.h>


// we use the sum of timeslice to measure cpu load
static spin_t load_lock [PRIORITY_COUNT];
static int    least_load[PRIORITY_COUNT];
static int    least_cpu [PRIORITY_COUNT];

// ready queue, container of ready tasks
typedef struct ready_q {
    spin_t   lock;
    u32      priorities;
    dllist_t tasks[PRIORITY_COUNT];
    int      load [PRIORITY_COUNT]; // protected by load_lock[pri];
} ready_q_t;

// `tid_next` is actually the head of ready queue, so it's also
// under the protection of `ready_q.lock`.
__PERCPU task_t  * tid_prev;
__PERCPU task_t  * tid_next;        // protected by ready_q.lock
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
    u32         cpu = tid->last_cpu;
    u32         pri = tid->priority;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // decrease load of current cpu
    raw_spin_take(&load_lock[pri]);
    rdy->load[pri] -= tid->timeslice;
    if (rdy->load[pri] < least_load[pri]) {
        least_load[pri] = rdy->load[pri];
        least_cpu [pri] = cpu;
    }
    raw_spin_give(&load_lock[pri]);

    // remove task from ready queue
    dl_remove(&rdy->tasks[pri], &tid->dl_sched);
    if ((NULL == rdy->tasks[pri].head) && (NULL == rdy->tasks[pri].tail)) {
        rdy->priorities &= ~(1U << pri);
    }

    // if the task is running, pick a new one
    if (tid == percpu_var(cpu, tid_next)) {
        dbg_assert(0 != rdy->priorities);
        u32 pri = CTZ32(rdy->priorities);
        task_t * cand = PARENT(rdy->tasks[pri].head, task_t, dl_sched);
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

    int      pri = tid->priority;
    int      cpu = tid->last_cpu;   // old cpu
    cpuset_t aff = tid->affinity;

    // pick a new cpu based on load
    // if the difference of load is not so significant
    // then still execute on the old cpu
    cpuset_t mask = (1UL << cpu_activated) - 1;
    int      cand = 0;
    int      load = 0x7fffffff;

    // pick cpu based on load
    raw_spin_take(&load_lock[pri]);
    int min_load = least_load[pri];
    int min_cand = least_cpu[pri];
    raw_spin_give(&load_lock[pri]);

    aff &= mask;
    if ((0 == aff) || (mask == aff)) {
        load = min_load;
        cand = min_cand;
    } else {
        while (aff) {
            cpuset_t    next = aff & (aff - 1);
            int         idx  = CTZ64(aff ^ next);
            ready_q_t * rdy  = percpu_ptr(idx, ready_q);
            raw_spin_take(&rdy->lock);
            if (rdy->load[pri] < load) {
                load = rdy->load[pri];
                cand = idx;
            }
            raw_spin_give(&rdy->lock);
            aff = next;
        }
    }

    aff = tid->affinity & mask;
    if ((cpu < 0) || (cpu >= cpu_activated)) {
        // last cpu index not valid, use the candidate
        cpu = cand;
    } else if ((0 != aff) && (0 == (aff & (1UL << cpu)))) {
        // last cpu is not affinitied, use the candidate
        cpu = cand;
    } else if (cand != cpu) {
        // TODO: if load difference is not so significant
        //       then we'll still use the old cpu
        cpu = cand;
    }

    // now `cpu` is the target cpu this task will execute on
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // increase load on this ready queue
    raw_spin_take(&load_lock[pri]);
    rdy->load[pri] += tid->timeslice;
    if (least_cpu[pri] == cpu) {
        // if current cpu is the least loaded one
        // also increase least_load
        // current cpu might no longer be the least loaded one
        // but we don't care, that'll be fixed by other cpu
        least_load[pri] += tid->timeslice;
    } else if (rdy->load[pri] < least_load[pri]) {
        least_load[pri] = rdy->load[pri];
        least_cpu [pri] = cpu;
    }
    raw_spin_give(&load_lock[pri]);

    // put task back into ready queue
    rdy->priorities |= 1U << pri;
    tid->queue = &rdy->tasks[pri];
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
// regist idle task of specific cpu

// __INIT void sched_regist_idle(int cpu, task_t * tid) {
//     ready_q_t * rdy = percpu_ptr(cpu, ready_q);
// }

//------------------------------------------------------------------------------
// task operations

// create new task
// TODO: kernel stack size should be configurable
// TODO: also save tid in page_array (kernel object always in higher half,
//       8-bytes aligned? 47-3=45 bits, so only 45 bits is enough. If we
//       want to use less bits, we can allocate tcb only from one pool)
task_t * task_create(int priority, cpuset_t affinity, void * proc,
                     void * a1, void * a2, void * a3, void * a4) {
    dbg_assert((0 <= priority) && (priority < PRIORITY_COUNT));

    // allocate tcb
    task_t * tid = pool_obj_alloc(&tcb_pool);

    // allocate space for kernel stack, must be continuous
    pfn_t kstk = page_block_alloc(ZONE_DMA|ZONE_NORMAL, 4);
    if (NO_PAGE == kstk) {
        // memory not enough, cannot create task
        return NULL;
    }

    // mark allocated page as kstack type
    for (pfn_t i = 0; i < 16; ++i) {
        page_array[kstk + i].type  = PT_KSTACK;
        page_array[kstk + i].block = 0;
        page_array[kstk + i].order = 4;
    }
    page_array[kstk].block = 1;
    page_array[kstk].order = 4;

    // setup register info on the new stack
    usize vstk = (usize) phys_to_virt((usize) kstk << PAGE_SHIFT);
    regs_init(&tid->regs, vstk + PAGE_SIZE * 16, proc, a1, a2, a3, a4);

    // fill task control block
    tid->lock      = SPIN_INIT;
    tid->state     = TS_SUSPEND;
    tid->ret_val   = 0;
    tid->priority  = priority;
    tid->affinity  = affinity;
    tid->last_cpu  = 0;
    tid->timeslice = 200;   // TODO: put default timeslice in config
    tid->remaining = 200;
    tid->kstack    = kstk;
    tid->ustack    = NULL;
    tid->dl_sched  = DLNODE_INIT;
    tid->queue     = NULL;
    tid->dl_proc   = DLNODE_INIT;
    tid->process   = NULL;

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
    page_block_free(tid->kstack, 4);

    dl_remove(&tid->process->tasks, &tid->dl_proc);
    if ((NULL == tid->process->tasks.head) &&
        (NULL == tid->process->tasks.tail)) {
        // we can only create new thread from the same process
        // if this is the last thread, meaning the process is finished
        process_delete(tid->process);
    }

    // TODO: signal parent for finish and wait
    // for the parent task to release this tcb
    pool_obj_free(&tcb_pool, tid);
}

// mark current task as deleted, this function don't return
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
    task_t * cand = PARENT(rdy->tasks[pri].head, task_t, dl_sched);
    thiscpu_var(tid_next) = cand;

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
    u32 ts  = sched_cont(tid, TS_SUSPEND);
    irq_spin_give(&tid->lock, key);

    if (TS_READY == ts) {
        return;
    }

    if (cpu_index() == tid->last_cpu) {
        task_switch();
    } else {
        smp_reschedule(tid->last_cpu);
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
    u32 ts  = sched_cont(tid, TS_DELAY);
    irq_spin_give(&tid->lock, key);

    if (TS_READY == ts) {
        return;
    }

    if (cpu_index() == tid->last_cpu) {
        task_switch();
    } else {
        smp_reschedule(tid->last_cpu);
    }
}

//------------------------------------------------------------------------------
// module setup

__INIT void task_lib_init() {
    for (int i = 0; i < cpu_installed; ++i) {
        percpu_var(i, tid_prev)   = NULL;
        percpu_var(i, tid_prev)   = NULL;
        percpu_var(i, no_preempt) = 0;

        ready_q_t * rdy = percpu_ptr(i, ready_q);
        rdy->lock       = SPIN_INIT;
        rdy->priorities = 0;
        for (int p = 0; p < PRIORITY_COUNT; ++p) {
            rdy->tasks[p] = DLLIST_INIT;
            rdy->load [p] = 0;
        }
    }

    for (int p = 0; p < PRIORITY_COUNT; ++p) {
        least_load[p] = 0;
        least_cpu [p] = 0;
    }

    pool_init(&tcb_pool, sizeof(task_t));
}

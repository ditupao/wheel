#include <wheel.h>

// priority assignment:
// 0~29: real time tasks
// 30:   complete fair scheduling
// 31:   idle task

// we use the sum of timeslice to measure cpu load
static spin_t load_lock [PRIORITY_COUNT];
static int    least_load[PRIORITY_COUNT];
static int    least_cpu [PRIORITY_COUNT];

typedef struct ready_q {
    spin_t   lock;
    u32      priorities;
    dllist_t tasks[PRIORITY_COUNT];
    int      load [PRIORITY_COUNT]; // protected by load_lock[pri];
} ready_q_t;

// typedef struct ready_q {
//     spin_t   lock;
//     u32      priorities;            // bit mask
//     dllist_t rt[PRIORITY_COUNT];    // round robin
//     task_t * fair;                  // complete fair
//     task_t * idle;                  // points to the idle tcb of this cpu
// } ready_q_t;

typedef struct pend_q {
    dllist_t rt[PRIORITY_COUNT];    // priority based
    dllist_t fair;                  // FIFO
} pend_q_t;

static __PERCPU ready_q_t ready_q;
static          rbtree_t  rb_fair;

__PERCPU u32       no_preempt;
__PERCPU task_t  * tid_prev;
__PERCPU task_t  * tid_next;        // protected by ready_q.lock

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
    u32         pri = tid->priority;
    u32         cpu = tid->last_cpu;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // // decrease load of current cpu
    // raw_spin_take(&load_lock[pri]);
    // rdy->load[pri] -= tid->timeslice;
    // if (rdy->load[pri] < least_load[pri]) {
    //     least_load[pri] = rdy->load[pri];
    //     least_cpu [pri] = cpu;
    // }
    // raw_spin_give(&load_lock[pri]);

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

    int pri = tid->priority;
    int cpu = tid->last_cpu;    // old cpu

    // take load_lock, perform load counting
    // TODO: exclude idle tasks
    raw_spin_take(&load_lock[pri]);

    cpuset_t aff  = tid->affinity;
    cpuset_t mask = (1UL << cpu_activated) - 1;
    int      cand = 0;
    int      load = 0x7fffffff;

    // choose target cpu based on load
    int min_load = least_load[pri];
    int min_cand = least_cpu [pri];

    aff &= mask;
    if ((0 == aff) || (mask == aff)) {
        // no affinity, use the least loaded one
        load = min_load;
        cand = min_cand;
    } else {
        while (aff) {
            cpuset_t    next = aff & (aff - 1);
            int         idx  = CTZ64(aff ^ next);
            ready_q_t * rdy  = percpu_ptr(idx, ready_q);
            if (rdy->load[pri] < load) {
                load = rdy->load[pri];
                cand = idx;
            }
            aff = next;
        }
    }

    aff = tid->affinity & mask;
    if ((cpu < 0) || (cpu >= cpu_activated)) {
        cpu = cand;     // last cpu not valid
    } else if ((0 != aff) && (0 == (aff & (1UL << cpu)))) {
        cpu = cand;     // last cpu not affined
    } else if (cand != cpu) {
        // TODO: if load difference is not so significant
        //       then we'll still use the old cpu
        cpu = cand;
    }

    // now `cpu` is the target cpu this task will execute on
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    rdy->load[pri] += tid->timeslice;

    // increase load on this ready queue
    if (least_cpu[pri] == cpu) {
        // current cpu might no longer be the least loaded one
        // loop through all cpu and find one
        least_load[pri] += tid->timeslice;
        for (int i = 0; i < cpu_activated; ++i) {
            ready_q_t * rdy = percpu_ptr(i, ready_q);
            if (rdy->load[pri] < least_load[pri]) {
                least_load[pri] = rdy->load[pri];
                least_cpu [pri] = i;
            }
        }
    }
    raw_spin_give(&load_lock[pri]);

    // put task back into ready queue
    raw_spin_take(&rdy->lock);
    rdy->priorities |= 1U << pri;
    tid->last_cpu    = cpu;
    tid->queue       = &rdy->tasks[pri];
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

// void sched_init(task_t * tid, int priority) {
//     tid->state    = TS_SUSPEND;
//     tid->priority = priority;
//     tid->last_cpu = -1;

//     if ((0 <= tid->priority) && (tid->priority < 30)) {
//         // realtime
//         tid->rt.timeslice = 200;
//         tid->rt.remaining = 200;
//         tid->rt.dl_sched  = DLNODE_INIT;
//     } else if (tid->priority == 30) {
//         // fair
//         tid->fair.vruntime = 0;
//         tid->fair.weight   = 1;
//         tid->fair.rb_sched = RBNODE_INIT;
//     } else {
//         dbg_print("[panic] priority not valid!\n");
//         dbg_trace();
//         while (1) {}
//     }
// }

// this function might be called during tick_advance
// task state not changed, no need to lock current tid
void sched_yield() {
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

void sched_tick() {
    // round robin (not only for non-rt tasks)
    task_t * tid = thiscpu_var(tid_prev);
    if (PRIORITY_IDLE != tid->priority) {
        if (--tid->remaining <= 0) {
            tid->remaining = tid->timeslice;
            sched_yield();
        }
    }
}

__INIT void sched_lib_init() {
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
}


#if 0

//------------------------------------------------------------------------------
// real time scheduling class, priority 0~29

// find the cpu with lowest current priority (prefer old cpu)
static int find_lowest_cpu(task_t * tid) {
    int lowest_cpu = -1;
    int lowest_pri = PRIORITY_IDLE;

    if (-1 != tid->last_cpu) {
        lowest_cpu = tid->last_cpu;
        lowest_pri = CTZ32(percpu_ptr(lowest_cpu, ready_q)->priorities);
    }

    for (int i = 0; i < cpu_activated; ++i) {
        ready_q_t * rdy = percpu_ptr(i, ready_q);
        if (CTZ32(rdy->priorities) < lowest_pri) {
            lowest_pri = CTZ32(rdy->priorities);
            lowest_cpu = i;
        }
    }

    return lowest_cpu;
}

static void rt_stop(task_t * tid) {
    // TODO: we can only stop current task
    //       so we can use thiscpu instead
    int         cpu = tid->last_cpu;
    int         pri = tid->priority;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);

    raw_spin_take(&rdy->lock);

    // remove task out from the ready queue
    dl_remove(&rdy->rt[pri], &tid->rt.dl_sched);
    if ((NULL == rdy->rt[pri].head) && (NULL == rdy->rt[pri].tail)) {
        rdy->priorities &= ~(1U << pri);
    }

    // if this task is currently running, find a new task to run
    // TODO: this might cause migration
    //       find the task with highest priority, not the highest in
    //       current ready queue
    if (tid == percpu_var(cpu, tid_next)) {
        dbg_assert(0 != rdy->priorities);
        u32 pri = CTZ32(rdy->priorities);
        task_t * cand = PARENT(rdy->rt[pri].head, task_t, dl_sched);
        percpu_var(cpu, tid_next) = cand;
    }

    raw_spin_give(&rdy->lock);
}

static void rt_cont(sched_item_t * tid) {
    int         cpu = find_lowest_cpu();
    int         pri = tid->priority;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // put task into the ready queue
    rdy->priorities |= 1U << pri;
    tid->last_cpu = cpu;
    dl_push_tail(&rdy->rt[pri], &tid->rt.dl_sched);

    // check whether we can preempt
    task_t * old = percpu_var(cpu, tid_next);
    if (pri < old->priority) {
        percpu_var(cpu, tid_next) = tid;
    }

    raw_spin_give(&rdy->lock);
}

//------------------------------------------------------------------------------
// fair scheduling class, priority 30

static void fair_stop(task_t * tid) {
    //
}

static void fair_cont(task_t * tid) {
    //
}

//------------------------------------------------------------------------------
// generic scheduling function

// caller should lock target task, also lock interrupt
int sched_stop(task_t * tid, u32 bits) {
    u32 state   = tid->state;
    tid->state |= bits;

    if (TS_READY != state) {
        // task already stopped
        return ERROR;
    }

    if ((0 <= tid->priority) && (tid->priority < 30)) {
        // realtime
        rt_stop(tid);
    } else if (tid->priority == 30) {
        // fair
    } else {
        dbg_print("[panic] priority not valid!\n");
        dbg_trace();
        while (1) {}
    }

    return OK;
}

int sched_cont(task_t * tid, u32 bits) {
    u32 state   = tid->state;
    tid->state &= ~bits;
    if ((TS_READY == state) || (TS_READY != tid->state)) {
        // already running or still not ready
        return ERROR;
    }

    if ((0 <= tid->priority) && (tid->priority < 30)) {
        // realtime, execute on the lowest cpu
        st_cont(tid);
    } else if (tid->priority == 30) {
        // fair
    } else {
        dbg_print("[panic] priority not valid!\n");
        dbg_trace();
        while (1) {}
    }

    return OK;
}

#endif

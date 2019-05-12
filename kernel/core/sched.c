#include <wheel.h>

__PERCPU task_t * tid_prev;
__PERCPU task_t * tid_next;

// priority assignment:
// 0~29: real time tasks
// 30:   complete fair scheduling
// 31:   idle task

typedef struct ready_q {
    spin_t   lock;
    u32      priorities;            // bit mask
    dllist_t rt[PRIORITY_COUNT];    // round robin
    rbtree_t fair;                  // complete fair
    task_t * idle;
} ready_q_t;

typedef struct pend_q {
    dllist_t rt[PRIORITY_COUNT];    // priority based
    dllist_t fair;                  // FIFO
} pend_q_t;

typedef struct sched_item {
    u32      state;
    int      priority;
    cpuset_t affinity;
    int      last_cpu;
    union {
        struct {
            int      timeslice;
            int      remaining;
            dlnode_t dl_sched;
        } rt;
        struct {
            int      vruntime;
            int      weight;
            rbnode_t rb_sched;
        } fair;
    };
} sched_item_t;


static __PERCPU ready_q_t ready_q;

// find the cpu with lowest current priority (prefer old cpu)
int find_lowest_cpu(sched_item_t * si) {
    int lowest_cpu = -1;
    int lowest_pri = PRIORITY_IDLE;

    if (-1 != si->last_cpu) {
        lowest_cpu = si->last_cpu;
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

// caller should lock target task, also lock interrupt
int sched_stop(sched_item_t * si, u32 bits) {
    u32 state  = si->state;
    si->state |= bits;

    if (TS_READY != state) {
        // task already stopped
        return ERROR;
    }

    if (0 <= si->priority && si->priority < 30) {
        // realtime
    } else if (si->priority == 30) {
        // fair
    } else {
        dbg_print("[panic] priority not valid!\n");
        dbg_trace();
        while (1) {}
    }

    return OK;
}

int sched_cont(sched_item_t * si, u32 bits) {
    u32 state  = si->state;
    si->state &= ~bits;
    if ((TS_READY == state) || (TS_READY != tid->state)) {
        // already running or still not ready
        return ERROR;
    }

    if (0 <= si->priority && si->priority < 30) {
        // realtime, execute on the lowest cpu
        int cpu = find_lowest_cpu();
    } else if (si->priority == 30) {
        // fair
    } else {
        dbg_print("[panic] priority not valid!\n");
        dbg_trace();
        while (1) {}
    }

    return OK;
}

//------------------------------------------------------------------------------
// real time scheduling class

void rt_stop() {
    //
}

void rt_cont(sched_item_t * si) {
    int         cpu = find_lowest_cpu();
    int         pri = si->priority;
    ready_q_t * rdy = percpu_ptr(cpu, ready_q);
    raw_spin_take(&rdy->lock);

    // put task into the ready queue
    rdy->priorities |= 1U << pri;
    si->last_cpu = cpu;
    dl_push_tail(&rdy->rt[pri], &si->rt.dl_sched);

    // check whether we can preempt
    task_t * old = percpu_var(cpu, tid_next);
    if (pri < old->priority) {
        percpu_var(cpu, tid_next) = (task_t *) si;
    }

    raw_spin_give(&rdy->lock);
}

#include <wheel.h>

typedef struct rdy {
    spin_t   lock;
    u32      priorities;
    dllist_t q[PRIORITY_COUNT];
} rdy_t;

__PERCPU task_t * tid_prev;
__PERCPU task_t * tid_next;     // also protected by rdy.lock
__PERCPU u32      no_preempt;
__PERCPU rdy_t    rdy;

//------------------------------------------------------------------------------
// task state switching

static void task_stop(task_t * tid, u32 state, void * callback,
                      void * a1, void * a2, void * a3, void * a4) {
    // lock interrupt and target task
    u32 key = int_lock();
    raw_spin_take(&tid->lock);

    // change state, return if already stopped
    u32 old_state = tid->state;
    tid->state |= state;
    if (TS_READY != old_state) {
        raw_spin_give(&tid->lock);
        int_unlock(key);
        return;
    }

    // lock ready queue of target cpu
    u32     cpu = tid->cpu_idx;
    u32     pri = tid->priority;
    rdy_t * rd  = percpu_ptr(cpu, rdy);
    dbg_assert(tid->queue == &rd->q[pri]);
    raw_spin_take(&rd->lock);

    // remove task from ready queue
    dl_remove(tid->queue, &tid->node);
    if ((NULL == tid->queue->head) && (NULL == tid->queue->tail)) {
        rd->priorities &= ~(1U << pri);
    }

    // if the task is running, pick a new one
    if (tid == percpu_var(cpu, tid_next)) {
        dbg_assert(0 != rd->priorities);
        u32 pri = CTZ32(rd->priorities);
        task_t * cand = PARENT(rd->q[pri].head, task_t, node);
        percpu_var(cpu, tid_next) = cand;
    }

    // TODO: state change finished, notify caller?

    // release all locks
    raw_spin_give(&rd->lock);
    raw_spin_give(&tid->lock);
    int_unlock(key);

    if (cpu_index() == cpu) {
        // TODO: switch task on current CPU
    } else {
        // send IPI to force task switch
    }
}

__INIT void task_lib_init() {
    for (u32 i = 0; i < cpu_installed; ++i) {
        percpu_var(i, tid_prev) = NULL;
        percpu_var(i, tid_prev) = NULL;
        percpu_var(i, no_preempt) = 0;

        rdy_t * rd = percpu_ptr(i, rdy);
        rd->lock = SPIN_INIT;
        rd->priorities = 0;
        for (u32 p = 0; p < PRIORITY_COUNT; ++p) {
            rd->q[p] = DLLIST_INIT;
        }
    }
}

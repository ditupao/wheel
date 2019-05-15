#ifndef CORE_SCHED_H
#define CORE_SCHED_H

#include <base.h>
#include <core/task.h>

typedef struct pend_q {
    u32      priorities;            // bit mask
    dllist_t tasks[PRIORITY_COUNT]; // priority based
} pend_q_t;

extern __PERCPU task_t * tid_prev;
extern __PERCPU task_t * tid_next;
extern __PERCPU u32      no_preempt;

// the last bit in `no_preempt` means there's task waiting to preempt
static inline void preempt_lock  () { thiscpu32_add(&no_preempt, 2); }
static inline void preempt_unlock() { thiscpu32_sub(&no_preempt, 2); }

extern u32  sched_stop(task_t * tid, u32 bits);
extern u32  sched_cont(task_t * tid, u32 bits);

extern void sched_yield();
extern void sched_tick ();

// requires: task, per-cpu var
extern __INIT void sched_lib_init();

#endif // CORE_SCHED_H

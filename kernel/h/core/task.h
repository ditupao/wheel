#ifndef CORE_TASK_H
#define CORE_TASK_H

#include <base.h>
#include "spin.h"
#include <libk/list.h>

typedef struct task {
    regs_t     regs;            // task context
    spin_t     lock;
    u32        priority;
    u32        cpu_idx;         // affinity cpu
    u32        state;
    pfn_t      stack;           // this is kernel stack
    dlnode_t   node;            // node in ready/pend queue
    dllist_t * queue;           // which queue is this task in
} task_t;

// 32 priorities at momst, so we can use `u32` as priority bitmask
#define PRIORITY_COUNT  32
#define PRIORITY_IDLE   31      // lowest priority = idle
#define PRIORITY_NONRT  30      // 2nd lowest priority = non-real-time

// task states
#define TS_READY        0x00    // running or runnable, in ready_q
#define TS_PEND         0x01    // waiting for something, in pend_q
#define TS_DELAY        0x02    // task delay or timeout, wdog active
#define TS_SUSPEND      0x04    // stopped on purpose, not on any q
#define TS_DELETE       0x08    // marked for delete

extern __PERCPU task_t * tid_prev;
extern __PERCPU task_t * tid_next;
extern __PERCPU u32      no_preempt;

// the last bit in `no_preempt` means there's task waiting to preempt
static inline void preempt_lock  () { thiscpu32_add(&no_preempt, 2); }
static inline void preempt_unlock() { thiscpu32_sub(&no_preempt, 2); }

extern void task_init   (task_t * tid, u32 priority, u32 cpu_idx, void * proc,
                         void * a1, void * a2, void * a3, void * a4);
extern void task_destroy(task_t * tid);
extern void task_suspend(task_t * tid);
extern void task_resume (task_t * tid);

extern __INIT void task_lib_init();

#endif // CORE_TASK_H

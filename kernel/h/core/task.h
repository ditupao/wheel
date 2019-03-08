#ifndef CORE_TASK_H
#define CORE_TASK_H

#include <base.h>
#include "spin.h"
#include <libk/list.h>

typedef struct task {
    regs_t     regs;
    spin_t     lock;
    u32        priority;
    u32        cpu_idx;
    u32        state;
    u32        options;
    pfn_t      stack;       // this is kernel stack
    dlnode_t   node;
    dllist_t * queue;
} task_t;

// 32 priorities at momst, so we can use `u32` as priority bitmask
#define PRIORITY_COUNT  32
#define PRIORITY_IDLE   31
#define PRIORITY_NONRT  30

// task states
#define TS_READY        0x00    // running or runnable, in ready_q
#define TS_PEND         0x01    // waiting for something, in pend_q
#define TS_DELAY        0x02    // task delay or timeout, wdog active
#define TS_SUSPEND      0x04    // stopped on purpose, not on any q
#define TS_DELETE       0x08    // marked for delete

extern __PERCPU task_t * tid_prev;
extern __PERCPU task_t * tid_next;
extern __PERCPU u32      no_preempt;


extern __INIT void task_lib_init();

#endif // CORE_TASK_H

#ifndef CORE_TASK_H
#define CORE_TASK_H

#include <base.h>
#include <mem/page.h>
#include <mem/vmspace.h>
#include <core/process.h>
#include <libk/spin.h>
#include <libk/list.h>

// task control block
// - kstack: used to execute syscall handler and exception handler
// - ustack: used to execute application code under user mode
typedef struct task {
    regs_t      regs;           // arch-specific status
    spin_t      lock;           // spinlock used in state-switching
    u32         state;          // task state
    int         ret_val;        // return code from PEND state
    int         priority;       // must < PRIORITY_COUNT, could use shorter type
    int         cpu_idx;        // must < cpu_installed, could use shorter type
    int         timeslice;      // total timeslice
    int         remaining;      // remaining timeslice
    pglist_t    kstack;         // kernel stack page list
    vmrange_t * ustack;         // user stack region
    dlnode_t    dl_sched;       // node in ready_q/pend_q
    dllist_t  * queue;          // current ready_q/pend_q
    dlnode_t    dl_proc;        // node in process
    process_t * process;        // current process
} task_t;

// task priorities
#define PRIORITY_COUNT  32      // we use `u32` as priority bitmask
#define PRIORITY_IDLE   31      // lowest priority = idle
#define PRIORITY_NONRT  30      // 2nd lowest priority = non-real-time

// task states
#define TS_READY        0x00    // running or runnable, in ready_q
#define TS_PEND         0x01    // waiting for something, in pend_q
#define TS_DELAY        0x02    // task delay or timeout, wdog active
#define TS_SUSPEND      0x04    // stopped on purpose, not on any q
#define TS_ZOMBIE       0x08    // finished, but TCB still present

extern __PERCPU task_t * tid_prev;
extern __PERCPU task_t * tid_next;
extern __PERCPU u32      no_preempt;

// the last bit in `no_preempt` means there's task waiting to preempt
static inline void preempt_lock  () { thiscpu32_add(&no_preempt, 2); }
static inline void preempt_unlock() { thiscpu32_sub(&no_preempt, 2); }

extern u32      sched_stop  (task_t * tid, u32 state);
extern u32      sched_cont  (task_t * tid, u32 state);
extern task_t * task_create (int priority, int cpu_idx, void * proc,
                             void * a1, void * a2, void * a3, void * a4);
extern void     task_exit   ();
extern void     task_yield  ();
extern void     task_suspend();
extern void     task_resume (task_t * tid);
extern void     task_delay  (int ticks);
extern void     task_wakeup (task_t * tid);

// requires: percpu-var
extern __INIT void task_lib_init();

#endif // CORE_TASK_H

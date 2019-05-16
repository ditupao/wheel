#ifndef CORE_TASK_H
#define CORE_TASK_H

#include <base.h>
#include <mem/page.h>
#include <mem/vmspace.h>
#include <core/process.h>
#include <libk/spin.h>
#include <libk/list.h>
#include <libk/rbtree.h>

// task control block
// - kstack: used to execute syscall handler and exception handler
// - ustack: used to execute application code under user mode
typedef struct task {
    // arch-specific status
    regs_t      regs;

    // task management
    spin_t      lock;
    dlnode_t    dl_task;
    char        name[64];

    // scheduler specific fields
    u32         state;
    int         priority;
    cpuset_t    affinity;
    int         last_cpu;
    int         timeslice;
    int         remaining;
    dlnode_t    dl_sched;

    // process control
    int         ret_val;        // return code from PEND state
    pfn_t       kstack;         // kernel stack, one block
    vmrange_t * ustack;         // user stack region
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

extern task_t * task_create (const char * name, int priority, void * proc,
                             void * a1, void * a2, void * a3, void * a4);
extern void     task_exit   ();
extern void     task_suspend();
extern void     task_resume (task_t * tid);
extern void     task_delay  (int ticks);
extern void     task_wakeup (task_t * tid);
extern void     task_dump   ();

// requires: pool
extern __INIT void task_lib_init();

#endif // CORE_TASK_H

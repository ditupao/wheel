#include <wheel.h>

static pool_t tcb_pool;

//------------------------------------------------------------------------------
// task operations

// create new task
task_t * task_create(int priority, void * proc,
                     void * a1, void * a2, void * a3, void * a4) {
    dbg_assert((0 <= priority) && (priority < PRIORITY_COUNT));

    // allocate tcb
    task_t * tid = pool_obj_alloc(&tcb_pool);

    // allocate space for kernel stack, must be a single block
    // TODO: put kernel stack size in config.h
    pfn_t kstk = page_block_alloc(ZONE_DMA|ZONE_NORMAL, 4);
    if (NO_PAGE == kstk) {
        // TODO: kernel panic, out-of-memory
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
    tid->lock      = SPIN_INIT;

    tid->state     = TS_SUSPEND;
    tid->priority  = priority;
    tid->affinity  = 0UL;
    tid->last_cpu  = -1;
    tid->timeslice = 200;
    tid->remaining = 200;
    tid->dl_sched  = DLNODE_INIT;

    tid->ret_val   = 0;
    tid->kstack    = kstk;
    tid->ustack    = NULL;
    tid->dl_proc   = DLNODE_INIT;
    tid->process   = NULL;

    return tid;
}

// work function to be called after task_exit
static void task_cleanup(task_t * tid) {
    dbg_assert(TS_ZOMBIE == tid->state);

    // unmap and remove vm region for user stack
    if ((NULL != tid->process) && (NULL != tid->ustack)) {
        vmspace_unmap(&tid->process->vm, tid->ustack);
        vmspace_free (&tid->process->vm, tid->ustack);
        tid->ustack = NULL;
    }

    // free all pages in kernel stack
    page_block_free(tid->kstack, 4);

    // remove this thread from the process
    if (NULL != tid->process) {
        dl_remove(&tid->process->tasks, &tid->dl_proc);
        if ((NULL == tid->process->tasks.head) &&
            (NULL == tid->process->tasks.tail)) {
            // if this is the last thread, also delete the process
            process_delete(tid->process);
        }
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
    u32 old = sched_cont(tid, TS_SUSPEND);
    int cpu = tid->last_cpu;
    irq_spin_give(&tid->lock, key);

    if (TS_READY == old) {
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
    u32 old = sched_cont(tid, TS_DELAY);
    int cpu = tid->last_cpu;
    irq_spin_give(&tid->lock, key);

    if (TS_READY == old) {
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
    pool_init(&tcb_pool, sizeof(task_t));
}

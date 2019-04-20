#include <wheel.h>

syscall_proc_t syscall_tbl[SYSCALL_NUM_COUNT];

// entry of the newly spawned thread, start in kernel mode
static void thread_entry(void * entry) {
    task_t    * self  = thiscpu_var(tid_prev);
    vmrange_t * range = vmspace_alloc(&self->process->vm, 16 * PAGE_SIZE);
    vmspace_map(&self->process->vm, range);

    dbg_assert(NULL == self->ustack);
    self->ustack = range;

    // jump into user mode, won't return
    enter_user((usize) entry, range->addr + 16 * PAGE_SIZE);
}

static int syscall_default(void * a1 __UNUSED, void * a2 __UNUSED,
                           void * a3 __UNUSED, void * a4 __UNUSED) {
    dbg_print("unsupported syscall.\r\n");
    return 0;
}

static int syscall_exit() {
    task_exit();
    return 0;
}

static int syscall_spawn(void * entry,       void * a2 __UNUSED,
                         void * a3 __UNUSED, void * a4 __UNUSED) {
    task_t * cur = thiscpu_var(tid_prev);
    task_t * tid = task_create(cur->process, cur->priority, 0, thread_entry, entry, 0,0,0);
    task_resume(tid);

    dbg_print("tracing inside sys_spawn:\r\n");
    dbg_trace();
    return 0;
}

static int syscall_write(const char * s,     void * a2 __UNUSED,
                         void * a3 __UNUSED, void * a4 __UNUSED) {
    dbg_print(s);
    return 0;
}

static int syscall_magic(void * a1 __UNUSED, void * a2 __UNUSED,
                         void * a3 __UNUSED, void * a4 __UNUSED) {
    dbg_print("magic system call");
    return 0xdeadbeef;
}

__INIT void syscall_lib_init() {
    for (int i = 0; i < SYSCALL_NUM_COUNT; ++i) {
        syscall_tbl[i] = (syscall_proc_t) syscall_default;
    }
    syscall_tbl[SYSCALL_EXIT ] = (syscall_proc_t) syscall_exit;
    syscall_tbl[SYSCALL_SPAWN] = (syscall_proc_t) syscall_spawn;
    syscall_tbl[SYSCALL_WRITE] = (syscall_proc_t) syscall_write;
    syscall_tbl[SYSCALL_MAGIC] = (syscall_proc_t) syscall_magic;
}

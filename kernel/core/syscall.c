#include <wheel.h>

// TODO: use a syscall_handler_tbl array, like isr_tbl
// TODO: must keep syscall function number standard
// TODO: maybe use linux syscall number assignment?

#define SYS_EXIT    0
#define SYS_SPAWN   1

#define SYS_OPEN    2
#define SYS_CLOSE   3

#define SYS_READ    4
#define SYS_WRITE   5

#define SYS_MAGIC   255

// // exit current process
// static void syscall_exit(int code) {
//     process_t * pid = thiscpu_var(tid_prev)->process;

//     // stop all child task (except this one)
// }

void sys_exit() {
    task_exit();
}

void sys_spawn(void * entry) {
    task_t * cur = thiscpu_var(tid_prev);
    task_t * tid = task_create(cur->process, PRIORITY_NONRT, 0, entry, 0,0,0,0);
    task_resume(tid);
}


usize syscall_dispatch(usize id, void * a1, void * a2, void * a3, void * a4) {
    // keep gcc happy
    a1 = a1;
    a2 = a2;
    a3 = a3;
    a4 = a4;

    switch (id) {
    case SYS_EXIT:
        task_exit();
        return 0;
    case SYS_SPAWN:
        sys_spawn(a1);
        return 0;
    case SYS_OPEN:
        return 0;
    case SYS_CLOSE:
        return 0;
    case SYS_READ:
        return 0;
    case SYS_WRITE:
        dbg_print((const char *) a1);
        return 0;
    case SYS_MAGIC:
        return 0xdeadbeef;
    default:
        return 0;
    }
}

void syscall_dispatch2() {
    dbg_print("doing syscall.\r\n");
}

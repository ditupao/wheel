#include <wheel.h>

// TODO: use a syscall_handler_tbl array, like isr_tbl
// TODO: must keep syscall function number standard
// TODO: maybe use linux syscall number assignment?

#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3

#define SYS_MAGIC   5

usize syscall_dispatch(usize id, void * a1, void * a2, void * a3, void * a4) {
    // keep gcc happy
    a1 = a1;
    a2 = a2;
    a3 = a3;
    a4 = a4;

    switch (id) {
    case SYS_WRITE:
        dbg_print((const char *) a1);
        return 0;
    case SYS_MAGIC:
        return 0xdeadbeef;
    default:
        return 0;
    }
}

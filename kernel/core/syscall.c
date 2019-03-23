#include <wheel.h>

// TODO: use a syscall_handler_tbl array, like isr_tbl
// TODO: must keep syscall function number standard
// TODO: maybe use linux syscall number assignment?
#define SYSCALL_ID_PRINT    1
#define SYSCALL_ID_MAGIC    2

usize syscall_dispatch(usize id, void * a1, void * a2, void * a3, void * a4) {
    switch (id) {
    case SYSCALL_ID_PRINT:
        dbg_print((const char *) a1);
        return 0;
    case SYSCALL_ID_MAGIC:
        return 0xdeadbeef;
    default:
        return 0;
    }
}

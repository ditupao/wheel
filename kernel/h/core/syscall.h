#ifndef CORE_SYSCALL_H
#define CORE_SYSCALL_H

#include <base.h>

typedef int (* syscall_proc_t) (void * a1, void * a2, void * a3, void * a4);

#define SYSCALL_NUM_COUNT   256

// typedef enum syscall_func {
// #define DEFINE_SYSCALL(name, func) name,
// #include <core/syscall_tbl.def>
// #undef DEFINE_SYSCALL
// } syscall_func_t;

extern syscall_proc_t syscall_tbl[];

// requires: nothing
extern __INIT void syscall_lib_init();

#endif // CORE_SYSCALL_H

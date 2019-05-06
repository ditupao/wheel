#ifndef CORE_SYSCALL_H
#define CORE_SYSCALL_H

#include <base.h>

typedef int (* syscall_proc_t) (void * a1, void * a2, void * a3, void * a4);

#define SYSCALL_NUM_COUNT 256

extern syscall_proc_t syscall_tbl[];

// syscall handler functions
#define DEFINE_SYSCALL(id, name, ...)   \
    extern int do_ ## name (__VA_ARGS__);
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

// requires: nothing
extern __INIT void syscall_lib_init();

#endif // CORE_SYSCALL_H

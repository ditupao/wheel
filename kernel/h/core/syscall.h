#ifndef CORE_SYSCALL_H
#define CORE_SYSCALL_H

#include <base.h>

#define SYSCALL_NUM_COUNT   256

#define SYSCALL_EXIT        0
#define SYSCALL_SPAWN       1
#define SYSCALL_OPEN        2
#define SYSCALL_CLOSE       3
#define SYSCALL_READ        4
#define SYSCALL_WRITE       5
#define SYSCALL_MAGIC       255

typedef int (* syscall_proc_t) (void * a1, void * a2, void * a3, void * a4);

extern syscall_proc_t syscall_tbl[];

// requires: nothing
extern __INIT void syscall_lib_init();

#endif // CORE_SYSCALL_H

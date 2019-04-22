#include <stddef.h>

// generate syscall id values
typedef enum syscall_id {
    #define DEFINE_SYSCALL(id, name, proc, ...) name = id,
    #include SYSCALL_DEF
    #undef DEFINE_SYSCALL
} syscall_id_t;

// generate syscall wrapper function prototypes
#define DEFINE_SYSCALL(id, name, proc, ...) extern int proc (__VA_ARGS__);
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

extern int syscall(int func, ...);

void another_thread_func() {
    syscall(SYSCALL_WRITE, 1, "printing within another thread!\r\n");
    syscall(SYSCALL_EXIT, 0);
}

void _entry() {
    unsigned int ret = syscall(SYSCALL_MAGIC, NULL);
    if (0xdeadbeef == ret) {
        syscall(SYSCALL_WRITE, 1, "we got dead beef!\r\n");
    } else {
        syscall(SYSCALL_WRITE, 1, "we got something else!\r\n");
    }

    syscall(SYSCALL_SPAWN_THREAD, another_thread_func);

    char * argv[] = { NULL };
    char * envp[] = { NULL };
    syscall(SYSCALL_SPAWN_PROCESS, "./setup.app", argv, envp);

    syscall(SYSCALL_EXIT, 0);
    syscall(SYSCALL_WRITE, 1, "already deleted!\r\n");

    while (1) {}
}

#include <stddef.h>
#include <stdint.h>

// generate syscall wrapper function prototypes
#define DEFINE_SYSCALL(id, name, ...) extern int name (__VA_ARGS__);
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

extern int syscall(int func, ...);

void another_thread_func() {
    write(1, "printing within another thread!\r\n", 0);
    exit(0);
}

void _entry() {
    if (0xdeadbeef == magic()) {
        write(1, "magic() got 0xdeadbeef!\r\n", 0);
    } else {
        write(1, "magic() got something else!\r\n", 0);
    }

    // syscall(SYSCALL_SPAWN_THREAD, another_thread_func);

    // const char * argv[] = { NULL };
    // const char * envp[] = { NULL };
    // spawn_process("./setup.app", argv, envp);

    exit(0);
}

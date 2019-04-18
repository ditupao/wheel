#include <stddef.h>

#define SYS_EXIT    0
#define SYS_SPAWN   1

#define SYS_OPEN    2
#define SYS_CLOSE   3

#define SYS_READ    4
#define SYS_WRITE   5

#define SYS_MAGIC   255

extern unsigned int syscall(int func, void * a1);
extern unsigned int syscall2();

void another_thread_func() {
    syscall(SYS_WRITE, "printing within another thread!\r\n");
    syscall(SYS_EXIT, 0);
}

void _entry() {
    // char * video = (char *) (0xb8000 + 0xffff800000000000UL);
    // char * msg   = "hello from user mode.";
    // for (int i = 0; msg[i]; ++i) {
    //     video[2 * i + 0] = msg[i];
    //     video[2 * i + 1] = 0x4e;
    // }

    unsigned int ret = syscall(SYS_MAGIC, NULL);
    if (0xdeadbeef == ret) {
        syscall(SYS_WRITE, "we got dead beef!\r\n");
    } else {
        syscall(SYS_WRITE, "we got something else!\r\n");
    }

    syscall2();

    syscall(SYS_SPAWN, another_thread_func);

    syscall(SYS_EXIT, 0);
    syscall(SYS_WRITE, "already deleted!\r\n");

    while (1) {}
}

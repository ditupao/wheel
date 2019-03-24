#include <stddef.h>

extern unsigned int syscall(int func, void * a1);

void _entry() {
    char * video = (char *) (0xb8000 + 0xffff800000000000UL);
    char * msg   = "hello from user mode.";
    for (int i = 0; msg[i]; ++i) {
        video[2 * i + 0] = msg[i];
        video[2 * i + 1] = 0x4e;
    }

    unsigned int ret = syscall(5, NULL);
    if (0xdeadbeef == ret) {
        syscall(1, "we got dead beef!\r\n");
    } else {
        syscall(1, "we got something else!\r\n");
    }

    while (1) {}
}

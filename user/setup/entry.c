#include <stddef.h>

void _entry() {
    char * video = (char *) (0xb8000 + 0xffff800000000000UL);
    char * msg   = "hello from user mode.";
    for (int i = 0; msg[i]; ++i) {
        video[2 * i + 0] = msg[i];
        video[2 * i + 1] = 0x4e;
    }
    __asm__ volatile("int $0x80");
    while (1) {}
}

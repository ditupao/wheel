#include <stddef.h>

void _entry() {
    __asm__ volatile("int $0x80");
    while (1) {}
}
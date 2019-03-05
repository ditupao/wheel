#include <wheel.h>

__INIT __NORETURN void sys_init_bsp(u32 ebx __UNUSED) {
    char * video = (char *) 0xb8000;
    video[0] = 'C';
    video[1] = 0x4e;

    serial_dev_init();
    dbg_print("hello, world!\r\n");

    while (1) {}
}

__INIT __NORETURN void sys_init_ap() {
    while (1) {}
}

__INIT __NORETURN void sys_init(u32 eax, u32 ebx) {
    switch (eax) {
    case 0x2badb002: sys_init_bsp(ebx); break;
    case 0xdeadbeef: sys_init_ap();     break;
    default:                            break;
    }
    while (1) {}
}

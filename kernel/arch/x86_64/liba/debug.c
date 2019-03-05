#include <wheel.h>

void dbg_print(const char * msg, ...) {
    va_list args;
    char buf[1024];

    va_start(args, msg);
    vsnprintf(buf, 1023, msg, args);
    va_end(args);

    serial_puts(buf);
}

void dbg_trace() {
    u64 * rbp;
    ASM("movq %%rbp, %0" : "=r"(rbp));

    while (rbp[0]) {
        dbg_print("function ret addr: %#llx.\r\n", rbp[1]);
        rbp = (u64 *) rbp[0];
    }
}

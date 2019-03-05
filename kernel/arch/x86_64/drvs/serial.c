#include <wheel.h>

#define COM1 0x3f8

static void _serial_putc(char c) {
    while ((in8(COM1 + 5) & 0x20) == 0) {}
    out8(COM1, c);
}

void serial_putc(char c) {
    _serial_putc(c);
}

void serial_puts(const char * s) {
    for (; *s; ++s) {
        _serial_putc(*s);
    }
}

__INIT void serial_dev_init() {
    out8(COM1 + 1, 0x00);   // disable all interrupts
    out8(COM1 + 3, 0x80);   // enable DLAB (set baud rate divisor)
    out8(COM1 + 0, 0x03);   // set divisor to 3 (lo byte) 38400 baud
    out8(COM1 + 1, 0x00);   //                  (hi byte)
    out8(COM1 + 3, 0x03);   // 8 bits, no parity, one stop bit
    out8(COM1 + 2, 0xc7);   // enable FIFO, clear them, with 14-byte threshold
    out8(COM1 + 4, 0x0b);   // IRQs enabled, RTS/DSR set
}

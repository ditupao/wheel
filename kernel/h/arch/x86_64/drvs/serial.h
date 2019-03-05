#ifndef ARCH_X86_64_DRVS_SERIAL_H
#define ARCH_X86_64_DRVS_SERIAL_H

#include <base.h>

extern void serial_putc(char c);
extern void serial_puts(const char * s);

extern __INIT void serial_dev_init();

#endif // ARCH_X86_64_DRVS_SERIAL_H

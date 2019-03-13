#ifndef ARCH_X86_64_DRVS_CONSOLE_H
#define ARCH_X86_64_DRVS_CONSOLE_H

#include <base.h>

extern void console_putc(const char c);
extern void console_puts(const char * s);

extern __INIT void console_dev_init();

#endif // ARCH_X86_64_DRVS_CONSOLE_H

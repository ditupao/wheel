#ifndef ARCH_X86_64_LIBA_DEBUG_H
#define ARCH_X86_64_LIBA_DEBUG_H

#include <base.h>

extern void dbg_print(const char * msg, ...);
extern void dbg_trace();

#define dbg_assert(x) ({ if (!(x)) {                            \
    dbg_print("assertion failed %s:%d.\n", __FILE__, __LINE__); \
    dbg_trace();                                                \
    while (1) {}                                                \
} })

#endif // ARCH_X86_64_LIBA_DEBUG_H

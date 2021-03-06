#ifndef ARCH_X86_64_LIBA_DEBUG_H
#define ARCH_X86_64_LIBA_DEBUG_H

#include <base.h>

extern void dbg_print (const char * msg, ...);

#define dbg_assert(x) ({ if (!(x)) {                            \
    dbg_print("assertion failed %s:%d.\n", __FILE__, __LINE__); \
    dbg_trace();                                                \
    while (1) {}                                                \
} })

extern void dbg_trace ();
extern void dbg_trace_from(u64 rip, u64 * rbp);

// requires: physical-page-alloc
extern __INIT void dbg_regist(u8 * sym_tbl, usize sym_len, u8 * str_tbl, usize str_len);

#endif // ARCH_X86_64_LIBA_DEBUG_H

#ifndef LIBC_SYSTEM_H
#define LIBC_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

// this is the top-level header of userland
// not be confused with kernel `wheel.h`

#include <stddef.h>
#include <stdint.h>

#define DEFINE_SYSCALL(i, type, name, ...) \
    extern type name (__VA_ARGS__);
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

#ifdef __cplusplus
}
#endif

#endif // LIBC_SYSTEM_H

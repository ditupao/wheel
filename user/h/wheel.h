#ifndef WHEEL_H
#define WHEEL_H

// this is the top-level header of userland
// not be confused with kernel `wheel.h`

#include <stddef.h>
#include <stdint.h>

#define DEFINE_SYSCALL(id, name, ...) extern int name (__VA_ARGS__);
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

#endif // WHEEL_H

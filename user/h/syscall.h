#ifndef WHEEL_SYSCALL_H
#define WHEEL_SYSCALL_H

#include <stddef.h>
#include <stdint.h>

#define DEFINE_SYSCALL(id, name, ...) extern int name (__VA_ARGS__);
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

#endif // WHEEL_SYSCALL_H

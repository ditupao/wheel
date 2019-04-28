// assembler wrapper of system call

.text
.code64

stub_syscall:
    pushq   %rbx
    pushq   %r10
    pushq   %r11
    movq    %rcx, %r10
    syscall
    popq    %r11
    popq    %r10
    popq    %rbx
    ret

#define DEFINE_SYSCALL(id, name, ...)   \
.global name;                           \
.type name,@function;                   \
name:;                                  \
    movl    $ id, %eax;                 \
    jmp     stub_syscall
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

// TODO: move this into crt
// create a static library for linking with user programs

.global _start
.extern _entry

.global syscall

.text
.code64

syscall:
    pushq   %rbx
    pushq   %r10
    pushq   %r11

    movl    %edi, %eax          // eax = syscall number
    movq    %rsi, %rdi
    movq    %rdx, %rsi
    movq    %rcx, %rdx

    syscall

    popq    %r11
    popq    %r10
    popq    %rbx

    ret

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
name:;                                  \
    movl    $ id, %eax;                 \
    jmp     stub_syscall
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

_start:
    // sections are loaded at correct location
    // stack space are allocated by the kernel
    call    _entry
    call    exit
    ret

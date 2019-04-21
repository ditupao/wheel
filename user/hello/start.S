// TODO: move this into crt
// create a static library for linking with user programs

.global _start
.extern _entry

.global syscall

.text
.code64

_start:
    // sections are loaded at correct location
    // stack space are allocated by the kernel
    call    _entry
    ret

syscall:
    pushq   %rbx
    pushq   %r10
    pushq   %r11

    movq    %rdi, %rax          // rax = syscall number
    movq    %rsi, %rdi
    movq    %rdx, %rsi
    movq    %rcx, %rdx

    syscall

    popq    %r11
    popq    %r10
    popq    %rbx

    ret
// this file serves as the standard library

.global syscall
.global _start

.extern main

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
    movq    %r8,  %r10
    movq    %r9,  %r8

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

// this is the real entry of user program
_start:
    pushq   $0                  // dummy return code
    pushq   $0                  // dummy rbp value
    movq    %rsp, %rbp

    xorq    %rdi, %rdi          // argc
    xorq    %rsi, %rsi          // argv
    xorq    %rdx, %rdx          // env
    call    main

    movl    %eax, %edi          // return value of main()
    call    exit                // should never return
1:
    jmp     1b

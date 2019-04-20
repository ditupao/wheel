#include <asm.h>

GLOBAL_FUNC(memcpy)
GLOBAL_FUNC(memmove)
GLOBAL_FUNC(memset)

.section .text
.code64

// ERMS (enhanced rep movsb/stosb)
// introduced in IvyBridge (2012)
// makes `rep+movsb` much faster
#define ERMS 0

//------------------------------------------------------------------------------
// void * memcpy(void * dst, const void * src, usize n);

memcpy:
#ifdef ERMS
    movq    %rdi, %rax
    movq    %rdx, %rcx
    rep movsb
    ret
#else // ERMS
    movq    %rdi, %rax
    movq    %rdx, %rcx
    shrq    $3, %rcx
    andl    $7, %edx
    rep movsq
    movl    %edx, %ecx
    rep movsb
    ret
#endif // ERMS

//------------------------------------------------------------------------------
// void * memmove(void * dst, const void * src, usize n);

memmove:
#ifdef ERMS
    movq    %rdi, %rax
    movq    %rdx, %rcx
    cmpq    %rdi, %rsi
    jae     1f

    // copy backward
    addq    %rcx, %rdi
    addq    %rcx, %rsi
    std
1:
    // copy forward
    rep movsb
    cld
    ret
#else // ERMS
    movq    %rdi, %rax
    movq    %rdx, %rbx
    movq    %rdx, %rcx
    shrq    $3, %rcx
    andl    $7, %edx
    cmpq    %rdi, %rsi
    jae     1f

    // copy backward
    addq    %rbx, %rdi
    addq    %rbx, %rsi
    std
1:
    // copy forward
    rep movsq
    movl    %edx, %ecx
    rep movsb
    ret
#endif // ERMS

//------------------------------------------------------------------------------
// void * memset(void * buf, u8 x, usize n);

memset:
#ifdef ERMS
    movl    %esi, %eax
    movq    %rdi, %rsi  // backup return value
    movq    %rdx, %rcx
    rep stosb
    movq    %rsi, %rax
    ret
#else // ERMS
    movq    %rdi, %r9
    movq    %rdx, %rcx
    andl    $7, %edx
    shrq    $3, %rcx
    movzbl  %sil, %esi
    movabs  $0x0101010101010101, %rax
    imulq   %rsi, %rax
    rep stosq
    movl    %edx, %ecx
    rep stosb
    movq    %r9, %rax
    ret
#endif // ERMS

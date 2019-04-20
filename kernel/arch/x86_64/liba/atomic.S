#include <asm.h>

GLOBAL_FUNC(atomic32_set)
GLOBAL_FUNC(atomic32_inc)
GLOBAL_FUNC(atomic32_dec)
GLOBAL_FUNC(atomic32_add)
GLOBAL_FUNC(atomic32_sub)
GLOBAL_FUNC(atomic32_and)
GLOBAL_FUNC(atomic32_or )
GLOBAL_FUNC(atomic32_xor)
GLOBAL_FUNC(atomic32_cas)

GLOBAL_FUNC(atomic64_set)
GLOBAL_FUNC(atomic64_inc)
GLOBAL_FUNC(atomic64_dec)
GLOBAL_FUNC(atomic64_add)
GLOBAL_FUNC(atomic64_sub)
GLOBAL_FUNC(atomic64_and)
GLOBAL_FUNC(atomic64_or )
GLOBAL_FUNC(atomic64_xor)
GLOBAL_FUNC(atomic64_cas)

GLOBAL_FUNC(thiscpu32_set)
GLOBAL_FUNC(thiscpu32_inc)
GLOBAL_FUNC(thiscpu32_dec)
GLOBAL_FUNC(thiscpu32_add)
GLOBAL_FUNC(thiscpu32_sub)
GLOBAL_FUNC(thiscpu32_and)
GLOBAL_FUNC(thiscpu32_or )
GLOBAL_FUNC(thiscpu32_xor)
GLOBAL_FUNC(thiscpu32_cas)

GLOBAL_FUNC(thiscpu64_set)
GLOBAL_FUNC(thiscpu64_inc)
GLOBAL_FUNC(thiscpu64_dec)
GLOBAL_FUNC(thiscpu64_add)
GLOBAL_FUNC(thiscpu64_sub)
GLOBAL_FUNC(thiscpu64_and)
GLOBAL_FUNC(thiscpu64_or )
GLOBAL_FUNC(thiscpu64_xor)
GLOBAL_FUNC(thiscpu64_cas)

.section .text
.code64

atomic32_set:
    xchgl       %esi, (%rdi)
    movl        %esi, %eax
    ret

atomic32_inc:
    movl        $1, %eax
    lock xaddl  %eax, (%rdi)
    ret

atomic32_dec:
    movl        $-1, %eax
    lock xaddl  %eax, (%rdi)
    ret

atomic32_add:
    lock xaddl  %esi, (%rdi)
    movl        %esi, %eax
    ret

atomic32_sub:
    negl        %esi
    lock xaddl  %esi, (%rdi)
    movl        %esi, %eax
    ret

atomic32_and:
    movl        (%rdi), %eax
_retry32_and:
    movl        %esi, %edx
    andl        %eax, %edx
    lock
    cmpxchgl    %edx, (%rdi)
    jnz         _retry32_and
    ret

atomic32_or:
    movl        (%rdi), %eax
_retry32_or:
    movl        %esi, %edx
    orl         %eax, %edx
    lock
    cmpxchgl    %edx, (%rdi)
    jnz         _retry32_or
    ret

atomic32_xor:
    movl        (%rdi), %eax
_retry32_xor:
    movl        %esi, %edx
    xorl        %eax, %edx
    lock
    cmpxchgl    %edx, (%rdi)
    jnz         _retry32_xor
    ret

atomic32_cas:
    movl        %esi, %eax
    lock
    cmpxchgl    %edx, (%rdi)
    ret

atomic64_set:
    xchgq       %rsi, (%rdi)
    movq        %rsi, %rax
    ret

atomic64_inc:
    movq        $1, %rax
    lock xaddq  %rax, (%rdi)
    ret

atomic64_dec:
    movq        $-1, %rax
    lock xaddq  %rax, (%rdi)
    ret

atomic64_add:
    lock xaddq  %rsi, (%rdi)
    movq        %rsi, %rax
    ret

atomic64_sub:
    negq        %rsi
    lock xaddq  %rsi, (%rdi)
    movq        %rsi, %rax
    ret

atomic64_and:
    movq        (%rdi), %rax
_retry64_and:
    movq        %rsi, %rdx
    andq        %rax, %rdx
    lock
    cmpxchgq    %rdx, (%rdi)
    jnz         _retry64_and
    ret

atomic64_or:
    movq        (%rdi), %rax
_retry64_or:
    movq        %rsi, %rdx
    orq         %rax, %rdx
    lock
    cmpxchgq    %rdx, (%rdi)
    jnz         _retry64_or
    ret

atomic64_xor:
    movq        (%rdi), %rax
_retry64_xor:
    movq        %rsi, %rdx
    xorq        %rax, %rdx
    lock
    cmpxchgq    %rdx, (%rdi)
    jnz         _retry64_xor
    ret

atomic64_cas:
    movq        %rsi, %rax
    lock
    cmpxchgq    %rdx, (%rdi)
    ret

thiscpu32_set:
    xchgl       %esi, %gs:(%rdi)
    movl        %esi, %eax
    ret

thiscpu32_inc:
    movl        $1, %eax
    lock xaddl  %eax, %gs:(%rdi)
    ret

thiscpu32_dec:
    movl        $-1, %eax
    lock xaddl  %eax, %gs:(%rdi)
    ret

thiscpu32_add:
    lock xaddl  %esi, %gs:(%rdi)
    movl        %esi, %eax
    ret

thiscpu32_sub:
    negl        %esi
    lock xaddl  %esi, %gs:(%rdi)
    movl        %esi, %eax
    ret

thiscpu32_and:
    movl        %gs:(%rdi), %eax
_retry32_thiscpu_and:
    movl        %esi, %edx
    andl        %eax, %edx
    lock
    cmpxchgl    %edx, %gs:(%rdi)
    jnz         _retry32_thiscpu_and
    ret

thiscpu32_or:
    movl        %gs:(%rdi), %eax
_retry32_thiscpu_or:
    movl        %esi, %edx
    orl         %eax, %edx
    lock
    cmpxchgl    %edx, %gs:(%rdi)
    jnz         _retry32_thiscpu_or
    ret

thiscpu32_xor:
    movl        %gs:(%rdi), %eax
_retry32_thiscpu_xor:
    movl        %esi, %edx
    xorl        %eax, %edx
    lock
    cmpxchgl    %edx, %gs:(%rdi)
    jnz         _retry32_thiscpu_xor
    ret

thiscpu32_cas:
    movl        %esi, %eax
    lock
    cmpxchgl    %edx, %gs:(%rdi)
    ret

thiscpu64_set:
    xchgq       %rsi, %gs:(%rdi)
    movq        %rsi, %rax
    ret

thiscpu64_inc:
    movq        $1, %rax
    lock xaddq  %rax, %gs:(%rdi)
    ret

thiscpu64_dec:
    movq        $-1, %rax
    lock xaddq  %rax, %gs:(%rdi)
    ret

thiscpu64_add:
    lock xaddq  %rsi, %gs:(%rdi)
    movq        %rsi, %rax
    ret

thiscpu64_sub:
    negq        %rsi
    lock xaddq  %rsi, %gs:(%rdi)
    movq        %rsi, %rax
    ret

thiscpu64_and:
    movq        %gs:(%rdi), %rax
_retry64_thiscpu_and:
    movq        %rsi, %rdx
    andq        %rax, %rdx
    lock
    cmpxchgq    %rdx, %gs:(%rdi)
    jnz         _retry64_thiscpu_and
    ret

thiscpu64_or:
    movq        %gs:(%rdi), %rax
_retry64_thiscpu_or:
    movq        %rsi, %rdx
    orq         %rax, %rdx
    lock
    cmpxchgq    %rdx, %gs:(%rdi)
    jnz         _retry64_thiscpu_or
    ret

thiscpu64_xor:
    movq        %gs:(%rdi), %rax
_retry64_thiscpu_xor:
    movq        %rsi, %rdx
    xorq        %rax, %rdx
    lock
    cmpxchgq    %rdx, %gs:(%rdi)
    jnz         _retry64_thiscpu_xor
    ret

thiscpu64_cas:
    movq        %rsi, %rax
    lock
    cmpxchgq    %rdx, %gs:(%rdi)
    ret

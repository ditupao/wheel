.global atomic32_set
.global atomic32_inc
.global atomic32_dec
.global atomic32_add
.global atomic32_sub
.global atomic32_and
.global atomic32_or
.global atomic32_xor
.global atomic32_cas

.global atomic64_set
.global atomic64_inc
.global atomic64_dec
.global atomic64_add
.global atomic64_sub
.global atomic64_and
.global atomic64_or
.global atomic64_xor
.global atomic64_cas

.global thiscpu32_set
.global thiscpu32_inc
.global thiscpu32_dec
.global thiscpu32_add
.global thiscpu32_sub
.global thiscpu32_and
.global thiscpu32_or
.global thiscpu32_xor
.global thiscpu32_cas

.global thiscpu64_set
.global thiscpu64_inc
.global thiscpu64_dec
.global thiscpu64_add
.global thiscpu64_sub
.global thiscpu64_and
.global thiscpu64_or
.global thiscpu64_xor
.global thiscpu64_cas

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

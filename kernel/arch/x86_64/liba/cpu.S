.section .text
.code64

.global entry_0
.global entry_1
.global entry_sc
.global load_gdtr
.global load_idtr
.global load_tr

.extern int_depth
.extern int_stack_ptr
.extern tid_prev
.extern tid_next

.extern exp_dispatch
.extern int_dispatch
.extern syscall_dispatch

//------------------------------------------------------------------------------
// exception and interrupt entry points

// save all registers to stack (except %rax)
.macro save_regs
    pushq   %rbx
    pushq   %rcx
    pushq   %rdx
    pushq   %rdi
    pushq   %rsi
    pushq   %rbp
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
.endm

// restore registers from stack
.macro restore_regs
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rbp
    popq    %rsi
    popq    %rdi
    popq    %rdx
    popq    %rcx
    popq    %rbx
    popq    %rax
.endm

.balign 16
entry_0:
    pushq   $-1
    pushq   %rax
    movl    $0, %eax
    jmp     exception_stub

.balign 16
entry_1:
    pushq   $-1
    pushq   %rax
    movl    $1, %eax
    jmp     exception_stub

vec = 2
.rept 256-2
.balign 16

.if ((10 <= vec) && (vec <= 14)) || (vec == 17)
    // exception with error code
    pushq   %rax
    movl    $ vec, %eax
    jmp     exception_stub
    pushq   $-1             // keep stub length same with no-err code ones
.elseif (vec < 32)
    // exception without error code
    pushq   $-1
    pushq   %rax
    movl    $ vec, %eax
    jmp     exception_stub
.else
    // interrupt (no error code)
    // vec 0x80 is not used
    pushq   $-1
    pushq   %rax
    movl    $ vec, %eax
    jmp     interrupt_stub
.endif

vec = vec + 1
.endr

// the entry for system call (int 0x80) is different
entry_sc:
    // system call always come from user mode
    // so we don't have to check CS
    swapgs
    sti
    call    syscall_dispatch
    cli
    swapgs
    iretq

// common code for exception handling
exception_stub:
    save_regs                           // save all registers
    movl    %eax, %edi                  // rdi = vector number
    movq    %rsp, %rsi                  // rsi = stack frame

    call    exp_dispatch                // execute handler function

    restore_regs                        // restore all registers
    addq    $8, %rsp                    // skip error code
    iretq                               // return from exception

// common code for interrupt handling
interrupt_stub:
    save_regs                           // save registers to current stack
    movl    %eax, %edi                  // rdi = vector number
    movq    %rsp, %rsi                  // rsi = stack frame

    incl    %gs:(int_depth)
    movl    %gs:(int_depth), %eax
    cmpl    $1, %eax                    // check interrupt re-entry
    jne     _no_stack_switch

    movq    %gs:(tid_prev), %rax
    movq    %rsp, (%rax)                // save to `tid_prev->regs->rsp`
    movq    %gs:(int_stack_ptr), %rsp   // switch to interruipt stack

_no_stack_switch:
    sti
    call    int_dispatch                // execute handler function
    cli

    decl    %gs:(int_depth)
    movl    %gs:(int_depth), %eax
    cmpl    $0, %eax
    jne     _no_stack_restore

    movq    %gs:(tid_next), %rax
    movq    %rax, %gs:(tid_prev)        // update `tid_prev`
    movq    (%rax), %rsp                // switch to `tid_next->regs->rsp`

_no_stack_restore:
    restore_regs                        // restore all registers
    addq    $8, %rsp                    // skip error code
    iretq                               // return from exception

//------------------------------------------------------------------------------
// helper functions, used by `cpu.c`

load_gdtr:
    lgdt    (%rdi)
    pushq   $8
    pushq   $_refresh
    lretq
_refresh:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss
    ret

load_idtr:
    lidt    (%rdi)
    ret

load_tr:
    ltr     %di
    ret

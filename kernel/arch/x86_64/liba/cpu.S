#include <asm.h>

.section .text
.code64

GLOBAL_FUNC(int0_entry)
GLOBAL_FUNC(int1_entry)

GLOBAL_FUNC(syscall_entry)
GLOBAL_FUNC(task_entry)
GLOBAL_FUNC(task_switch)

GLOBAL_FUNC(enter_user)
GLOBAL_FUNC(load_gdtr)
GLOBAL_FUNC(load_idtr)
GLOBAL_FUNC(load_tr)

EXTERN_DATA(int_depth)
EXTERN_DATA(int_stack_ptr)
EXTERN_DATA(tid_prev)
EXTERN_DATA(tid_next)

EXTERN_FUNC(exp_dispatch)           // in `liba/cpu.c`
EXTERN_FUNC(int_dispatch)           // in `liba/cpu.c`
EXTERN_FUNC(syscall_dispatch)       // in `core/syscall.c`
EXTERN_FUNC(syscall_dispatch2)      // in `core/syscall.c`

EXTERN_FUNC(work_dequeue)
EXTERN_FUNC(task_exit)

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
int0_entry:
    pushq   $-1
    pushq   %rax
    movl    $0, %eax
    jmp     exception_stub

.balign 16
int1_entry:
    pushq   $-1
    pushq   %rax
    movl    $1, %eax
    jmp     exception_stub

vec = 2
.rept 256-2
.balign 16

// generate ISR entry for every interrupt/exception
// but not for system call
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

// common code for exception handling
exception_stub:
    save_regs                           // save all registers
    movl    %eax, %edi                  // rdi = vector number
    movq    %rsp, %rsi                  // rsi = stack frame

    testl   $3, 136(%rsp)               // if come from user mode
    je      1f
    swapgs
1:
    call    exp_dispatch                // execute handler function

    testl   $3, 136(%rsp)               // if going to user mode
    je      2f
    swapgs
2:
    restore_regs                        // restore all registers
    addq    $8, %rsp                    // skip error code
    iretq                               // return from exception

// common code for interrupt handling
interrupt_stub:
    save_regs                           // save registers to current stack
    movl    %eax, %edi                  // rdi = vector number
    movq    %rsp, %rsi                  // rsi = stack frame

    testl   $3, 136(%rsp)               // if come from user mode
    je      3f
    swapgs                              // swap gs.base with kernel-gs.base
3:
    incl    %gs:(int_depth)
    movl    %gs:(int_depth), %eax
    cmpl    $1, %eax                    // check interrupt re-entry
    jne     _no_stack_switch

    movq    %gs:(tid_prev), %rax
    movq    %rsp, (%rax)                // save to tid_prev->regs->rsp
    movq    %gs:(int_stack_ptr), %rsp   // switch to interrupt stack

_no_stack_switch:
    sti
    call    int_dispatch                // execute handler function
    cli

    decl    %gs:(int_depth)
    movl    %gs:(int_depth), %eax
    cmpl    $0, %eax
    jne     _no_stack_restore           // if we're still inside ISR

    movq    %gs:(tid_next), %rax
    movq    0x00(%rax), %rsp            // get tid_next->regs->rsp
    movq    0x08(%rax), %rbx            // get tid_next->regs->rsp0
    movq    0x10(%rax), %rcx            // get tid_next->regs->cr3
    movq    %rax, %gs:(tid_prev)        // update `tid_prev`
    movq    $tss, %rdx
    movq    %rbx, %gs:4(%rdx)           // store rsp0 in tss (maybe unaligned?)
    movq    %cr3, %rdx
    cmpq    %rcx, %rdx                  // compare target cr3 and current cr3
    je      4f                          // still in the same address space
    movq    %rcx, %cr3                  // load new page table into cr3
4:
    call    work_dequeue                // flush work queue

_no_stack_restore:
    testl   $3, 136(%rsp)               // if going to user mode
    je      5f
    swapgs
5:
    restore_regs                        // restore all registers
    addq    $8, %rsp                    // skip error code
    iretq                               // return from exception

//------------------------------------------------------------------------------
// system call entry

// entry point for syscall/sysret version
// rax - syscall number
// rdi - argument 1
// rsi - argument 2
// rdx - argument 3
// r10 - argument 4 (replace rcx)
// r8  - argument 5
// r9  - argument 6
// caller of syscall must also save: rbx, rcx, r11
// the content of register r12-r15 are reserved
// after `syscall`, old rip was saved in rcx, rflags saved in r11
// stack (rsp) remains unchanged (still user stack)
syscall_entry:
    swapgs
    movq    %gs:(tid_prev), %rbx        // current task
    movq    0x08(%rbx), %rbx            // get tid->regs->rsp0
    xchgq   %rbx, %rsp                  // rsp = new stack, rbx = old stack

    pushq   $0                          // dummy return address
    pushq   %rbp                        // save old rbp (from user mode)
    movq    %rsp, %rbp                  // point to current stack frame

    pushq   %rcx                        // -0x08(rbp) save old rip (from user mode)
    pushq   %rbx                        // -0x10(rbp) save old rsp (from user mode)
    pushq   %r11                        // -0x18(rbp) save old rflags (from user mode)

    // TODO: use rax as index to syscall_tbl
    movq    %r10, %rcx                  // sys V abi
    call    syscall_dispatch

    movq    -0x18(%rbp), %r11           // restore user rflags
    movq    -0x10(%rbp), %rsp           // restore user rsp
    movq    -0x08(%rbp), %rcx           // restore user rip
    movq         (%rbp), %rbp           // restore user rbp

#if 0
    // TODO: rbx, r11 cannot be changed by `syscall_dispatch`
    movq    %gs:(tid_prev), %rbx        // current task
    movq    0x18(%rbx), %rcx            // get tid->regs->rip3
    movq    0x20(%rbx), %rsp            // get tid->regs->rsp3
    movq    0x28(%rbx), %r11            // get tid->regs->rflags3
#endif

    swapgs
    sysretq

//------------------------------------------------------------------------------
// task support

// the starting point of a new task
// rax - function pointer
// rdi - argument 1
// rsi - argument 2
// rdx - argument 3
// rcx - argument 4
task_entry:
    pushq   $0                          // dummy return code
    pushq   $0                          // old rbp value
    movq    %rsp, %rbp                  // point to current stack

    call    * %rax                      // start running task code
    call    task_exit                   // remove this task
1:
    hlt
    jmp     1b

// called outside int context, switch to tid_next unconditionally
// we have to make sure `tid_next` doesn't change during the switch
// ABI scratch registers: rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11
task_switch:
    pushfq
    cli
    popq    %rax                        // rax = rflags (the state before cli)
    cmpq    $0, %gs:(int_depth)
    jne     _no_task_switch             // we're inside isr, no need to switch
    cmpq    $0, %gs:(no_preempt)
    jne     _no_task_switch             // if preemption is locked, return

    movq    %gs:(tid_prev), %rsi        // load `tid_prev` to rsi
    movq    %gs:(tid_next), %rdi        // load `tid_next` to rdi
    cmpq    %rdi, %rsi
    je      _no_task_switch             // same task, no need to switch

    movq    %cs, %r8                    // r8  = cs
    popq    %r9                         // r9  = rip, restart from caller directly
    movq    %ss,  %r10                  // r10 = ss
    movq    %rsp, %r11                  // r11 = rsp
    pushq   %r10                        // ss
    pushq   %r11                        // rsp
    pushq   %rax                        // rflags
    pushq   %r8                         // cs
    pushq   %r9                         // rip, return address
    pushq   $0                          // error code
    pushq   $0                          // rax
    save_regs                           // save rest of the registers on stack
    movq    %rsp, (%rsi)                // store the stack top into TCB

    movq    0x00(%rdi), %rsp            // get tid_next->regs->rsp
    movq    0x08(%rdi), %rbx            // get tid_next->regs->rsp0
    movq    0x10(%rdi), %rcx            // get tid_next->regs->cr3
    movq    %rdi, %gs:(tid_prev)        // update `tid_prev`, context already saved
    movq    $tss, %rax
    movq    %rbx, %gs:4(%rax)           // store rsp0 in tss
    movq    %cr3, %rax
    cmpq    %rcx, %rax                  // compare target cr3 and current cr3
    je      6f                          // still in the same address space
    movq    %rcx, %cr3                  // load new page table into cr3
6:
    call    work_dequeue                // flush work queue

    restore_regs                        // restore all registers
    addq    $8, %rsp                    // skip error code
    iretq

_no_task_switch:
    pushq   %rax
    popfq
    ret

enter_user:
    swapgs
    movq    %rsi, %rsp  // switch to user stack
    pushfq
    cli
    popq    %rax
    subq    $8, %rsi
    pushq   $0          // no return address
    pushq   $0x23       // ss
    pushq   %rsi        // rsp
    pushq   %rax        // rflags
    pushq   $0x2b       // cs
    pushq   %rdi        // rip
    iretq

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

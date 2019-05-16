#include <linkage.h>

.section .text
.code64

GLOBAL_FUNC(int0_entry)
GLOBAL_FUNC(int1_entry)
GLOBAL_FUNC(exception_stub)
GLOBAL_FUNC(interrupt_stub)

GLOBAL_FUNC(syscall_entry)
GLOBAL_FUNC(task_entry)
GLOBAL_FUNC(task_switch)

GLOBAL_FUNC(return_to_user)
GLOBAL_FUNC(load_gdtr)
GLOBAL_FUNC(load_idtr)
GLOBAL_FUNC(load_tr)

EXTERN_DATA(int_depth)
EXTERN_DATA(int_rsp)
EXTERN_DATA(tid_prev)
EXTERN_DATA(tid_next)

EXTERN_DATA(isr_tbl)
EXTERN_DATA(syscall_tbl)

EXTERN_FUNC(work_dequeue)       // in `core/work.c`
EXTERN_FUNC(task_exit)          // in `core/task.c`

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
    movl    $0, %eax            // clear upper 32 bits
    jmp     exception_stub

.balign 16
int1_entry:
    pushq   $-1
    pushq   %rax
    movl    $1, %eax            // clear upper 32 bits
    jmp     exception_stub

vec = 2
.rept 256-2
.balign 16

// generate ISR entry for every interrupt/exception
// but not for system call
.if ((10 <= vec) && (vec <= 14)) || (vec == 17)
    // exception with error code
    pushq   %rax
    movl    $ vec, %eax         // clear upper 32 bits
    jmp     exception_stub
    pushq   $-1                 // keep stub length same with no-err code ones
.elseif (vec < 32)
    // exception without error code
    pushq   $-1
    pushq   %rax
    movl    $ vec, %eax         // clear upper 32 bits
    jmp     exception_stub
.else
    // interrupt (no error code)
    pushq   $-1
    pushq   %rax
    movl    $ vec, %eax
    jmp     interrupt_stub
.endif

vec = vec + 1
.endr

// common code for exception handling
// exceptions cannot re-enter, so `sti/cli` are not used
exception_stub:
    save_regs                       // save all registers
    movl    %eax, %edi              // rdi = vector number, clear upper 32 bits
    movq    %rsp, %rsi              // rsi = stack frame

    testl   $3, 0x88(%rsp)          // if come from user mode
    je      1f
    swapgs
1:
    pushq   $0                      // dummy return addr
    pushq   $0                      // dummy rbp value
    movq    %rsp, %rbp

    movq    $isr_tbl, %rax
    andl    $0xff, %edi             // rdi < 256 (clear upper 32 bits)
    movq    (%rax, %rdi, 8), %rax   // rax = isr_tbl[rdi]
    call    * %rax

    addq    $16, %rsp               // pop dummy return addr and rbp
    testl   $3, 0x88(%rsp)          // if going to user mode
    je      2f
    swapgs
2:
    restore_regs                    // restore all registers
    addq    $8, %rsp                // skip error code
    iretq                           // return from exception

// common code for interrupt handling
// interrupt re-enter is allowed, wrap isr handler with `sti/cli`
// also use int_depth to guide stack switching
interrupt_stub:
    save_regs                       // save registers to current stack
    movq    %rax, %rdi              // rdi = vector number
    movq    %rsp, %rsi              // rsi = stack frame
    movq    %rsp, %rbp              // save old stack pointer

    testl   $3, 0x88(%rsp)          // if come from user mode
    je      1f
    swapgs                          // swap gs.base with kernel-gs.base
1:
    incl    %gs:(int_depth)
    movl    %gs:(int_depth), %eax
    cmpl    $1, %eax                // check interrupt re-enter
    jne     2f

    movq    %gs:(tid_prev), %rax
    movq    %rsp, (%rax)            // save to tid_prev->regs->rsp
    movq    %gs:(int_rsp), %rsp     // switch to interrupt stack

2:
    pushq   $0                      // dummy return addr
    pushq   $0                      // dummy rbp value

    movq    $isr_tbl, %rax
    andl    $0xff, %edi             // rdi < 256 (clear upper 32 bits)
    movq    (%rax, %rdi, 8), %rax   // rax = isr_tbl[rdi]
    sti
    call    * %rax
    cli
    movq    %rbp, %rsp              // switch back to old stack

    decl    %gs:(int_depth)
    cmpl    $0, %gs:(int_depth)     // if we're still inside ISR
    jne     4f                      // no need to switch stack and swapgs
    cmpl    $0, %gs:(no_preempt)    // if we've disabled preemption
    jne     3f                      // no task switch is performed

return_to_task:
    movq    %gs:(tid_next), %rdi
    movq    0x00(%rdi), %rsp        // get tid_next->regs->rsp
    movq    0x08(%rdi), %rbx        // get tid_next->regs->rsp0
    movq    0x10(%rdi), %rcx        // get tid_next->regs->cr3

    movq    %rdi, %gs:(tid_prev)    // update `tid_prev`
    movq    $tss, %rdx
    movq    %rbx, %gs:4(%rdx)       // store rsp0 in tss (maybe unaligned?)
    testq   %rcx, %rcx
    jz      3f                      // regs->cr3 == 0 means kernel task
    movq    %cr3, %rdx
    cmpq    %rcx, %rdx
    je      3f                      // still in the same address space
    movq    %rcx, %cr3              // load new page table into cr3
3:
    call    work_dequeue            // flush work queue
    testl   $3, 0x88(%rsp)          // whether going to user mode
    je      4f
    swapgs
4:
    restore_regs                    // restore all registers
    addq    $8, %rsp                // skip error code
    iretq                           // return from exception

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
    movq    %gs:(tid_prev), %rbx    // current task
    movq    0x08(%rbx), %rbx        // get tid->regs->rsp0
    xchgq   %rbx, %rsp              // rsp = new stack, rbx = old stack

    pushq   $0                      // dummy return address
    pushq   %rbp                    // save old rbp (from user mode)
    movq    %rsp, %rbp

    pushq   %rcx                    // -0x08(rbp) save old rip (from user mode)
    pushq   %rbx                    // -0x10(rbp) save old rsp (from user mode)
    pushq   %r11                    // -0x18(rbp) save old rflags (from user mode)

    movq    $syscall_tbl, %rbx
    andl    $0xff, %eax             // rax < 256 (clear upper 32 bits)
    movq    (%rbx, %rax, 8), %rax   // rax = syscall_tbl[rax]
    movq    %r10, %rcx              // conform to sys V abi
    call    * %rax

    movq    -0x18(%rbp), %r11       // restore user rflags
    movq    -0x10(%rbp), %rsp       // restore user rsp
    movq    -0x08(%rbp), %rcx       // restore user rip
    movq         (%rbp), %rbp       // restore user rbp

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
    pushq   $0                      // dummy return addr
    pushq   $0                      // dummy rbp value
    movq    %rsp, %rbp

    call    * %rax                  // start running task code
    call    task_exit               // remove this task
1:
    hlt
    jmp     1b

// called outside int context, switch to tid_next unconditionally
// we have to make sure `tid_next` doesn't change during the switch
// ABI scratch registers: rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11
task_switch:
    pushfq
    cli
    popq    %rax                    // rax = rflags (the state before cli)
    cmpq    $0, %gs:(int_depth)
    jne     _no_task_switch         // we're inside isr, no need to switch
    cmpq    $0, %gs:(no_preempt)
    jne     _no_task_switch         // if preemption is locked, return

    movq    %gs:(tid_prev), %rsi    // load `tid_prev` to rsi
    movq    %gs:(tid_next), %rdi    // load `tid_next` to rdi
    cmpq    %rdi, %rsi
    je      _no_task_switch         // same task, no need to switch

    movq    %cs, %r8                // r8  = cs
    popq    %r9                     // r9  = rip, restart from caller directly
    movq    %ss,  %r10              // r10 = ss
    movq    %rsp, %r11              // r11 = rsp
    pushq   %r10                    // ss
    pushq   %r11                    // rsp
    pushq   %rax                    // rflags
    pushq   %r8                     // cs
    pushq   %r9                     // rip, return address
    pushq   $0                      // error code
    pushq   $0                      // rax
    save_regs                       // save rest of the registers on stack
    movq    %rsp, (%rsi)            // store the stack top into TCB

    jmp     return_to_task

_no_task_switch:
    pushq   %rax
    popfq
    ret

// build temporary SS/RSP/RFLAGS/CS/RIP structure on kernel stack
// rdi - rip value when switch to user mode
// rsi - rsp value when switch to user mode
return_to_user:
    pushfq
    cli
    popq    %rax        // rflags before cli
    pushq   $0x23       // ss
    pushq   %rsi        // rsp
    pushq   %rax        // rflags
    pushq   $0x2b       // cs
    pushq   %rdi        // rip
    swapgs
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

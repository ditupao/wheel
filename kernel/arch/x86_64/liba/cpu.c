#include <wheel.h>

typedef struct gdt_ptr {
    u16 limit;
    u64 base;
} __PACKED gdt_ptr_t;

typedef struct idt_ent {
    u16 offset_low;
    u16 selector;
    u16 attr;
    u16 offset_mid;
    u32 offset_high;
    u32 reserved;
} __PACKED idt_ent_t;

typedef struct idt_ptr {
    u16 limit;
    u64 base;
} __PACKED idt_ptr_t;

typedef struct tss {
    u32 reserved1;
    u32 rsp0_lower;
    u32 rsp0_upper;
    u32 rsp1_lower;
    u32 rsp1_upper;
    u32 rsp2_lower;
    u32 rsp2_upper;
    u32 reserved2;
    u32 reserved3;
    u32 ist1_lower;
    u32 ist1_upper;
    u32 ist2_lower;
    u32 ist2_upper;
    u32 ist3_lower;
    u32 ist3_upper;
    u32 ist4_lower;
    u32 ist4_upper;
    u32 ist5_lower;
    u32 ist5_upper;
    u32 ist6_lower;
    u32 ist6_upper;
    u32 ist7_lower;
    u32 ist7_upper;
    u32 reserved4;
    u32 reserved5;
    u16 reserved6;
    u16 io_map_base;
} __PACKED tss_t;

static u64       gdt[MAX_CPU_COUNT*2+6];
static idt_ent_t idt[MAX_INT_COUNT];
__PERCPU tss_t   tss;

__INITDATA u32 cpu_installed = 0;  // number of cpu installed
           u32 cpu_activated = 0;  // number of cpu activated
           u64 percpu_base   = 0;  // cpu0's offset to its percpu area
           u64 percpu_size   = 0;  // length of one per-cpu area
__PERCPU   u32 int_depth;
__PERCPU   u64 int_stack_ptr;
__PERCPU   u8  int_stack[16*PAGE_SIZE];

// interrupt service routines
isr_proc_t isr_tbl[MAX_INT_COUNT];

// defined in cpu.S
extern void entry_0 ();
extern void entry_1 ();
extern void entry_sc();
extern void load_gdtr(gdt_ptr_t * ptr);
extern void load_idtr(idt_ptr_t * ptr);
extern void load_tr  (u16 sel);

//------------------------------------------------------------------------------
// essential cpu features

static __INITDATA int support_fsgsbase = 0;
static __INITDATA int support_erms     = 0;
static __INITDATA int support_noexec   = 0;

__INIT void cpu_init() {
    u32 a, b, c, d;

    if (0 == cpu_activated) {
        a = 1;
        cpuid(&a, &b, &c, &d);
        if (c & (1U <<  0)) { /*dbg_print(", sse3");*/       }
        if (c & (1U <<  9)) { /*dbg_print(", ssse3");*/      }
        if (c & (1U << 19)) { /*dbg_print(", sse4.1");*/     }
        if (c & (1U << 20)) { /*dbg_print(", sse4.2");*/     }
        if (c & (1U <<  6)) { /*dbg_print(", sse4a");*/      }
        if (c & (1U << 11)) { /*dbg_print(", sse5-xop");*/   }
        if (c & (1U << 16)) { /*dbg_print(", sse5-fma4");*/  }
        if (c & (1U << 29)) { /*dbg_print(", sse5-cvt16");*/ }
        if (c & (1U << 28)) { /*dbg_print(", sse5-avx");*/   }

        a = 7;
        c = 0;
        cpuid(&a, &b, &c, &d);
        support_fsgsbase = (b & (1U << 0)) ? 1 : 0;
        support_erms     = (b & (1U << 9)) ? 1 : 0;

        // check extended processor info and feature bits
        a = 0x80000001;
        cpuid(&a, &b, &c, &d);
        support_noexec = (d & (1U << 20)) ? 1 : 0;  // NX bit in page entries
    }

    u64 cr0 = read_cr0();
    cr0 |=  (1UL <<  1);        // cr0.MP
    cr0 &= ~(1UL <<  2);        // cr0.EM: disable emulated mode
    cr0 |=  (1UL <<  5);        // cr0.NE: enable native exception
    cr0 |=  (1UL << 16);        // cr0.WP: enable write protection
    write_cr0(cr0);

    u64 cr4 = read_cr4();
    cr4 |= (1UL << 16);         // FSGSBASE, enable wrfsbase/wrgsbase in ring3
    write_cr4(cr4);

    // enable No-Execute bit in page entries
    u64 efer = read_msr(0xc0000080);
    efer |= (1UL << 11);        // no-execute mode enable
    efer |= (1UL << 14);        // fast fxsave/fxrstor
    write_msr(0xc0000080, efer);
}

__INIT void gdt_init() {
    if (0 == cpu_activated) {
        gdt[0] = 0UL;                   // dummy
        gdt[1] = 0x00a0980000000000UL;  // kernel code
        gdt[2] = 0x00c0920000000000UL;  // kernel data
        gdt[3] = 0x00a0f80000000000UL;  // user code
        gdt[4] = 0x00c0f20000000000UL;  // user data
        gdt[5] = 0UL;                   // padding
    }
    gdt_ptr_t gdtr;
    gdtr.base = (u64) gdt;
    gdtr.limit = 48 + 16 * MAX_CPU_COUNT - 1;
    load_gdtr(&gdtr);
}

// all entries being 64bit Interrupt Gate, no IST
static __INIT void fill_idt_ent(idt_ent_t * entry, u64 isr, int dpl) {
    entry->attr        = 0x8e00 | ((dpl & 3) << 13);
    entry->selector    = 0x08;
    entry->offset_low  =  isr        & 0xffff;
    entry->offset_mid  = (isr >> 16) & 0xffff;
    entry->offset_high = (isr >> 32) & 0xffffffff;
    entry->reserved    = 0;
}

__INIT void idt_init() {
    if (0 == cpu_activated) {
        u64 entry = (u64) entry_0;
        u64 step  = (u64) entry_1 - (u64) entry_0;
        for (int vec = 0; vec < MAX_INT_COUNT; ++vec) {
            if ((vec == 0x80) || (vec == 0x81)) {
                fill_idt_ent(&idt[vec], (u64) entry_sc, 3);
            } else {
                fill_idt_ent(&idt[vec], entry, 0);
            }
            entry += step;
        }
    }
    idt_ptr_t idtr;
    idtr.base  = (u64) &idt;
    idtr.limit = MAX_INT_COUNT * sizeof(idt_ent_t) - 1;
    load_idtr(&idtr);
}

__INIT void tss_init() {
    u32 idx = cpu_activated;
    u64 tss_size = (u64) sizeof(tss_t);
    u64 tss_addr = (u64) percpu_ptr(idx, tss);
    memset((void *) tss_addr, 0, tss_size);

    u64 lower = 0UL;
    u64 upper = 0UL;
    lower |=  tss_size        & 0x000000000000ffffUL;   // limit [15:0]
    lower |= (tss_addr << 16) & 0x000000ffffff0000UL;   // base  [23:0]
    lower |= (tss_size << 32) & 0x000f000000000000UL;   // limit [19:16]
    lower |= (tss_addr << 32) & 0xff00000000000000UL;   // base  [31:24]
    lower |=                    0x0000e90000000000UL;   // present 64bit ring3
    upper  = (tss_addr >> 32) & 0x00000000ffffffffUL;   // base  [63:32]

    gdt[2 * idx + 6] = lower;
    gdt[2 * idx + 7] = upper;

    load_tr(((2 * idx + 6) << 3) | 3);
}

u32 int_lock() {
    u64 key;
    ASM("pushfq; cli; popq %0" : "=r"(key));
    return (u32) key & 0x200;
}

void int_unlock(u32 key) {
    if (key & 0x200) {
        ASM("sti");
    }
}

// setup interrupt handling, clear handler table and interrupt stack
__INIT void int_init() {
    memset(isr_tbl, 0, MAX_INT_COUNT * sizeof(isr_proc_t));
    for (u32 i = 0; i < cpu_installed; ++i) {
        u64 top = (u64) percpu_ptr(i, int_stack[16*PAGE_SIZE]);
        percpu_var(i, int_stack_ptr) = top;
        percpu_var(i, int_depth)     = 0;
    }
}

//------------------------------------------------------------------------------
// exception/interrupt/syscall dispatcher

void exp_dispatch(int vec, int_frame_t * f) {
    static const char * sym[] = {
        "DE", "DB", "NMI","BP", "OF", "BR", "UD", "NM",
        "DF", "??", "TS", "NP", "SS", "GP", "PF", "??",
        "MF", "AC", "MC", "XF", "??", "??", "??", "??",
        "??", "??", "??", "??", "??", "??", "SX", "??"
    };

    dbg_print("#%s:", sym[vec]);
    dbg_print(" sp=%x:%llx", f->ss, f->rsp);
    dbg_print(" ip=%x:%llx", f->cs, f->rip);
    dbg_print(" flg=%llx err=%llx.\r\n", f->rflags, f->errcode);
    dbg_trace();

    while (1) {}
}

void int_dispatch(int vec, int_frame_t * f) {
    if (NULL != isr_tbl[vec]) {
        isr_tbl[vec](vec, f);
    } else {
        dbg_print("INT#%x!\n", vec);
        while (1) {}
    }
}

void syscall_dispatch() {
    dbg_print("handling system call.\r\n");
    // while (1) {}
}

//------------------------------------------------------------------------------
// task support

extern __NORETURN void task_entry(void * proc, void * a1, void * a2, void * a3, void * a4);

void regs_init(regs_t * regs, void * sp, void * proc,
               void * a1, void * a2, void * a3, void * a4) {
    dbg_assert(0 != regs);
    dbg_assert(0 != sp);
    dbg_assert(0 != proc);

    memset(regs, 0, sizeof(regs_t));
    sp = (void *) ((u64) sp & ~7);
    regs->rsp    = (int_frame_t *) ((u64) sp - sizeof(int_frame_t));
    regs->rsp0   = (u64) sp;

    regs->rsp->cs     = 0x08;             // kernel code segment
    regs->rsp->ss     = 0x10;             // kernel data segment
    regs->rsp->rip    = (u64) task_entry; // entry address
    regs->rsp->rsp    = (u64) sp;         // stack top
    regs->rsp->rflags = 0x0200;           // interrupt enabled
    regs->rsp->rdi    = (u64) proc;
    regs->rsp->rsi    = (u64) a1;
    regs->rsp->rdx    = (u64) a2;
    regs->rsp->rcx    = (u64) a3;
    regs->rsp->r8     = (u64) a4;
}

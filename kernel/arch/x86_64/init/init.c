#include <wheel.h>

// defined in `layout.ld`
extern u8 _trampoline_addr;
extern u8 _trampoline_end;
extern u8 _percpu_addr;
extern u8 _percpu_end;
extern u8 _ramfs_addr;
extern u8 _init_end;
extern u8 _text_end;
extern u8 _rodata_end;
extern u8 _kernel_end;

//------------------------------------------------------------------------------
// parse madt table, find all local apic and io apic

static __INIT void parse_madt(madt_t * tbl) {
    u8 * end = (u8 *) tbl + tbl->header.length;
    u8 * p   = (u8 *) tbl + sizeof(madt_t);

    cpu_installed = 0;
    cpu_activated = 0;

    loapic_override(tbl->loapic_addr);
    while (p < end) {
        acpi_subtbl_t * sub = (acpi_subtbl_t *) p;
        switch (sub->type) {
        case MADT_TYPE_IO_APIC:
            ioapic_dev_add((madt_ioapic_t *) sub);
            break;
        case MADT_TYPE_INTERRUPT_OVERRIDE:
            ioapic_int_override((madt_int_override_t *) sub);
            break;
        case MADT_TYPE_LOCAL_APIC:
            loapic_dev_add((madt_loapic_t *) sub);
            break;
        case MADT_TYPE_LOCAL_APIC_OVERRIDE:
            loapic_override(((madt_loapic_override_t *) sub)->address);
            break;
        case MADT_TYPE_LOCAL_APIC_NMI:
            loapic_set_nmi((madt_loapic_mni_t *) sub);
            break;
        default:
            dbg_print("madt entry type %d not known!\r\n", sub->type);
            break;
        }
        p += sub->length;
    }
}

//------------------------------------------------------------------------------
// parse physical memory map

static __INIT void parse_mmap(u8 * mmap_base, u32 mmap_size) {
    // reserve space for percpu data
    u8 * v_end   = (u8 *) ROUND_UP((u64) &_kernel_end, 64);
    percpu_base = (u64) (v_end - &_percpu_addr);
    percpu_size = ROUND_UP((u64) (&_percpu_end - &_percpu_addr), 64);
    for (u32 i = 0; i < cpu_installed; ++i) {
        u8 * dst = &_percpu_addr + percpu_base + percpu_size * i;
        memcpy(dst, &_percpu_addr, percpu_size);
    }

    // page array comes right after percpu area
    v_end = (u8 *) ROUND_UP((u64) v_end + cpu_installed * percpu_size, 16);
    page_array = (page_t *) v_end;
    page_count = 0;

    // walk through the memory layout table, fill invalid entries of page array
    mb_mmap_item_t * map_end = (mb_mmap_item_t *) (mmap_base + mmap_size);
    for (mb_mmap_item_t * item = (mb_mmap_item_t *) mmap_base; item < map_end;) {
        pfn_t start = (item->addr + PAGE_SIZE - 1) >> PAGE_SHIFT;
        pfn_t end   = (item->addr + item->len)     >> PAGE_SHIFT;
        if ((start < end) && (MB_MEMORY_AVAILABLE == item->type)) {
            for (; page_count < start; ++page_count) {
                page_array[page_count].type = PT_INVALID;
            }
            for (; page_count < end; ++page_count) {
                page_array[page_count].type = PT_KERNEL;
            }
        }
        item = (mb_mmap_item_t *) ((u64) item + item->size + sizeof(item->size));
    }

    // walk through the table again, add usable ranges to page frame allocator
    u64 p_end = ROUND_UP(virt_to_phys(&page_array[page_count]), PAGE_SIZE);
    for (mb_mmap_item_t * item = (mb_mmap_item_t *) mmap_base; item < map_end;) {
        u64 start = ROUND_UP(item->addr, PAGE_SIZE);
        u64 end   = ROUND_DOWN(item->addr + item->len, PAGE_SIZE);
        if ((start < end) && (MB_MEMORY_AVAILABLE == item->type)) {
            if (start < KERNEL_LMA) {
                dbg_print("+ ram 0x%08llx-0x%08llx.\r\n", start, MIN(KERNEL_LMA, end));
                page_range_add(start, MIN(KERNEL_LMA, end));
            }
            if (p_end < end) {
                dbg_print("+ ram 0x%08llx-0x%08llx.\r\n", MAX(start, p_end), end);
                page_range_add(MAX(start, p_end), end);
            }
        }
        item = (mb_mmap_item_t *) ((u64) item + item->size + sizeof(item->size));
    }
}

//------------------------------------------------------------------------------
// create page table for kernel context

// TODO: add MMU_KERNEL mark to mapping attributes
// we allow user access just for test purpose
static __INIT usize ctx_create() {
    usize virt, phys, mark;
    usize ctx = mmu_ctx_create();

    // boot, trampoline and init sections
    virt = KERNEL_VMA;
    phys = KERNEL_LMA;
    mark = ROUND_UP(&_init_end, PAGE_SIZE);
    mmu_map(ctx, virt, phys, (mark - virt) >> PAGE_SHIFT, 0);    // MMU_KERNEL

    // kernel code section
    virt = mark;
    phys = virt - KERNEL_VMA + KERNEL_LMA;
    mark = ROUND_UP(&_text_end, PAGE_SIZE);
    mmu_map(ctx, virt, phys, (mark - virt) >> PAGE_SHIFT, MMU_RDONLY); // MMU_KERNEL

    // kernel read only data section
    virt = mark;
    phys = virt - KERNEL_VMA + KERNEL_LMA;
    mark = ROUND_UP(&_rodata_end, PAGE_SIZE);
    mmu_map(ctx, virt, phys, (mark - virt) >> PAGE_SHIFT, MMU_RDONLY|MMU_NOEXEC);  // MMU_KERNEL

    // kernel data section
    virt = mark;
    phys = virt - KERNEL_VMA + KERNEL_LMA;
    mark = ROUND_UP(&page_array[page_count], PAGE_SIZE);
    mmu_map(ctx, virt, phys, (mark - virt) >> PAGE_SHIFT, MMU_NOEXEC); // MMU_KERNEL

    // map all physical memory to higher half
    // TODO: only map present pages, and add IO/Local APIC in driver
    mmu_map(ctx, MAPPED_ADDR, 0, 1U << 20, MMU_NOEXEC);    // MMU_KERNEL

    // map all physical memory to lower half (identity mapping)
    mmu_map(ctx, 0, 0, 1U << 20, 0);

    return ctx;
}

//------------------------------------------------------------------------------
// pre-kernel initialization routines

// backup multiboot structures
static __INITDATA mb_info_t mbi;

// kernel page table
static __INITDATA usize  kernel_ctx;

// tcb for root and idle tasks
static __INITDATA task_t root_tcb;
static __PERCPU   task_t idle_tcb;

// forward declarations
static void root_proc();
static void idle_proc();

__INIT __NORETURN void sys_init_bsp(u32 ebx) {
    // enable early debug output
    serial_dev_init();
    dbg_print("wheel operating system starting up.\r\n");

    // backup multiboot info
    mbi = * (mb_info_t *) phys_to_virt(ebx);

    // save memory map
    u64 mmap_size = mbi.mmap_length;
    u8  mmap_base[mmap_size];
    memcpy(mmap_base, phys_to_virt(mbi.mmap_addr), mmap_size);

    // parse elf section header table, find symbol table and string table
    u8 * shtbl    = (u8 *) phys_to_virt(mbi.elf.addr);
    u8 * sym_addr = NULL;
    u8 * str_addr = NULL;
    u64  sym_size = 0;
    u64  str_size = 0;
    for (u32 i = 1; i < mbi.elf.num; ++i) {
        elf64_shdr_t * sym = (elf64_shdr_t *) (shtbl + mbi.elf.size * i);
        elf64_shdr_t * str = (elf64_shdr_t *) (shtbl + mbi.elf.size * sym->sh_link);
        if ((SHT_SYMTAB == sym->sh_type) && (SHT_STRTAB == str->sh_type)) {
            sym_addr = (u8 *) phys_to_virt(sym->sh_addr);
            str_addr = (u8 *) phys_to_virt(str->sh_addr);
            sym_size = sym->sh_size;
            str_size = str->sh_size;
            break;
        }
    }

    // backup symbol table and string table
    u8 symtab[sym_size];
    u8 strtab[str_size];
    memcpy(symtab, sym_addr, sym_size);
    memcpy(strtab, str_addr, str_size);

    // parse madt and get multiprocessor info
    acpi_tbl_init();
    parse_madt(acpi_madt);

    // init page frame allocator
    page_lib_init();
    parse_mmap(mmap_base, mmap_size);

    // init essential cpu functions
    cpu_init();
    gdt_init();
    idt_init();
    tss_init();
    dbg_regist(symtab, sym_size, strtab, str_size);
    write_gsbase(percpu_base);

    // init interrupt handling
    int_init();
    ioapic_init_all();
    loapic_dev_init();

    kernel_ctx = ctx_create();
    mmu_ctx_set(kernel_ctx);

    // init core kernel facilities
    work_lib_init();
    tick_lib_init();
    task_lib_init();

    // dummy tcb
    task_t tcb_temp = { .priority = PRIORITY_IDLE };
    thiscpu_var(tid_prev) = &tcb_temp;
    thiscpu_var(tid_next) = &tcb_temp;

    // prepare tcb for root and idle-0
    task_init(&root_tcb, 10, 0, root_proc, 0,0,0,0);
    task_init(thiscpu_ptr(idle_tcb), PRIORITY_IDLE, 0, idle_proc, 0,0,0,0);

    // activate two tasks, switch to root automatically
    atomic32_inc(&cpu_activated);
    task_resume(thiscpu_ptr(idle_tcb)); 
    task_resume(&root_tcb);

    dbg_print("YOU CAN'T SEE THIS LINE!\r\n");
    while (1) {}
}

__INIT __NORETURN void sys_init_ap() {
    // essential cpu features
    cpu_init();
    gdt_init();
    idt_init();
    tss_init();
    write_gsbase(percpu_base + cpu_activated * percpu_size);

    // setup local apic and timer
    loapic_dev_init();

    // dummy tcb
    task_t tcb_temp = { .priority = PRIORITY_IDLE };
    thiscpu_var(tid_prev) = &tcb_temp;
    thiscpu_var(tid_next) = &tcb_temp;

    // prepare tcb for idle task
    task_init(thiscpu_ptr(idle_tcb), PRIORITY_IDLE, cpu_activated, idle_proc, 0,0,0,0);

    dbg_print("processor %02d running.\r\n", cpu_activated);

    // activate idle-x, and switch task manually
    atomic32_inc((u32 *) &cpu_activated);
    task_resume(thiscpu_ptr(idle_tcb));             // won't preempt
    thiscpu_var(tid_next) = thiscpu_ptr(idle_tcb);
    task_switch();

    dbg_print("YOU CAN'T SEE THIS LINE!\r\n");
    while (1) {}
}

__INIT __NORETURN void sys_init(u32 eax, u32 ebx) {
    switch (eax) {
    case 0x2badb002: sys_init_bsp(ebx); break;
    case 0xdeadbeef: sys_init_ap();     break;
    default:                            break;
    }
    while (1) {}
}

//------------------------------------------------------------------------------
// post-kernel initialization

// void user_code() {
//     char * video = (char *) phys_to_virt(0xb8000);

//     video[0] = '3';
//     video[1] = 0x4e;

//     ASM("int $0x80");

//     video[2] = '4';
//     video[3] = 0x4e;

//     while (1) {}
// }

static void root_proc() {
    console_dev_init();
    dbg_print("processor 00 running.\r\n");

    // copy trampoline code
    u8 * src = (u8 *) &_trampoline_addr;
    u8 * dst = (u8 *) phys_to_virt(0x7c000);
    u64  len = (u64) (&_trampoline_end - &_trampoline_addr);
    memcpy(dst, src, len);

    // start application processors one-by-one
    while (cpu_activated < cpu_installed) {
        u32 idx = cpu_activated;
        loapic_emit_init(idx);       loapic_timer_busywait(10); // INIT+10ms
        loapic_emit_sipi(idx, 0x7c); loapic_timer_busywait(1);  // SIPI+1ms
        loapic_emit_sipi(idx, 0x7c); loapic_timer_busywait(1);  // SIPI+1ms
        while (percpu_var(idx, tid_prev) != percpu_ptr(idx, idle_tcb)) {
            loapic_timer_busywait(10);
        }
    }

    dbg_print("all cpu started!\r\n");

    u8  * bin_addr = &_ramfs_addr;
    usize bin_size = (usize) (&_init_end - &_ramfs_addr);
    usize entry = elf64_parse(bin_addr, bin_size);
    // dbg_print("elf file parsing %s.\r\n", (OK == ret) ? "ok" : "failed");
    if (0 != entry) {
        // allocate stack space for user-mode stack
        pfn_t uframe = page_block_alloc(ZONE_DMA|ZONE_NORMAL, 0);
        u64 ustack = (u64) phys_to_virt((usize) uframe << PAGE_SHIFT);
        enter_user(entry, ustack + PAGE_SIZE);
    } else {
        dbg_print("elf file parsing error, cannot execute!\r\n");
    }

#if 0
    // jump into ring3 (code linked in higher half)
    pfn_t uframe = page_block_alloc(ZONE_DMA|ZONE_NORMAL, 0);
    u64 ustack = (u64) phys_to_virt((usize) uframe << PAGE_SHIFT);

    dbg_print("jumping to ring3 at %llx %llx.\r\n", (u64) user_code, ustack+PAGE_SIZE);
    enter_user((u64) user_code, ustack+PAGE_SIZE);
#endif

    while (1) {}
}

static void idle_proc() {
    while (1) {
        ASM("hlt");
    }
}

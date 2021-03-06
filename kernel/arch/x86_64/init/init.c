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
            dbg_print("madt entry type %d not known!\n", sub->type);
            break;
        }
        p += sub->length;
    }
}

//------------------------------------------------------------------------------
// parse physical memory map

static __INIT void parse_mmap(u8 * mmap_buf, u32 mmap_len) {
    // reserve space for percpu data
    u8 * v_end   = (u8 *) ROUND_UP((u64) &_kernel_end, 64);
    percpu_base = (u64) (v_end - &_percpu_addr);
    percpu_size = ROUND_UP((u64) (&_percpu_end - &_percpu_addr), 64);
    for (int i = 0; i < cpu_installed; ++i) {
        u8 * dst = &_percpu_addr + percpu_base + percpu_size * i;
        memcpy(dst, &_percpu_addr, percpu_size);
    }

    // page array comes right after percpu area
    v_end = (u8 *) ROUND_UP((u64) v_end + cpu_installed * percpu_size, 16);
    page_array = (page_t *) v_end;
    page_count = 0;

    // walk through the memory layout table, fill invalid entries of page array
    mb_mmap_item_t * map_end = (mb_mmap_item_t *) (mmap_buf + mmap_len);
    for (mb_mmap_item_t * item = (mb_mmap_item_t *) mmap_buf; item < map_end;) {
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
    for (mb_mmap_item_t * item = (mb_mmap_item_t *) mmap_buf; item < map_end;) {
        u64 start = ROUND_UP(item->addr, PAGE_SIZE);
        u64 end   = ROUND_DOWN(item->addr + item->len, PAGE_SIZE);
        if ((start < end) && (MB_MEMORY_AVAILABLE == item->type)) {
            if (start < KERNEL_LMA) {
                dbg_print("+ ram 0x%08llx-0x%08llx.\n", start, MIN(KERNEL_LMA, end));
                page_range_add(start, MIN(KERNEL_LMA, end));
            }
            if (p_end < end) {
                dbg_print("+ ram 0x%08llx-0x%08llx.\n", MAX(start, p_end), end);
                page_range_add(MAX(start, p_end), end);
            }
        }
        item = (mb_mmap_item_t *) ((u64) item + item->size + sizeof(item->size));
    }
}

//------------------------------------------------------------------------------
// pre-kernel initialization routines

// backup multiboot structures
static __INITDATA mb_info_t mbi;

// forward declarations
static void root_proc();

__INIT __NORETURN void sys_init_bsp(u32 ebx) {
    serial_dev_init();
    console_dev_init();
    dbg_print("wheel operating system starting up.\n");

    // backup multiboot info
    mbi = * (mb_info_t *) phys_to_virt(ebx);

    // save memory map
    u64 mmap_len = mbi.mmap_length;
    u8  mmap_buf[mmap_len];
    memcpy(mmap_buf, phys_to_virt(mbi.mmap_addr), mmap_len);

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

    // init page frame allocator, requires multicore info
    page_lib_init();
    parse_mmap(mmap_buf, mmap_len);

    // init essential cpu functions
    dbg_regist(symtab, sym_size, strtab, str_size);
    cpu_init();
    gdt_init();
    idt_init();
    tss_init();
    write_gsbase(percpu_base);

    // init interrupt handling
    int_init();
    ioapic_all_init();
    loapic_dev_init();

    // create and switch to kernel page table
    kernel_ctx_init();

    // init core kernel features
    work_lib_init();
    tick_lib_init();
    task_lib_init();
    sched_lib_init();

    // dummy tcb, allocated on stack
    task_t tcb_temp = { .priority = PRIORITY_IDLE + 1 };
    thiscpu_var(tid_prev) = &tcb_temp;
    thiscpu_var(tid_next) = &tcb_temp;

    // cpu pre-kernel initialization finished
    dbg_print("> cpu %02d started.\n", cpu_activated);
    atomic32_inc((u32 *) &cpu_activated);

    // start executing root task
    task_t * root = task_create("root", PRIORITY_NONRT, root_proc, 0,0,0,0);
    task_resume(root);

    dbg_print("YOU CAN'T SEE THIS LINE!\n");
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

    // load kernel page table
    kernel_ctx_load();

    // dummy tcb, allocated on stack
    task_t tcb_temp = { .priority = PRIORITY_IDLE + 1 };
    thiscpu_var(tid_prev) = &tcb_temp;
    thiscpu_var(tid_next) = &tcb_temp;

    // cpu pre-kernel initialization finished
    dbg_print("> cpu %02d started.\n", cpu_activated);
    atomic32_inc((u32 *) &cpu_activated);

    // start executing idle task
    sched_yield();

    dbg_print("YOU CAN'T SEE THIS LINE!\n");
    while (1) {}
}

__INIT __NORETURN void sys_init(u32 eax, u32 ebx) {
    switch (eax) {
    case 0x2badb002: sys_init_bsp(ebx); break;
    case 0xdeadbeef: sys_init_ap ();    break;
    default:                            break;
    }
    while (1) {}
}

//------------------------------------------------------------------------------
// post-kernel initialization

static void root_proc() {
    // copy trampoline code
    u8 * src = (u8 *) &_trampoline_addr;
    u8 * dst = (u8 *) phys_to_virt(0x7c000);
    u64  len = (u64) (&_trampoline_end - &_trampoline_addr);
    memcpy(dst, src, len);

    // start application processors one-by-one
    while (cpu_activated < cpu_installed) {
        int idx = cpu_activated;
        loapic_emit_init(idx);          // send INIT
        tick_delay(10);                 // wait for 10ms
        loapic_emit_sipi(idx, 0x7c);    // send SIPI
        tick_delay(1);                  // wait for 1ms
        loapic_emit_sipi(idx, 0x7c);    // send SIPI again
        tick_delay(1);                  // wait for 1ms

        // during pre-kernel stage, each cpu use the same kernel stack
        // so we have to start each cpu one-by-one
        while ((percpu_var(idx, tid_prev) == NULL) ||
               (percpu_var(idx, tid_prev)->priority > PRIORITY_IDLE)) {
            tick_delay(10);
        }
    }

    // initialize the rest of kernel features
    vmspace_lib_init();
    process_lib_init();
    syscall_lib_init();

    ios_lib_init();

    // initialize device driver(s)
    kbd_lib_init();     // drvs
    tty_lib_init();     // drvs
    ps2kbd_dev_init();  // arch-drvs

    // create tty device for stdin to work
    tty_dev_create();

    // dbg_print("content of ramfs.tar:\n");
    // tar_dump(&_ramfs_addr);

    const char * argv[] = {
        "setup.app",
        "hello",
        "world",
        NULL
    };
    const char * envp[] = {
        "SYSTEM=wheel",
        "HOST=x86_64",
        NULL
    };
    do_spawn_process(argv[0], argv, envp);
}

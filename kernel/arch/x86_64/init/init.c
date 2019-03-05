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
    usize page_count = 0;

    // walk through the memory layout table, fill invalid entries of page array
    mb_mmap_item_t * map_end = (mb_mmap_item_t *) (mmap_base + mmap_size);
    for (mb_mmap_item_t * item = (mb_mmap_item_t *) mmap_base; item < map_end;) {
        pfn_t start = (item->addr + PAGE_SIZE - 1) >> PAGE_SHIFT;
        pfn_t end   = (item->addr + item->len)     >> PAGE_SHIFT;
        if ((start < end) && (MB_MEMORY_AVAILABLE == item->type)) {
            for (pfn_t p = page_count; p < start; ++p) {
                page_array[p].type = PT_INVALID;
            }
            page_count = end;
        }
        item = (mb_mmap_item_t *) ((u64) item + item->size + sizeof(item->size));
    }

    // mark ram used by kernel image
    u64 p_end = ROUND_UP(virt_to_phys(&page_array[page_count]), PAGE_SIZE);
    for (pfn_t p = (KERNEL_LMA >> PAGE_SHIFT); p < (p_end >> PAGE_SHIFT); ++p) {
        page_array[p].type = PT_KERNEL;
    }

    // walk through the table again, add usable ranges to page frame allocator
    for (mb_mmap_item_t * item = (mb_mmap_item_t *) mmap_base; item < map_end;) {
        u64 start = ROUND_UP(item->addr, PAGE_SIZE);
        u64 end   = ROUND_DOWN(item->addr + item->len, PAGE_SIZE);
        if ((start < end) && (MB_MEMORY_AVAILABLE == item->type)) {
            if (start < KERNEL_LMA) {
                dbg_print("+ ram 0x%08llx-0x%08llx.\r\n", start, MIN(KERNEL_LMA, end));
            }
            if (p_end < end) {
                dbg_print("+ ram 0x%08llx-0x%08llx.\r\n", MAX(start, p_end), end);
            }
        }
        item = (mb_mmap_item_t *) ((u64) item + item->size + sizeof(item->size));
    }
}

//------------------------------------------------------------------------------
// parse madt table, find all local apic and io apic

static __INIT void parse_madt(madt_t * tbl) {
    u8 * end = (u8 *) tbl + tbl->header.length;
    u8 * p   = (u8 *) tbl + sizeof(madt_t);

    cpu_installed = 0;
    cpu_activated = 0;

    while (p < end) {
        acpi_subtbl_t * sub = (acpi_subtbl_t *) p;
        switch (sub->type) {
        case MADT_TYPE_IO_APIC:
            dbg_print("io apic.\r\n");
            break;
        case MADT_TYPE_INTERRUPT_OVERRIDE:
            dbg_print("int override.\r\n");
            break;
        case MADT_TYPE_LOCAL_APIC:
            dbg_print("local apic.\r\n");
            ++cpu_installed;
            break;
        case MADT_TYPE_LOCAL_APIC_OVERRIDE:
            dbg_print("local apic override.\r\n");
            break;
        case MADT_TYPE_LOCAL_APIC_NMI:
            dbg_print("local apic nmi.\r\n");
            break;
        default:
            dbg_print("madt entry type %d not known!\r\n", sub->type);
            break;
        }
        p += sub->length;
    }

    cpu_installed = MIN(cpu_installed, MAX_CPU_COUNT);
}

//------------------------------------------------------------------------------
// pre-kernel initialization routines

// backup multiboot structures
static __INITDATA mb_info_t mbi;

__INIT __NORETURN void sys_init_bsp(u32 ebx) {
    // enable early debug output
    serial_dev_init();
    dbg_print("wheel operating system starting up.\r\n");

    // backup multiboot info
    mbi = * (mb_info_t *) phys_to_virt(ebx);
    // TODO: save vbe ctrl info and mode info

    // save memory map
    u64 mmap_size = mbi.mmap_length;
    u8  mmap_base[mmap_size];
    memcpy(mmap_base, phys_to_virt(mbi.mmap_addr), mmap_size);
    // TODO: vbe mode array also needs to be backed up

    // parse madt and get multiprocessor info
    acpi_tbl_init();
    parse_madt(acpi_madt);

    // init page frame allocator
    parse_mmap(mmap_base, mmap_size);

    dbg_print("percpu base 0x%llx, size 0x%llx.\r\n", percpu_base, percpu_size);

    while (1) {}
}

__INIT __NORETURN void sys_init_ap() {
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
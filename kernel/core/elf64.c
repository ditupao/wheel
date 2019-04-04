#include <wheel.h>

// find the smallest power of 2 larger than or equal to x
static inline u32 pow2(u32 x) {
    --x;
    x |= x >>  1;
    x |= x >>  2;
    x |= x >>  4;
    x |= x >>  8;
    x |= x >> 16;
    return x + 1;
}

typedef struct elf64_dyn {
    i64 d_tag;
    union {
        u64 d_val;
        u64 d_ptr;
    } d_un;
} elf64_dyn_t;

#define DT_NULL     0
#define DT_NEEDED   1
#define DT_PLTRELSZ 2
#define DT_PLTGOT   3
#define DT_HASH     4
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_INIT     12
#define DT_FINI     13
#define DT_SONAME   14
#define DT_RPATH    15
#define DT_SYMBOLIC 16
#define DT_REL      17
#define DT_RELSZ    18
#define DT_RELENT   19
#define DT_PLTREL   20
#define DT_DEBUG    21
#define DT_TEXTREL  22
#define DT_JMPREL   23
#define DT_ENCODING 32

// 只有PT_LOAD类型的segment才是需要加载到内存的，其他类型的segment只是提供额外信息
// 因此PT_LOAD相互之间不能重叠。
// filesz和memsz可以不同，剩余的空间使用0填充
void elf64_load_segment(u8 * data, elf64_phdr_t * seg) {
    usize vm_start   = ROUND_DOWN(seg->p_vaddr, PAGE_SIZE);
    usize vm_end     = seg->p_vaddr + seg->p_memsz;
    int   page_count = ROUND_UP(vm_end - vm_start, PAGE_SIZE) >> PAGE_SHIFT;

    // segment pages are organized in single linked list
    pfn_t seg_head = NO_PAGE;
    for (int i = 0; i < page_count; ++i) {
        pfn_t p = page_block_alloc(ZONE_DMA|ZONE_NORMAL, 0);
        if (NO_PAGE == p) {
            dbg_print("memory not enough!\r\n");
            return;
        }
        page_array[p].type = PT_KERNEL;
        page_array[p].next = seg_head;
        seg_head = p;
        mmu_map(mmu_ctx_get(), vm_start + i * PAGE_SIZE, (usize) p << PAGE_SHIFT, 1, 0);
    }

    memcpy((u8 *) seg->p_vaddr, data, seg->p_filesz);
    memset((u8 *) seg->p_vaddr + seg->p_filesz, 0, seg->p_memsz - seg->p_filesz);
}

// parse and load an elf file into the context of current process
usize elf64_parse(u8 * buf, usize len) {
    // retrieve and verify elf header
    elf64_hdr_t * hdr = (elf64_hdr_t *) buf;
    if ((sizeof(elf64_hdr_t) > len)   ||
        (hdr->e_ident[0] != 0x7f)     ||
        (hdr->e_ident[1] != 'E')      ||
        (hdr->e_ident[2] != 'L')      ||
        (hdr->e_ident[3] != 'F')      ||
#if ARCH == x86_64
        (hdr->e_ident[4] != 2)        ||        // 64-bit
        (hdr->e_ident[5] != 1)        ||        // little endian
        (hdr->e_machine  != EM_AMD64) ||        // x86_64
#endif
        ((hdr->e_type != ET_REL) && (hdr->e_type != ET_DYN))) {
        return 0;
    }

    dbg_print("file type is %d, entry point 0x%x.\r\n", hdr->e_type, hdr->e_entry);

    // get program header table's offset, must present
    if ((hdr->e_phoff     == 0) ||
        (hdr->e_phnum     == 0) ||
        (hdr->e_phentsize == 0) ||
        (hdr->e_phoff >= len)   ||
        (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum >= len)) {
        return 0;
    }

    vmspace_t * vm = &(thiscpu_var(tid_prev)->process->vm);
    vmrange_t * ranges[hdr->e_phnum];

    // make sure address space is valid
    for (int i = 0; i < hdr->e_phnum; ++i) {
        elf64_phdr_t * phdr = (elf64_phdr_t *) (buf + hdr->e_phoff + i * hdr->e_phentsize);
        if (PT_LOAD != phdr->p_type) {
            ranges[i] = NULL;
            continue;
        }

        usize vm_start = ROUND_DOWN(phdr->p_vaddr, PAGE_SIZE);
        usize vm_end   = ROUND_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        ranges[i]      = vmspace_alloc_at(vm, vm_start, vm_end - vm_start);
        if (NULL == ranges[i]) {
            dbg_print("-- !!<range not available>!!\r\n");
            return 0;
        }
    }

    // loop through the phdr table again, and load each segment into memory
    for (int i = 0; i < hdr->e_phnum; ++i) {
        elf64_phdr_t * phdr = (elf64_phdr_t *) (buf + hdr->e_phoff + i * hdr->e_phentsize);
        if (PT_LOAD != phdr->p_type) {
            continue;
        }

        usize vm_start = ROUND_DOWN(phdr->p_vaddr, PAGE_SIZE);
        usize vm_end   = ROUND_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        vmrange_t * range = vmspace_alloc_at(vm, vm_start, vm_end - vm_start);
        if (NULL != range) {
            dbg_print("-- !!<range not available>!!\r\n");
            return 0;
        }

        dbg_print("-- LOAD to 0x%llx, size 0x%x, attr %u.\r\n",
                  phdr->p_vaddr, phdr->p_memsz, phdr->p_flags);
        elf64_load_segment(buf + phdr->p_offset, phdr);
    }

    // // loop through the segment table again, this time find all dynamic segments
    // // and perform dynamic linking
    // for (int i = 0; i < hdr->e_phnum; ++i) {
    //     elf64_phdr_t * phdr = (elf64_phdr_t *) (buf + hdr->e_phoff + i * hdr->e_phentsize);
    //     if (PT_DYNAMIC != phdr->p_type) {
    //         continue;
    //     }
    //     dbg_print("-- DYNAMIC to 0x%llx, size 0x%x.\r\n", phdr->p_vaddr, phdr->p_memsz);

    //     elf64_dyn_t * dyn_entries = (elf64_dyn_t *) (buf + phdr->p_offset);
    //     usize         dyn_count   = phdr->p_filesz / sizeof(elf64_dyn_t);
    //     dbg_print("item total length = %d.\r\n", phdr->p_filesz);
    //     for (int j = 0; j < dyn_count; ++j) {
    //         switch (dyn_entries[j].d_tag) {
    //         case DT_REL:
    //             dbg_print("   --> REL.\r\n");
    //             break;
    //         case DT_RELENT:
    //             dbg_print("   --> RELENT.\r\n");
    //             break;
    //         case DT_RELSZ:
    //             dbg_print("   --> RELSZ.\r\n");
    //             break;
    //         case DT_RELA:
    //             dbg_print("   --> RELA.\r\n");
    //             break;
    //         case DT_RELAENT:
    //             dbg_print("   --> RELAENT.\r\n");
    //             break;
    //         case DT_RELASZ:
    //             dbg_print("   --> RELASZ.\r\n");
    //             break;
    //         default:
    //             dbg_print("   --> unknown dyn type %x.\r\n", dyn_entries[j].d_tag);
    //             break;
    //         }
    //     }
    // }

    return hdr->e_entry;

    // get section header table's offset
    if ((hdr->e_shoff     == 0) ||
        (hdr->e_shnum     == 0) ||
        (hdr->e_shentsize == 0) ||
        (hdr->e_shoff >= len)   ||
        (hdr->e_shoff + hdr->e_shentsize * hdr->e_shnum >= len)) {
        // TODO: if no section table, we should assume no relocation info and proceed
        return ERROR;
    }

    // retrieve section name string table
    // TODO: it this index valid?
    elf64_shdr_t * secnametab = (elf64_shdr_t *) (buf + hdr->e_shoff + hdr->e_shstrndx * hdr->e_shentsize);

    // start from index 1, skipping SHN_UNDEF
    for (int i = 1; i < hdr->e_shnum; ++i) {
        elf64_shdr_t * shdr = (elf64_shdr_t *) (buf + hdr->e_shoff + i * hdr->e_shentsize);
        char * name = (char *) (buf + secnametab->sh_offset + shdr->sh_name);
        switch (shdr->sh_type) {
        case SHT_PROGBITS:
            dbg_print("== progbits %s.\r\n", name);
            break;
        case SHT_SYMTAB:
            dbg_print("== symtab %s.\r\n", name);
            break;
        case SHT_DYNSYM:
            dbg_print("== dynsym %s.\r\n", name);
            break;
        case SHT_STRTAB:
            dbg_print("== string table %s.\r\n", name);
            break;
        case SHT_RELA:
            dbg_print("== rela %s.\r\n", name);
            break;
        case SHT_DYNAMIC:
            dbg_print("== dynamic %s.\r\n", name);
            break;
        default:
            dbg_print("== other#%d %s.\r\n", shdr->sh_type, name);
            break;
        }
    }

    dbg_print("object parsing ok.\r\n");
    return OK;
}


// load an elf file into the context of current process
// and start executing the code in it (current task)
int elf64_load_and_run(u8 * elf, usize len) {
    // parse elf file
    // load each segment into memory, and manage vmspace
    // (relocation)
    // switch to ring3 at the entry point

    // retrieve and verify elf header
    elf64_hdr_t * hdr = (elf64_hdr_t *) elf;
    if ((sizeof(elf64_hdr_t) > len)   ||
        (hdr->e_ident[0] != 0x7f)     ||
        (hdr->e_ident[1] != 'E')      ||
        (hdr->e_ident[2] != 'L')      ||
        (hdr->e_ident[3] != 'F')      ||
#if ARCH == x86_64
        (hdr->e_ident[4] != 2)        ||        // 64-bit
        (hdr->e_ident[5] != 1)        ||        // little endian
        (hdr->e_machine  != EM_AMD64) ||        // x86_64
#else
    #error "arch not supported"
#endif
        ((hdr->e_type != ET_REL) && (hdr->e_type != ET_EXEC) && (hdr->e_type != ET_DYN))) {
        // file format error
        return ERROR;
    }

    // get program header table's offset
    if ((hdr->e_phoff     == 0) ||
        (hdr->e_phnum     == 0) ||
        (hdr->e_phentsize == 0) ||
        (hdr->e_phoff >= len)   ||
        (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum >= len)) {
        // segments table not exist
        return ERROR;
    }

    // allocated vm range and pages
    vmspace_t * vm = &(thiscpu_var(tid_prev)->process->vm);

    // make sure address space is valid
    for (int i = 0; i < hdr->e_phnum; ++i) {
        elf64_phdr_t * phdr = (elf64_phdr_t *) (elf + hdr->e_phoff + i * hdr->e_phentsize);
        if (PT_LOAD != phdr->p_type) {
            continue;
        }

        usize vm_start = ROUND_DOWN(phdr->p_vaddr, PAGE_SIZE);
        usize vm_end   = ROUND_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        if (NO == vmspace_is_free(vm, vm_start, vm_end - vm_start)) {
            // cannot full-fill the requirements, exiting
            return 0;
        }
    }

    return OK;
}

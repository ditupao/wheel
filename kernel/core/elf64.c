#include <wheel.h>

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

// load an elf file into the context of current process
// and start executing the code in it (current task)
int elf64_load(u8 * elf, usize len) {
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
    if ((hdr->e_phoff     ==   0) ||
        (hdr->e_phnum     ==   0) ||
        (hdr->e_phentsize ==   0) ||
        (hdr->e_phoff     >= len) ||
        (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum >= len)) {
        // segments table not exist
        return ERROR;
    }

    // virtual address space of this process
    process_t * pid = thiscpu_var(tid_prev)->process;
    vmspace_t * vm  = &pid->vm;

    usize page_required = 0;

    // loop through each segment, make sure vm range is usable
    for (int i = 0; i < hdr->e_phnum; ++i) {
        elf64_phdr_t * phdr = (elf64_phdr_t *) (elf + hdr->e_phoff + i * hdr->e_phentsize);
        if (PT_LOAD != phdr->p_type) {
            continue;
        }

        // align segment to page boundry
        usize vm_start = ROUND_DOWN(phdr->p_vaddr, PAGE_SIZE);
        usize vm_end   = ROUND_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        if (YES != vmspace_is_free(vm, vm_start, vm_end)) {
            return ERROR;
        }

        page_required += (vm_end - vm_start) >> PAGE_SHIFT;
    }

    // check if there's enough free page
    if (free_page_count(ZONE_NORMAL) < page_required) {
        return ERROR;
    }

    // all vm ranges allocated for this elf
    vmrange_t * ranges[hdr->e_phnum];
    memset(ranges, 0, hdr->e_phnum * sizeof(vmrange_t *));

    // loop through each segment again, allocate space and load the content
    for (int i = 0; i < hdr->e_phnum; ++i) {
        elf64_phdr_t * phdr = (elf64_phdr_t *) (elf + hdr->e_phoff + i * hdr->e_phentsize);
        if (PT_LOAD != phdr->p_type) {
            continue;
        }

        // align segment to page boundry
        usize vm_start = ROUND_DOWN(phdr->p_vaddr, PAGE_SIZE);
        usize vm_end   = ROUND_UP  (phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);

        ranges[i] = vmspace_alloc_at(vm, vm_start, vm_end - vm_start);
        if (NULL == ranges[i]) {
            goto error;
        }
        if (vmspace_map(vm, ranges[i])) {
            goto error;
        }

        memcpy((u8 *) phdr->p_vaddr, (u8 *) elf + phdr->p_offset, phdr->p_filesz);
        memset((u8 *) phdr->p_vaddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
    }

    pid->entry = hdr->e_entry;

    // get section header table's offset
    if ((hdr->e_shoff     ==   0) ||
        (hdr->e_shnum     ==   0) ||
        (hdr->e_shentsize ==   0) ||
        (hdr->e_shoff     >= len) ||
        (hdr->e_shoff + hdr->e_shentsize * hdr->e_shnum >= len)) {
        // TODO: if no section table, we should assume no relocation info and proceed
        return OK;
    }

    for (int i = 0; i < hdr->e_shnum; ++i) {
        elf64_shdr_t * shdr = (elf64_shdr_t *) (elf + hdr->e_shoff + i * hdr->e_shentsize);
        if (SHT_REL == shdr->sh_type) {
            //
        }
        if (SHT_RELA == shdr->sh_type) {
            //
        }
    }

    return OK;

error:
    for (int i = 0; i < hdr->e_phnum; ++i) {
        if (NULL == ranges[i]) {
            continue;
        }
        vmspace_unmap(vm, ranges[i]);
        vmspace_free(vm, ranges[i]);
    }
    return ERROR;
}

// __INIT void load_lib_init() {
//     pool_init(&obj_pool, sizeof(vmobject_t));
//     pool_init(&seg_pool, sizeof(vmsegment_t));
// }

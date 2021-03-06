#include <wheel.h>

// copy to a bunch of memory represented by the page list
// if there are space left, fill them with zero
// return the number of bytes not copied
usize copy_to_pglist(pglist_t * pages, u8 * buff, usize len, usize offset) {
    pfn_t blk = pages->head;

    // copy buffer to the page list
    while ((NO_PAGE != blk) && (len > 0)) {
        dbg_assert(offset < PAGE_SIZE);
        dbg_assert(1 == page_array[blk].block);

        u8  * addr = (u8 *) phys_to_virt((usize) blk << PAGE_SHIFT);
        usize size = PAGE_SIZE << page_array[blk].order;
        usize copy = MIN(len, size - offset);
        memcpy(addr + offset, buff, copy);

        buff   += copy;
        offset += copy;
        len    -= copy;
        offset -= size;
        blk     = page_array[blk].next;
    }

    // fill the rest of page list with zero
    while (NO_PAGE != blk) {
        dbg_assert(1 == page_array[blk].block);

        u8  * addr = (u8 *) phys_to_virt((usize) blk << PAGE_SHIFT);
        usize size = PAGE_SIZE << page_array[blk].order;
        memset(addr + offset, 0, size - offset);

        offset = 0;
        blk    = page_array[blk].next;
    }

    return len;
}

// load an elf file into the context of current process
// and start executing the code in it (current task)
int elf64_load(process_t * pid, u8 * elf, usize len) {
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

    // calculate the number of pages required to hold this elf object
    usize page_required = 0;

    // loop through each segment, make sure vmrange is usable
    for (int i = 0; i < hdr->e_phnum; ++i) {
        elf64_phdr_t * phdr = (elf64_phdr_t *) (elf + hdr->e_phoff + i * hdr->e_phentsize);
        if (PT_LOAD != phdr->p_type) {
            continue;
        }

        // align segment to page boundry
        usize vm_start = ROUND_DOWN(phdr->p_vaddr, PAGE_SIZE);
        usize vm_end   = ROUND_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
        if (YES != vmspace_is_free(&pid->vm, vm_start, vm_end)) {
            return ERROR;
        }

        page_required += (vm_end - vm_start) >> PAGE_SHIFT;
    }

    // check if there's enough free page
    if (free_page_count(ZONE_NORMAL) < page_required) {
        return ERROR;
    }

    // all vmranges allocated for this elf
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

        ranges[i] = vmspace_alloc_at(&pid->vm, vm_start, vm_end - vm_start);
        if (NULL == ranges[i]) {
            goto error;
        }
        if (vmspace_map(&pid->vm, ranges[i])) {
            goto error;
        }

        copy_to_pglist(&ranges[i]->pages, elf + phdr->p_offset,
                       phdr->p_filesz, phdr->p_vaddr - vm_start);
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
        vmspace_unmap(&pid->vm, ranges[i]);
        vmspace_free(&pid->vm, ranges[i]);
    }
    return ERROR;
}

#include <wheel.h>

// parse and load an elf file
// we need to keep a handle of each ELF loaded
// so that we can free all pages allocated for it.
int elf_parse(u8 * buf, usize len) {
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
        return ERROR;
    }

    // get program header table's offset, must present
    if ((hdr->e_phoff     == 0) ||
        (hdr->e_phnum     == 0) ||
        (hdr->e_phentsize == 0) ||
        (hdr->e_phoff >= len)   ||
        (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum >= len)) {
        return ERROR;
    }

    // TODO: at least one PT_LOAD segment. exit if not found
    for (int i = 0; i < hdr->e_phnum; ++i) {
        elf64_phdr_t * phdr = (elf64_phdr_t *) (buf + hdr->e_phoff + i * hdr->e_phentsize);
        switch (phdr->p_type) {
        case PT_LOAD:
            dbg_print("-- LOAD to 0x%llx, size 0x%x, attr %u.\r\n", phdr->p_vaddr, phdr->p_memsz, phdr->p_flags);
            break;
        case PT_DYNAMIC:
            dbg_print("-- DYNAMIC to 0x%llx, size 0x%x, attr %u.\r\n", phdr->p_vaddr, phdr->p_memsz, phdr->p_flags);
            break;
        default:
            break;
        }
    }

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
        case SHT_DYNSYM:
            dbg_print("== symtab %s.\r\n", name);
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

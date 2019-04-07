#ifndef CORE_ELF64_H
#define CORE_ELF64_H

#include <base.h>

//------------------------------------------------------------------------------
// elf file header

typedef struct elf64_hdr {
    u8  e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} __PACKED elf64_hdr_t;

// object file types
#define ET_NONE         0
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3
#define ET_CORE         4

// required architecture
#define EM_NONE         0
#define EM_SPARC        2
#define EM_386          3
#define EM_SPARC32PLUS  18
#define EM_SPARCV9      43
#define EM_AMD64        62

//------------------------------------------------------------------------------
// elf segment header

typedef struct elf64_phdr {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} __PACKED elf64_phdr_t;

// segment types
#define PT_NULL             0
#define PT_LOAD             1
#define PT_DYNAMIC          2
#define PT_INTERP           3
#define PT_NOTE             4
#define PT_SHLIB            5
#define PT_PHDR             6
#define PT_TLS              7

// segment flags
#define PF_X                1   // execute
#define PF_W                2   // write
#define PF_R                4   // read

//------------------------------------------------------------------------------
// elf section header

typedef struct elf64_shdr {
    u32 sh_name;
    u32 sh_type;
    u64 sh_flags;
    u64 sh_addr;
    u64 sh_offset;
    u64 sh_size;
    u32 sh_link;
    u32 sh_info;
    u64 sh_addralign;
    u64 sh_entsize;
} __PACKED elf64_shdr_t;

// elf section types
#define SHT_NULL            0
#define SHT_PROGBITS        1
#define SHT_SYMTAB          2
#define SHT_STRTAB          3
#define SHT_RELA            4
#define SHT_HASH            5
#define SHT_DYNAMIC         6
#define SHT_NOTE            7
#define SHT_NOBITS          8
#define SHT_REL             9
#define SHT_SHLIB           10
#define SHT_DYNSYM          11
#define SHT_INIT_ARRAY      14
#define SHT_FINI_ARRAY      15
#define SHT_PREINIT_ARRAY   16
#define SHT_GROUP           17
#define SHT_SYMTAB_SHNDX    18

//------------------------------------------------------------------------------
// symbol table

typedef struct elf64_sym {
    u32 st_name;
    u8  st_info;
    u8  st_other;
    u16 st_shndx;
    u64 st_value;
    u64 st_size;
} __PACKED elf64_sym_t;

// symbol types
#define STT_NONE            0
#define STT_OBJECT          1
#define STT_FUNC            2
#define STT_SECTION         3
#define STT_FILE            4

//------------------------------------------------------------------------------
// public function

// extern usize elf64_parse(u8 * buf, usize len);
extern int elf64_load(u8 * elf, usize len);

#endif // CORE_ELF64_H

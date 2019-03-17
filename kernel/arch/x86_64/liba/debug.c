#include <wheel.h>

static elf64_sym_t * sym_addr = NULL;
static char        * str_addr = NULL;
static usize  sym_size = 0;
static usize  str_size = 0;

// TODO: convert symbol table into another format
__INIT void dbg_regist(u8 * sym_tbl, usize sym_len, u8 * str_tbl, usize str_len) {
    u32 n = ROUND_UP(ROUND_UP(sym_len, 8) + str_len, PAGE_SIZE) >> PAGE_SHIFT;
    pfn_t rng = page_range_alloc(ZONE_DMA|ZONE_NORMAL, n);
    for (pfn_t i = 0; i < n; ++i) {
        page_array[rng + i].type = PT_KERNEL;
    }
    u8 * buf = (u8 *) phys_to_virt((usize) rng << PAGE_SHIFT);
    sym_addr = (elf64_sym_t *) buf;
    str_addr = (char *) buf + ROUND_UP(sym_len, 8);
    memcpy(sym_addr, sym_tbl, sym_len);
    memcpy(str_addr, str_tbl, str_len);
    sym_size = sym_len / sizeof(elf64_sym_t);
    str_size = str_len;
}

// return 1 if we've achieved root
static int dbg_lookup(u64 addr) {
    elf64_sym_t * func = NULL;
    usize         dist = (usize) -1;
    for (usize i = 0; i < sym_size; ++i) {
        if ((STT_FUNC == (sym_addr[i].st_info & 0x0f)) &&
            (sym_addr[i].st_value < addr)              &&
            ((addr - sym_addr[i].st_value) < dist)) {
            dist = addr - sym_addr[i].st_value;
            func = &sym_addr[i];
        }
    }

    if (NULL == func) {
        dbg_print("--> 0x%016llx.\r\n", addr);
    } else {
        dbg_print("--> 0x%016llx (%s + 0x%x).\r\n",
            addr, str_addr + func->st_name, addr - func->st_value);
        if ((0 == strcmp(str_addr + func->st_name, "task_entry")) ||
            (0 == strcmp(str_addr + func->st_name, "sys_init"))) {
            return 1;
        }
    }

    return 0;
}

void dbg_print(const char * msg, ...) {
    va_list args;
    char buf[1024];

    va_start(args, msg);
    vsnprintf(buf, 1023, msg, args);
    va_end(args);

    serial_puts(buf);
    console_puts(buf);
}

void dbg_trace() {
    u64 * rbp;
    ASM("movq %%rbp, %0" : "=r"(rbp));

    for (int i = 0; rbp[0] && (i < 16); ++i) {
        if (dbg_lookup(rbp[1])) {
            break;
        }
        rbp = (u64 *) rbp[0];
    }
}

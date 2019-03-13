#include <wheel.h>

#define VGA_CRTC_ADDR   0x03d4
#define VGA_CRTC_DATA   0x03d5

#define ROW_COUNT       25
#define COL_COUNT       80

#define IDX(row, col) ((((row) + ROW_COUNT) % ROW_COUNT) * COL_COUNT + (col))

typedef struct {
    u8 row;     // current caret row index
    u8 col;     // current caret column index
    u8 attr;    // current text attribute
    u8 base;    // row number of starting line
} __PACKED loc_t;

static int   dev_installed = 0;

static u16   vbuf[ROW_COUNT * COL_COUNT];   // off-screen buffer
static u16 * vram;                          // points to video ram
static loc_t location;

static void _console_set_caret(u16 idx) {
    out8(VGA_CRTC_ADDR, 0x0f);              // caret location low
    out8(VGA_CRTC_DATA, idx & 0xff);
    out8(VGA_CRTC_ADDR, 0x0e);              // caret location high
    out8(VGA_CRTC_DATA, (idx >> 8) & 0xff);
}

static void _console_putc(char c) {
    u32     old_val;
    u32     new_val;
    loc_t * prev;
    loc_t * next;

retry:
    old_val = atomic32_get((u32 *) &location);
    new_val = old_val;
    prev    = (loc_t *) &old_val;
    next    = (loc_t *) &new_val;

    switch (c) {
    case '\t': next->col += 8; next->col &= ~7; break;
    case '\n': next->col  = 0; next->row +=  1; break;
    case '\r': next->col  = 0; break;
    default:   next->col += 1; break;
    }

    while (next->col >= COL_COUNT) {
        next->col -= COL_COUNT;
        next->row += 1;
    }

    u64 fill = (u64) ' ' | ((u64) prev->attr << 8);
    fill |= fill << 16;
    fill |= fill << 32;
    while (next->row - next->base >= ROW_COUNT) {
        u64 * line = (u64 *) &vbuf[IDX(next->base, 0)];
        next->base += 1;
        for (int j = 0; j < COL_COUNT / 4; ++j) { line[j] = fill; }
    }

    if (atomic32_cas((u32 *) &location, old_val, new_val) != old_val) {
        goto retry;
    }

    if (prev->base != next->base) {
        for (int i = 0; i < ROW_COUNT; ++i) {
            memcpy(&vram[IDX(i, 0)], &vbuf[IDX(next->base + i, 0)], 2 * COL_COUNT);
        }
    }

    if ('\t' != c && '\n' != c && '\r' != c) {
        u16 fill = (u16) c | ((u16) prev->attr << 8);
        vbuf[IDX(prev->row, prev->col)] = fill;
        vram[IDX(prev->row - next->base, prev->col)] = fill;
    }
}

void console_putc(const char c) {
    if (!dev_installed) {
        return;
    }
    _console_putc(c);
    _console_set_caret(IDX(location.row - location.base, location.col));
}

void console_puts(const char * s) {
    if (!dev_installed) {
        return;
    }
    for (; *s; ++s) {
        _console_putc(*s);
    }
    _console_set_caret(IDX(location.row - location.base, location.col));
}

__INIT void console_dev_init() {
    u64 * dst = (u64 *) vbuf;
    for (int i = 0; i < ROW_COUNT * COL_COUNT / 4; ++i) {
        dst[i] = 0x1f201f201f201f20UL;
    }

    vram = (u16 *) phys_to_virt(0xb8000);
    memcpy(vram, vbuf, ROW_COUNT * COL_COUNT * 2);

    location.attr = 0x1f;   // white on blue
    location.row  = 0;
    location.col  = 0;
    location.base = 0;

    dev_installed = 1;
}

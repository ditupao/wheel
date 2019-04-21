#include <wheel.h>

// keyboard device has many work modes:
// - RAW,       output scan code directly
// - MEDIUMRAW, output key code
// - XLATE,     output converted ascii or escape sequence

#define PS2KBD_CTRL_PORT 0x64
#define PS2KBD_DATA_PORT 0x60

static void ps2kbd_int_handler(int vec __UNUSED, int_frame_t * sp __UNUSED) {
    if (in8(PS2KBD_CTRL_PORT) & 1) {
        u8 scancode = in8(PS2KBD_DATA_PORT);
        dbg_print("<%02x>", scancode);
    }
    loapic_send_eoi();
}

__INIT void ps2kbd_dev_init() {
    // PS/2 keyboard default connect to PIN1 of PIC, but we use APIC
    // first convert PIC pin number to gsi, then to vector number
    int gsi = ioapic_irq_to_gsi(1);
    int vec = ioapic_gsi_to_vec(gsi);
    isr_tbl[vec] = ps2kbd_int_handler;
    ioapic_gsi_unmask(gsi);
}

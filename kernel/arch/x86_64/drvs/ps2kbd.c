#include <wheel.h>

// keyboard device has many work modes:
// - RAW,       output scan code directly
// - MEDIUMRAW, output key code
// - XLATE,     output converted ascii or escape sequence

// driver only need to convert scan code to key code
// user programs then translate to ascii or unicode

#define PS2KBD_CTRL_PORT 0x64
#define PS2KBD_DATA_PORT 0x60

typedef enum keycode {
    KEY_RESERVED,

    // number keys
    KEY_0,  KEY_1,  KEY_2,  KEY_3,  KEY_4,
    KEY_5,  KEY_6,  KEY_7,  KEY_8,  KEY_9,

    // letteer keys
    KEY_A,  KEY_B,  KEY_C,  KEY_D,  KEY_E,  KEY_F,  KEY_G,
    KEY_H,  KEY_I,  KEY_J,  KEY_K,  KEY_L,  KEY_M,  KEY_N,
    KEY_O,  KEY_P,  KEY_Q,  KEY_R,  KEY_S,  KEY_T,
    KEY_U,  KEY_V,  KEY_W,  KEY_X,  KEY_Y,  KEY_Z,

    // function keys
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10,KEY_F11,KEY_F12,

    // and modifiers
    KEY_LEFTCTRL,   KEY_RIGHTCTRL,
    KEY_LEFTSHIFT,  KEY_RIGHTSHIFT,
    KEY_LEFTALT,    KEY_RIGHTALT,

    // punctuations
    KEY_BACKTICK,   KEY_MINUS,      KEY_EQUAL,      KEY_TAB,
    KEY_LEFTBRACE,  KEY_RIGHTBRACE, KEY_SEMICOLON,  KEY_QUOTE,
    KEY_COMMA,      KEY_DOT,        KEY_SLASH,      KEY_BACKSLASH,
    KEY_SPACE,      KEY_BACKSPACE,  KEY_ENTER,

    // control keys
    KEY_ESCAPE,     KEY_CAPSLOCK,   KEY_NUMLOCK,    KEY_SCROLLLOCK,
    KEY_INSERT,     KEY_DELETE,     KEY_HOME,       KEY_END,
    KEY_PAGEUP,     KEY_PAGEDOWN,
    KEY_UP,         KEY_DOWN,       KEY_LEFT,       KEY_RIGHT,

    // key pad
    KEY_KP_1,   KEY_KP_2,   KEY_KP_3,   KEY_KP_4,   KEY_KP_5,
    KEY_KP_6,   KEY_KP_7,   KEY_KP_8,   KEY_KP_9,   KEY_KP_0,
    KEY_KP_SLASH,   KEY_KP_STAR,    KEY_KP_MINUS,   KEY_KP_PLUS,
    KEY_KP_ENTER,   KEY_KP_DOT,

    // special
    KEY_PRTSC,      KEY_PAUSE,      KEY_APPS,

    // multimedia
    KEY_MM_PREV,    KEY_MM_NEXT,    KEY_MM_PLAY,    KEY_MM_STOP,
    KEY_MM_MUTE,    KEY_MM_CALC,    KEY_MM_VOLUP,   KEY_MM_VOLDOWN,
    KEY_MM_EMAIL,   KEY_MM_SELECT,  KEY_MM_MYCOMPUTER,

    // multimedia www
    KEY_WWW_HOME,   KEY_WWW_SEARCH, KEY_WWW_FAVORITES,
    KEY_WWW_FORWARD,KEY_WWW_BACK,   KEY_WWW_STOP,   KEY_WWW_REFRESH,

    // GUI (windows key)
    KEY_GUI_LEFT,   KEY_GUI_RIGHT,

    // acpi
    KEY_ACPI_POWER, KEY_ACPI_SLEEP, KEY_ACPI_WAKE,
} keycode_t;

// lookup table to convert scan code set 1 to key code
static keycode_t normal_lookup[] = {
    KEY_RESERVED,   KEY_ESCAPE,     KEY_1,          KEY_2,          // 0x00 - 0x03
    KEY_3,          KEY_4,          KEY_5,          KEY_6,          // 0x04 - 0x07
    KEY_7,          KEY_8,          KEY_9,          KEY_0,
    KEY_MINUS,      KEY_EQUAL,      KEY_BACKSPACE,  KEY_TAB,
    KEY_Q,          KEY_W,          KEY_E,          KEY_R,
    KEY_T,          KEY_Y,          KEY_U,          KEY_I,
    KEY_O,          KEY_P,          KEY_LEFTBRACE,  KEY_RIGHTBRACE,
    KEY_ENTER,      KEY_LEFTCTRL,   KEY_A,          KEY_S,
    KEY_D,          KEY_F,          KEY_G,          KEY_H,
    KEY_J,          KEY_K,          KEY_L,          KEY_SEMICOLON,
    KEY_QUOTE,      KEY_BACKTICK,   KEY_LEFTSHIFT,  KEY_BACKSLASH,
    KEY_Z,          KEY_X,          KEY_C,          KEY_V,
    KEY_B,          KEY_N,          KEY_M,          KEY_COMMA,
    KEY_DOT,        KEY_SLASH,      KEY_RIGHTSHIFT, KEY_KP_STAR,
    KEY_LEFTALT,    KEY_SPACE,      KEY_CAPSLOCK,   KEY_F1,
    KEY_F2,         KEY_F3,         KEY_F4,         KEY_F5,
    KEY_F6,         KEY_F7,         KEY_F8,         KEY_F9,
    KEY_F10,        KEY_NUMLOCK,    KEY_SCROLLLOCK, KEY_KP_7,
    KEY_KP_8,       KEY_KP_9,       KEY_KP_MINUS,   KEY_KP_4,
    KEY_KP_5,       KEY_KP_6,       KEY_KP_PLUS,    KEY_KP_1,
    KEY_KP_2,       KEY_KP_3,       KEY_KP_0,       KEY_KP_DOT,
    KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   KEY_F11,
    KEY_F12,        KEY_RESERVED,   KEY_RESERVED,   KEY_RESERVED,   // 0x58 - 0x5b
};

static int kbd_capslock = 0;
static int kbd_numlock  = 1;

// we have to track the state of modifier keys
static int kbd_l_shift   = 0;
static int kbd_r_shift   = 0;
static int kbd_l_control = 0;
static int kbd_r_control = 0;
static int kbd_l_alt     = 0;
static int kbd_r_alt     = 0;

// defined in core/kbd.c
extern pipe_t * kbd_pipe;

// send converted ascii to pipe
static void send_ascii(char c) {
    u8 buf[10];
    buf[0] = (u8) c;
    pipe_write(kbd_pipe, buf, 1);
}

// notice that 0 appears first, different from keyboard layout
static const char syms[] = ")!@#$%^&*(";

// convert from key code to ascii
static void send_keycode(keycode_t key, int release) {
    if (release) {
        switch (key) {
        case KEY_LEFTSHIFT:  kbd_l_shift   = 0; break;
        case KEY_RIGHTSHIFT: kbd_r_shift   = 0; break;
        case KEY_LEFTCTRL:   kbd_l_control = 0; break;
        case KEY_RIGHTCTRL:  kbd_r_control = 0; break;
        case KEY_LEFTALT:    kbd_l_alt     = 0; break;
        case KEY_RIGHTALT:   kbd_r_alt     = 0; break;
        default:                                break;
        }
    } else {
        switch (key) {
        case KEY_LEFTSHIFT:  kbd_l_shift   = 1; break;
        case KEY_RIGHTSHIFT: kbd_r_shift   = 1; break;
        case KEY_LEFTCTRL:   kbd_l_control = 1; break;
        case KEY_RIGHTCTRL:  kbd_r_control = 1; break;
        case KEY_LEFTALT:    kbd_l_alt     = 1; break;
        case KEY_RIGHTALT:   kbd_r_alt     = 1; break;
        case KEY_CAPSLOCK:   kbd_capslock ^= 1; break;
        case KEY_NUMLOCK:    kbd_numlock  ^= 1; break;

        // letters
        case KEY_A: case KEY_B: case KEY_C: case KEY_D: case KEY_E:
        case KEY_F: case KEY_G: case KEY_H: case KEY_I: case KEY_J:
        case KEY_K: case KEY_L: case KEY_M: case KEY_N: case KEY_O:
        case KEY_P: case KEY_Q: case KEY_R: case KEY_S: case KEY_T:
        case KEY_U: case KEY_V: case KEY_W: case KEY_X: case KEY_Y:
        case KEY_Z:
            if (kbd_capslock ^ (kbd_l_shift | kbd_r_shift)) {
                send_ascii('A' + (key - KEY_A));
            } else {
                send_ascii('a' + (key - KEY_A));
            }
            break;

        // numbers
        case KEY_0: case KEY_1: case KEY_2: case KEY_3: case KEY_4:
        case KEY_5: case KEY_6: case KEY_7: case KEY_8: case KEY_9:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii(syms[key - KEY_0]);
            } else {
                send_ascii('0' + (key - KEY_0));
            }
            break;

        case KEY_BACKTICK:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('~');
            } else {
                send_ascii('`');
            }
            break;
        case KEY_MINUS:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('_');
            } else {
                send_ascii('-');
            }
            break;
        case KEY_EQUAL:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('+');
            } else {
                send_ascii('=');
            }
            break;
        case KEY_LEFTBRACE:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('{');
            } else {
                send_ascii('[');
            }
            break;
        case KEY_RIGHTBRACE:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('}');
            } else {
                send_ascii(']');
            }
            break;
        case KEY_SEMICOLON:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii(':');
            } else {
                send_ascii(';');
            }
            break;
        case KEY_QUOTE:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('\"');
            } else {
                send_ascii('\'');
            }
            break;
        case KEY_COMMA:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('<');
            } else {
                send_ascii(',');
            }
            break;
        case KEY_DOT:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('>');
            } else {
                send_ascii('.');
            }
            break;
        case KEY_SLASH:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('?');
            } else {
                send_ascii('/');
            }
            break;
        case KEY_BACKSLASH:
            if (kbd_l_shift | kbd_r_shift) {
                send_ascii('|');
            } else {
                send_ascii('\\');
            }
            break;

        // whitespace
        case KEY_TAB:   send_ascii('\t');   break;
        case KEY_SPACE: send_ascii(' ');    break;
        case KEY_ENTER: send_ascii('\n');   break;

        default:
            break;
        }
    }
}

// some key have multiple scan code
#define STATE_NORMAL        0
#define STATE_E0            1

#define STATE_PRTSC_DOWN_2  2   // print screen pressed, got e0, 2a
#define STATE_PRTSC_DOWN_3  3   // print screen pressed, got e0, 2a, e0
#define STATE_PRTSC_UP_2    4   // print screen released, got e0, b7
#define STATE_PRTSC_UP_3    5   // print screen released, got e0, b7, e0

#define STATE_PAUSE_1       6   // pause sequence, got e1
#define STATE_PAUSE_2       7   // pause sequence, got e1, 1d
#define STATE_PAUSE_3       8   // pause sequence, got e1, 1d, 45
#define STATE_PAUSE_4       9   // pause sequence, got e1, 1d, 45, e1
#define STATE_PAUSE_5       10  // pause sequence, got e1, 1d, 45, e1, 9d

static int state = 0;

// convert from scan code set 1 to key code
static void digest_scan_code(u8 scancode) {
    switch (state) {
    case STATE_NORMAL:
        if (0xe0 == scancode) {
            state = STATE_E0;
            break;
        }
        if (0xe1 == scancode) {     // must be pause_sequence
            state = STATE_PAUSE_1;
            break;
        }
        send_keycode(normal_lookup[scancode & 0x7f], scancode & 0x80);
        break;
    case STATE_E0:
        if (0x2a == scancode) {
            // print screen press sequence
            state = STATE_PRTSC_DOWN_2;
            break;
        }
        if (0xb7 == scancode) {
            // print screen release requence
            state = STATE_PRTSC_UP_2;
            break;
        }
        switch (scancode & 0x7f) {
        case 0x10:  send_keycode(KEY_MM_PREV,       scancode & 0x80);   break;
        case 0x19:  send_keycode(KEY_MM_NEXT,       scancode & 0x80);   break;
        case 0x1c:  send_keycode(KEY_KP_ENTER,      scancode & 0x80);   break;
        case 0x1d:  send_keycode(KEY_RIGHTCTRL,     scancode & 0x80);   break;
        case 0x20:  send_keycode(KEY_MM_MUTE,       scancode & 0x80);   break;
        case 0x21:  send_keycode(KEY_MM_CALC,       scancode & 0x80);   break;
        case 0x22:  send_keycode(KEY_MM_PLAY,       scancode & 0x80);   break;
        case 0x24:  send_keycode(KEY_MM_STOP,       scancode & 0x80);   break;
        case 0x2e:  send_keycode(KEY_MM_VOLDOWN,    scancode & 0x80);   break;
        case 0x30:  send_keycode(KEY_MM_VOLUP,      scancode & 0x80);   break;
        case 0x32:  send_keycode(KEY_WWW_HOME,      scancode & 0x80);   break;
        case 0x35:  send_keycode(KEY_KP_SLASH,      scancode & 0x80);   break;
        case 0x38:  send_keycode(KEY_RIGHTALT,      scancode & 0x80);   break;
        case 0x47:  send_keycode(KEY_HOME,          scancode & 0x80);   break;
        case 0x48:  send_keycode(KEY_UP,            scancode & 0x80);   break;
        case 0x49:  send_keycode(KEY_PAGEUP,        scancode & 0x80);   break;
        case 0x4b:  send_keycode(KEY_LEFT,          scancode & 0x80);   break;
        case 0x4d:  send_keycode(KEY_RIGHT,         scancode & 0x80);   break;
        case 0x4f:  send_keycode(KEY_END,           scancode & 0x80);   break;
        case 0x50:  send_keycode(KEY_DOWN,          scancode & 0x80);   break;
        case 0x51:  send_keycode(KEY_PAGEDOWN,      scancode & 0x80);   break;
        case 0x52:  send_keycode(KEY_INSERT,        scancode & 0x80);   break;
        case 0x53:  send_keycode(KEY_DELETE,        scancode & 0x80);   break;
        case 0x5b:  send_keycode(KEY_GUI_LEFT,      scancode & 0x80);   break;
        case 0x5c:  send_keycode(KEY_GUI_RIGHT,     scancode & 0x80);   break;
        case 0x5d:  send_keycode(KEY_APPS,          scancode & 0x80);   break;
        case 0x5e:  send_keycode(KEY_ACPI_POWER,    scancode & 0x80);   break;
        case 0x5f:  send_keycode(KEY_ACPI_SLEEP,    scancode & 0x80);   break;
        case 0x63:  send_keycode(KEY_ACPI_WAKE,     scancode & 0x80);   break;
        case 0x65:  send_keycode(KEY_WWW_SEARCH,    scancode & 0x80);   break;
        case 0x66:  send_keycode(KEY_WWW_FAVORITES, scancode & 0x80);   break;
        case 0x67:  send_keycode(KEY_WWW_REFRESH,   scancode & 0x80);   break;
        case 0x68:  send_keycode(KEY_WWW_STOP,      scancode & 0x80);   break;
        case 0x69:  send_keycode(KEY_WWW_FORWARD,   scancode & 0x80);   break;
        case 0x6a:  send_keycode(KEY_WWW_BACK,      scancode & 0x80);   break;
        case 0x6b:  send_keycode(KEY_MM_MYCOMPUTER, scancode & 0x80);   break;
        case 0x6c:  send_keycode(KEY_MM_EMAIL,      scancode & 0x80);   break;
        case 0x6d:  send_keycode(KEY_MM_SELECT,     scancode & 0x80);   break;
        }
        state = STATE_NORMAL;
        break;
    case STATE_PRTSC_DOWN_2:
        if (0xe0 == scancode) {
            state = STATE_PRTSC_DOWN_3;
        } else {
            state = STATE_NORMAL;
        }
        break;
    case STATE_PRTSC_UP_2:
        if (0xe0 == scancode) {
            state = STATE_PRTSC_UP_3;
        } else {
            state = STATE_NORMAL;
        }
        break;
    case STATE_PRTSC_DOWN_3:
        if (0x37 == scancode) {
            send_keycode(KEY_PRTSC, NO);    // down
        }
        state = STATE_NORMAL;
        break;
    case STATE_PRTSC_UP_3:
        if (0xaa == scancode) {
            send_keycode(KEY_PRTSC, YES);   // up
        }
        state = STATE_NORMAL;
        break;
    case STATE_PAUSE_1:
        if (0x1d == scancode) {
            state = STATE_PAUSE_2;
        } else {
            state = STATE_NORMAL;
        }
        break;
    case STATE_PAUSE_2:
        if (0x45 == scancode) {
            state = STATE_PAUSE_3;
        } else {
            state = STATE_NORMAL;
        }
        break;
    case STATE_PAUSE_3:
        if (0xe1 == scancode) {
            state = STATE_PAUSE_4;
        } else {
            state = STATE_NORMAL;
        }
        break;
    case STATE_PAUSE_4:
        if (0x9d == scancode) {
            state = STATE_PAUSE_5;
        } else {
            state = STATE_NORMAL;
        }
        break;
    case STATE_PAUSE_5:
        if (0xc5 == scancode) {
            send_keycode(KEY_PAUSE, NO);
        }
        state = STATE_NORMAL;
        break;
    default:
        break;
    }
}

static void ps2kbd_int_handler(int vec __UNUSED, int_frame_t * sp __UNUSED) {
    while (in8(PS2KBD_CTRL_PORT) & 1) {
        u8 scancode = in8(PS2KBD_DATA_PORT);
        digest_scan_code(scancode);
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

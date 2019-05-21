#include <wheel.h>

pipe_t * kbd_pipe = NULL;   // pipe for transmitting key code
// pipe_t * tty_pipe = NULL;   // pipe for transmitting ascii

// keyboard lock content, scroll lock is omitted
static int kbd_capslock = 0;
static int kbd_numlock  = 1;

// we have to track the state of modifier keys
static int kbd_l_shift   = 0;
static int kbd_r_shift   = 0;
static int kbd_l_control = 0;
static int kbd_r_control = 0;
static int kbd_l_alt     = 0;
static int kbd_r_alt     = 0;

// notice that 0 appears first, different from keyboard layout
static const char syms[] = ")!@#$%^&*(";

// send converted ascii to pipe
static void send_ascii(char c) {
    if (NULL != tty_pipe) {
        pipe_write(tty_pipe, (u8 *) &c, 1);
    }
}

static void handle_keycode(keycode_t key, int release) {
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

static void kbd_proc() {
    dbg_print("keyboard-server started...\r\n");
    while (1) {
        u32 encoded;
        pipe_read(kbd_pipe, (u8 *) &encoded, 4);
        handle_keycode(encoded & 0x7fffffff, encoded & 0x80000000);
    }
}

__INIT void kbd_lib_init() {
    kbd_pipe = pipe_create();
    task_t * kbd_task = task_create("kbd-server", 0, kbd_proc, 0,0,0,0);
    task_resume(kbd_task);
}

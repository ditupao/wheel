#include <wheel.h>

pipe_t * kbd_pipe = NULL;

static void kbd_proc() {
    kbd_pipe = pipe_create();
    dbg_print("keyboard-server started...\r\n");
    while (1) {
        u8 buf[10];
        pipe_read(kbd_pipe, buf, 1);
        dbg_print("%c", buf[0]);
    }
}

__INIT void kbd_lib_init() {
    task_t * kbd_task = task_create(0, 0, kbd_proc, 0,0,0,0);
    task_resume(kbd_task);
}

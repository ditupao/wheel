#include <wheel.h>

// TODO: also handle raw-mode output
//       allow user programs to write char to coordinate

void tty_proc() {
    while (1) {
        char buf[1024];
        int len = pipe_read(tty_pipe, (u8 *) buf, 1023);
        buf[len] = '\0';
        dbg_print("%s", buf);
    }
}

__INIT void tty_lib_init() {
    task_t * tty = task_create("tty_server", PRIORITY_NONRT-1, tty_proc, 0,0,0,0);
    task_resume(tty);
}

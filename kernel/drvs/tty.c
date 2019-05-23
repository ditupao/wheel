#include <wheel.h>

// for pseudo terminal device, we only need to keep
// input data buffer, output is not buffered (?)
typedef struct tty_dev {
    iodev_t   dev;      // header
    int       echo;
    fifobuf_t fifo;     // input buffer
} tty_dev_t;

// some process is reading from the tty device
// return how many bytes read
static usize tty_buff_read(tty_dev_t * tty, u8 * buf, usize len) {
    return fifobuf_read(&tty->fifo, buf, len);
}

// we got user input from keyboard, put it in the current tty
static usize tty_buff_write(tty_dev_t * tty, u8 * buf, usize len) {
    usize ret = fifobuf_write(&tty->fifo, buf, len);
    ios_notify_readers((iodev_t *) tty);
    return ret;
}

fdesc_t * tty_dev_open(iodev_t * dev __UNUSED) {
    return NULL;
}

void tty_dev_close() {
    //
}

// called by read(stdin)
usize tty_dev_read(iodev_t * dev, u8 * buf, usize len) {
    return tty_buff_read((tty_dev_t *) dev, buf, len);
}

// called by write(stdout)
usize tty_dev_write(iodev_t * dev __UNUSED, const u8 * buf, usize len) {
    char * mbuf = (char *) buf;

    mbuf[len] = '\0';
    dbg_print((const char *) mbuf);
    return len;
}

//------------------------------------------------------------------------------

static iodrv_t   drv;
static tty_dev_t tty;

static int tty_dev_created = 0;

pipe_t * tty_pipe = NULL;

// TODO: create infrastructure for io driver and device creation, like
//       io_driver_regist()
//       io_device_create("/dev/tty")
//       vfs_node_regist("/dev/tty", dev)

iodev_t * tty_dev_create() {
    if (tty_dev_created) {
        return (iodev_t *) &tty;
    }

    // regist io driver
    drv.open  = tty_dev_open;
    drv.close = tty_dev_close;
    drv.read  = tty_dev_read;
    drv.write = tty_dev_write;

    // create io device
    tty.dev.drv     = &drv;
    tty.dev.readers = DLLIST_INIT;
    tty.dev.writers = DLLIST_INIT;
    tty.dev.pended  = NULL;

    // init custom fields
    tty.echo    = 1;
    fifobuf_init(&tty.fifo);

    // TODO: add this device to the global tty list, so that we can
    // track which tty is the current one. Also we can know which tty
    // can read from keyboard and print to console.

    tty_dev_created = 1;
    return (iodev_t *) &tty;
}

// this task is used to connect keyboard input with tty
// it copies data from tty_pipe, and write to tty_device
// but we can make it better
// reconnect tty_pipe to tty, so that every time ps2 keyboard
// driver writes to tty_pipe, it is actually writing to tty device
static void tty_proc() {
    static char buf[1024];
    while (1) {
        int len = pipe_read(tty_pipe, (u8 *) buf, 1023);
        if (!tty_dev_created) {
            continue;
        }

        // TODO: find current tty that is using,
        //       and route message to that tty.
        tty_buff_write(&tty, (u8 *) buf, len);

        // echo on the screen
        if (tty.echo) {
            buf[len] = '\0';
            dbg_print("%s", buf);
        }
    }
}

__INIT void tty_lib_init() {
    tty_pipe = pipe_create();
    task_t * tty = task_create("tty-server", 0, tty_proc, 0,0,0,0);
    task_resume(tty);
}

#include <wheel.h>

// pseudo-terminal device driver

int fd_read(fdesc_t * fd, char * buf, int len) {
    iodrv_t * drv = fd->dev->drv;
    // TODO: check fdesc mode, whether we have read access
    return drv->read(fd->dev, buf, len);
}

int fd_write(fdesc_t * fd, char * buf, int len) {
    iodrv_t * drv = fd->dev->drv;
    // TODO: check fdesc mode, whether we have read access
    return drv->write(fd->dev, buf, len);
}

//------------------------------------------------------------------------------

// for pseudo terminal device, we only need to keep
// input data buffer, output is not buffered (?)
typedef struct tty_dev {
    iodev_t  dev;           // header
    int      echo;
    pglist_t pages;         // input data buffer
    int      roffset;       // offset within pages.head [0, PAGE_SIZE-1]
    int      woffset;       // offset within pages.tail [0, PAGE_SIZE-1]
} tty_dev_t;

// some process is reading from the tty device
// return how many bytes read
static usize tty_buff_read(tty_dev_t * tty, u8 * buf, usize len) {
    usize total_len = len;

    while (len) {
        if (tty->pages.head == tty->pages.tail) {
            // this is the last page, make sure read_offset
            // does not exceeds write_offset
            pfn_t head = tty->pages.head;
            u8  * addr = phys_to_virt((usize) head << PAGE_SHIFT);
            usize copy = MIN(len, (usize) tty->woffset - tty->roffset);
            memcpy(buf, addr + tty->roffset, copy);
            tty->roffset += copy;
            buf          += copy;
            len          -= copy;

            // there's nothing more to read
            return total_len - len;
        }

        // roffset and woffset in different pages
        pfn_t head = tty->pages.head;
        u8  * addr = phys_to_virt((usize) head << PAGE_SHIFT);
        usize copy = MIN(len, (usize) PAGE_SIZE - tty->roffset);
        memcpy(buf, addr + tty->roffset, copy);
        tty->roffset += copy;
        buf          += copy;
        len          -= copy;

        // free head page if all content have been read
        if (PAGE_SIZE == tty->roffset) {
            head = pglist_pop_head(&tty->pages);
            page_block_free(head, 0);
            tty->roffset = 0;
        }
    }

    return total_len;
}

// we got user input from keyboard, put it in the current tty
static void tty_buff_write(tty_dev_t * tty, u8 * buf, usize len) {
    while (len) {
        // write to `pages.tail + write_offset`
        pfn_t tail = tty->pages.tail;
        u8  * addr = phys_to_virt((usize) tail << PAGE_SHIFT);
        usize copy = MIN(len, (usize) PAGE_SIZE - tty->woffset);
        memcpy(addr + tty->woffset, buf, copy);
        tty->woffset += copy;
        buf          += copy;
        len          -= copy;

        // allocate new space if current tail page is used up
        if (PAGE_SIZE == tty->woffset) {
            tail = page_block_alloc(ZONE_NORMAL|ZONE_DMA, 0);
            pglist_push_tail(&tty->pages, tail);
            page_array[tail].block = 1;
            page_array[tail].order = 0;
            page_array[tail].type  = PT_PIPE;
            tty->woffset = 0;
        }
    }
}

void tty_dev_open() {
    //
}

void tty_dev_close() {
    //
}

// called by read(stdin)
int tty_dev_read(iodev_t * dev, char * buf, int len) {
    return tty_buff_read((tty_dev_t *) dev, (u8 *) buf, len);
}

// called by write(stdout)
int tty_dev_write(iodev_t * dev __UNUSED, char * buf, int len) {
    buf[len] = '\0';
    dbg_print(buf);
    return len;
}

//------------------------------------------------------------------------------

static iodrv_t   drv;
static tty_dev_t tty;

static int tty_dev_created = 0;

pipe_t * tty_pipe = NULL;

// TODO: create infrastructure for io driver and device creation, like
//       io_driver_regist()
//       io_device_create()

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

    // init custom fields
    tty.echo    = 1;
    tty.pages   = PGLIST_INIT;
    tty.roffset = 0;
    tty.woffset = 0;

    // must have at least one page block
    pfn_t pn = page_block_alloc(ZONE_NORMAL|ZONE_DMA, 0);
    page_array[pn].block = 1;
    page_array[pn].order = 0;
    page_array[pn].type  = PT_PIPE;
    pglist_push_tail(&tty.pages, pn);

    // TODO: add this device to the global tty list, so that we can
    // track which tty is the current one. Also we can know which tty
    // can read from keyboard and print to console.

    tty_dev_created = 1;
    return (iodev_t *) &tty;
}

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

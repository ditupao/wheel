#include <wheel.h>

static pool_t iodev_pool;
static pool_t fdesc_pool;

fdesc_t * ios_open(const char * filename, int mode) {
    dbg_print("opening file %s.\n", filename);
    iodev_t * dev = tty_dev_create();
    fdesc_t * fd  = (fdesc_t *) pool_obj_alloc(&fdesc_pool);
    fd->dev       = dev;
    fd->tid       = thiscpu_var(tid_prev);
    fd->mode      = mode;
    fd->state     = 0;
    fd->dl_reader = DLNODE_INIT;
    fd->dl_writer = DLNODE_INIT;

    if (mode & IOS_READ) {
        dl_push_head(&dev->readers, &fd->dl_reader);
    }
    if (mode & IOS_WRITE) {
        dl_push_head(&dev->writers, &fd->dl_writer);
    }

    return fd;
}

void ios_close(fdesc_t * fd) {
    if (fd->mode & IOS_READ) {
        dl_remove(&fd->dev->readers, &fd->dl_reader);
    }
    if (fd->mode & IOS_WRITE) {
        dl_remove(&fd->dev->writers, &fd->dl_writer);
    }
    pool_obj_free(&fdesc_pool, fd);
}

usize ios_read(fdesc_t * fd, u8 * buf, usize len) {
    // current task is already in the readers list
    // no need to prepare waiter struct
    iodev_t * dev = fd->dev;
    iodrv_t * drv = dev->drv;

    if (0 == len) {
        return 0;
    }

    // this function cannot be called inside ISR
    if (thiscpu_var(int_depth)) {
        return -1;
    }

    while (1) {
        // TODO: check if we got any pending signals to handle
        // TODO: check if we are timeout

        // check if we can read any content
        usize ret = drv->read(fd->dev, buf, len);
        if (ret) {
            return ret;
        }

        // cannot read any information, pend current task
        task_t * self = thiscpu_var(tid_prev);
        dev->pended = self;
        sched_stop(self, TS_PEND);
        task_switch();
        dev->pended = NULL;
    }
}

usize ios_write(fdesc_t * fd, const u8 * buf, usize len) {
    // task is already in the writers list
    // no need to create waiter object on stack
    iodev_t * dev = fd->dev;
    iodrv_t * drv = dev->drv;

    if (0 == len) {
        return 0;
    }

    // this function cannot be called inside ISR
    if (thiscpu_var(int_depth)) {
        return -1;
    }

    while (1) {
        // TODO: check for signals
        // TODO: check for timeout

        // check if we can write into the file
        usize ret = drv->write(fd->dev, buf, len);
        if (ret) {
            return ret;
        }

        // cannot write even on byte, pend current task
        task_t * self = thiscpu_var(tid_prev);
        dev->pended = self;
        sched_stop(self, TS_PEND);
        task_switch();
        dev->pended = NULL;
    }
}

void ios_notify_readers(iodev_t * dev) {
    dlnode_t * node = dev->readers.head;
    while (node) {
        fdesc_t * fd  = PARENT(node, fdesc_t, dl_reader);
        task_t  * tid = fd->tid;
        int       cpu = tid->last_cpu;

        raw_spin_take(&tid->lock);
        sched_cont(tid, TS_PEND);
        raw_spin_give(&tid->lock);

        if (cpu_index() == cpu) {
            task_switch();
        } else {
            smp_reschedule(cpu);
        }

        node = node->next;
    }
}

void ios_notify_writers(iodev_t * dev) {
    dlnode_t * node = dev->writers.head;
    while (node) {
        fdesc_t * fd  = PARENT(node, fdesc_t, dl_writer);
        task_t  * tid = fd->tid;
        int       cpu = tid->last_cpu;

        raw_spin_take(&tid->lock);
        sched_cont(tid, TS_PEND);
        raw_spin_give(&tid->lock);

        if (cpu_index() == cpu) {
            task_switch();
        } else {
            smp_reschedule(cpu);
        }

        node = node->next;
    }
}

// ios_lib_init? vfs_lib_init?
__INIT void ios_lib_init() {
    pool_init(&iodev_pool, sizeof(iodev_t));
    pool_init(&fdesc_pool, sizeof(fdesc_t));
}

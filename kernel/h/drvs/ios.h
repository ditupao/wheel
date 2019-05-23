#ifndef DRVS_IOS_H
#define DRVS_IOS_H

#include <base.h>
// #include <core/task.h>
#include <libk/list.h>

typedef struct task  task_t;

typedef struct iodrv iodrv_t;
typedef struct iodev iodev_t;
typedef struct fdesc fdesc_t;

struct iodrv {
    fdesc_t * (* open)  (iodev_t * dev);
    void      (* close) (iodev_t * dev);
    usize     (* read)  (iodev_t * dev,       u8 * buf, usize len);
    usize     (* write) (iodev_t * dev, const u8 * buf, usize len);
};

struct iodev {
    iodrv_t * drv;          // type of this device
    dllist_t  readers;      // list of readers
    dllist_t  writers;      // list of writers
    task_t  * pended;       // test read file pending
};

// file descriptor, tasks' holder to an opened file
struct fdesc {
    iodev_t * dev;          // which device we've opened
    task_t  * tid;          // not NULL only if pending
    int       mode;         // open mode, RO/WO/RW
    dlnode_t  dl_reader;    // node in the readers list
    dlnode_t  dl_writer;    // node in the writers list
};

#define IOS_READ    1
#define IOS_WRITE   2

extern fdesc_t * ios_open (const char * filename, int mode);
extern fdesc_t * ios_fork (fdesc_t * fd);
extern void      ios_close(fdesc_t * fd);
extern usize     ios_read (fdesc_t * fd,       u8 * buf, usize len);
extern usize     ios_write(fdesc_t * fd, const u8 * buf, usize len);

// called by io drivers, wake up readers/writers
extern void ios_notify_readers(iodev_t * dev);
extern void ios_notify_writers(iodev_t * dev);

extern __INIT void ios_lib_init();

#endif // DRVE_IOS_H

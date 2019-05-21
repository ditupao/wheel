#ifndef DRVS_TTY_H
#define DRVS_TTY_H

#include <base.h>
#include <core/pipe.h>

typedef struct iodrv iodrv_t;
typedef struct iodev iodev_t;
typedef struct fdesc fdesc_t;

struct iodrv {
    void (* open)  (iodev_t * fd);
    void (* close) (iodev_t * fd);
    int  (* read)  (iodev_t * fd, char * buf, int len);
    int  (* write) (iodev_t * fd, char * buf, int len);
};

struct iodev {
    iodrv_t * drv;          // type of this device
    dllist_t  readers;      // list of readers
    dllist_t  writers;      // list of writers
};

// file descriptor, tasks' holder to an opened file
struct fdesc {
    iodev_t * dev;          // which device we've opened
    int       mode;         // access mode, RO/WO/RW
    dlnode_t  dl_reader;    // node in the readers list
    dlnode_t  dl_writer;    // node in the writers list
};

extern int fd_read (fdesc_t * fd, char * buf, int len);
extern int fd_write(fdesc_t * fd, char * buf, int len);

extern pipe_t * tty_pipe;   // for input

extern iodev_t * tty_dev_create();

extern __INIT void tty_lib_init();

#endif // DRVS_TTY_H

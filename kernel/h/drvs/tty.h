#ifndef DRVS_TTY_H
#define DRVS_TTY_H

#include <base.h>

typedef struct pipe  pipe_t;
typedef struct iodev iodev_t;

extern pipe_t * tty_pipe;   // for input

extern iodev_t * tty_dev_create();

extern __INIT void tty_lib_init();

#endif // DRVS_TTY_H

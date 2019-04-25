#ifndef CORE_PIPE_H
#define CORE_PIPE_H

#include <base.h>
#include <mem/page.h>
#include <core/spin.h>
#include <core/semaphore.h>

typedef struct pipe {
    spin_t      lock;
    pglist_t    pages;
    int         offset_read;    // offset within pages.head [0, PAGE_SIZE-1]
    int         offset_write;   // offset within pages.tail [0, PAGE_SIZE-1]

    // TODO: semaphore or pend queue
    semaphore_t sem;
} pipe_t;

extern pipe_t * pipe_create();
extern void     pipe_delete(pipe_t * pipe);
extern usize    pipe_read  (pipe_t * pipe, u8 * buf, usize len);
extern usize    pipe_write (pipe_t * pipe, u8 * buf, usize len);

// requires: nothing
extern __INIT void pipe_lib_init();

#endif // CORE_PIPE_H

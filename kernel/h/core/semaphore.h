#ifndef CORE_SEMAPHORE_H
#define CORE_SEMAPHORE_H

#include <base.h>
#include "spin.h"
#include <libk/list.h>

typedef struct semaphore {
    spin_t   lock;
    int      limit;
    int      count;
    dllist_t pend_q;
} semaphore_t;

extern void semaphore_init   (semaphore_t * sem, int limit, int count);
extern int  semaphore_take   (semaphore_t * sem, int timeout);
extern int  semaphore_trytake(semaphore_t * sem);
extern void semaphore_give   (semaphore_t * sem);

#endif // CORE_SEMAPHORE_H

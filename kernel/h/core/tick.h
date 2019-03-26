#ifndef CORE_TICK_H
#define CORE_TICK_H

#include <base.h>
#include <libk/list.h>

typedef int (* wdog_proc_t) (void * a1, void * a2, void * a3, void * a4);

typedef struct wdog {
    dlnode_t    node;
    usize       ticks;
    wdog_proc_t proc;
    void *      arg1;
    void *      arg2;
    void *      arg3;
    void *      arg4;
} wdog_t;

extern void wdog_init   (wdog_t * wd);
extern void wdog_destroy(wdog_t * wd);
extern void wdog_start  (wdog_t * wd, int ticks, void * proc,
                         void * a1, void * a2, void * a3, void * a4);
extern void wdog_cancel (wdog_t * wd);

extern __INIT void tick_lib_init();

#endif // CORE_TICK_H

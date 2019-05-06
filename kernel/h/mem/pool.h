#ifndef MEM_POOL_H
#define MEM_POOL_H

#include <base.h>
#include <mem/page.h>
#include <libk/spin.h>

typedef struct pool {
    spin_t   lock;
    u32      obj_size;
    pglist_t full;          // all objects not allocated
    pglist_t empty;         // all objects allocated
    pglist_t partial;       // some objects allocated
} pool_t;

extern void pool_init   (pool_t * pool, u32 obj_size);
extern void pool_destroy(pool_t * pool);
extern void pool_shrink (pool_t * pool);

extern void * pool_obj_alloc(pool_t * pool);
extern void   pool_obj_free (pool_t * pool, void * obj);

#endif // MEM_POOL_H

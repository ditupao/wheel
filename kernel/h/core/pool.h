#ifndef CORE_POOL_H
#define CORE_POOL_H

#include <base.h>
#include <core/spin.h>

typedef struct slab_list {
    pfn_t head;
    pfn_t tail;
} slab_list_t;

typedef struct pool {
    spin_t      lock;
    u32         obj_size;
    slab_list_t full;       // all objects not allocated
    slab_list_t partial;    // some objects allocated
    slab_list_t empty;      // all objects allocated
} pool_t;

extern void pool_init   (pool_t * pool, u32 obj_size);
extern void pool_destroy(pool_t * pool);
extern void pool_shrink (pool_t * pool);

extern void * pool_obj_alloc(pool_t * pool);
extern void   pool_obj_free (pool_t * pool, void * obj);

#endif // CORE_POOL_H

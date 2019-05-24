#include <wheel.h>

// in the pool there are 3 slab lists:
// - full list,     inuse = 0
// - partial list,  inuse = ~
// - empty list,    inuse = MAX
// among them, full and empty lists are stacks, partial list is ordered
// partial slabs are sotred by inuse
// slabs with larger inuse is stored to the tail

#define NO_OBJ ((u32) -1)

void pool_init(pool_t * pool, u32 obj_size) {
    if (obj_size < 4) {
        obj_size = 4;
    }

    if (obj_size > (PAGE_SIZE / 8)) {
        dbg_print("[panic] pool obj-size too large!\n");
        return;
    }

    obj_size = ROUND_UP(obj_size, 8);

    // initialize member variables
    pool->lock     = SPIN_INIT;
    pool->obj_size = obj_size;
    pool->full     = PGLIST_INIT;
    pool->partial  = PGLIST_INIT;
    pool->empty    = PGLIST_INIT;
}

void pool_destroy(pool_t * pool) {
    pglist_free_all(&pool->full);
    pglist_free_all(&pool->partial);
    pglist_free_all(&pool->empty);
}

void pool_shrink(pool_t * pool) {
    pglist_free_all(&pool->full);
}

// allocate an object from the pool
// first try to get a free obj from partial, then full, if still
// not available, then we create a new slab by requesting a new page.
void * pool_obj_alloc(pool_t * pool) {
    pfn_t slab = NO_PAGE;
    u8 *  va   = NULL;

    if ((NO_PAGE == pool->partial.head) &&
        (NO_PAGE == pool->partial.tail)) {
        if ((NO_PAGE == pool->full.head) &&
            (NO_PAGE == pool->full.tail)) {
            // partial list and full list both empty, request a new page
            slab = page_block_alloc(ZONE_DMA | ZONE_NORMAL, 0);
            page_array[slab].type    = PT_POOL;
            page_array[slab].inuse   = 0;
            page_array[slab].objects = 0;
            page_array[slab].block   = 1;
            page_array[slab].order   = 0;

            // construct freelist
            va = (u8 *) phys_to_virt((usize) slab << PAGE_SHIFT);
            int obj_size  = pool->obj_size;
            int obj_count = PAGE_SIZE / obj_size;
            for (int i = 0; i < obj_count; ++i) {
                * (u32 *) (va + i * obj_size) = (i + 1) * obj_size;
            }
            * (u32 *) (va + (obj_count - 1) * obj_size) = NO_OBJ;
        } else {
            // partial list empty, full list not empty
            // take a page out from the full list
            slab = pglist_pop_head(&pool->full);
        }

        // push this page into the partial list
        pglist_push_head(&pool->partial, slab);
    }

    // get the tail of partial list, but do not remove it from the list
    slab = pool->partial.tail;
    va   = (u8 *) phys_to_virt((usize) slab << PAGE_SHIFT);

    // get the object in the freelist
    u32 obj  = page_array[slab].objects;
    u32 next = * (u32 *) (va + obj);
    page_array[slab].objects = next;
    page_array[slab].inuse  += 1;

    if (next == NO_OBJ) {
        // all objects allocated, move this slab to empty list
        pglist_remove(&pool->partial, slab);
        pglist_push_head(&pool->empty, slab);
    }

    return (void *) (va + obj);
}

// return an object to the pool, add to corresponding slab
void pool_obj_free(pool_t * pool, void * obj) {
    pfn_t slab = (pfn_t) (virt_to_phys(obj) >> PAGE_SHIFT);

    dbg_assert(PT_POOL == page_array[slab].type);
    dbg_assert(((usize) obj & (PAGE_SIZE - 1)) % pool->obj_size == 0);

    // add object to the freelist, and substract 1 from inuse
    * (u32 *) obj = page_array[slab].objects;
    page_array[slab].objects = (u16) ((usize) obj & (PAGE_SIZE - 1));
    page_array[slab].inuse  -= 1;

    if (NO_OBJ == * (u32 *) obj) {
        // slab state change from empty to partial, move into partial list
        pglist_remove(&pool->empty, slab);
        pglist_push_tail(&pool->partial, slab);
    } else if (page_array[slab].inuse == 0) {
        // slab state change from partial to full, move into full list
        pglist_remove(&pool->partial, slab);
        pglist_push_head(&pool->full, slab);
    } else {
        // slab state didn't change, but `inuse` became smaller,
        // so we may have to move this slab forward.
        pfn_t prev = page_array[slab].prev;
        while ((NO_PAGE != prev) && (page_array[slab].inuse < page_array[prev].inuse)) {
            prev = page_array[prev].prev;
        }

        if (prev != page_array[slab].prev) {
            // `prev` is not the direct predecessor, we can infer that:
            // 1. partial is not empty, at least two items
            // 2. `slab` is not the head of list
            // 3. `prev` is not the tail of list

            // first take `slab` out from the list
            pglist_remove(&pool->partial, slab);

            // then insert `slab` after `prev`
            if (NO_PAGE == prev) {
                pglist_push_head(&pool->partial, slab);
            } else {
                pfn_t next = page_array[prev].next;
                page_array[slab].prev = prev;
                page_array[slab].next = next;
                page_array[prev].next = slab;
                if (NO_PAGE != next) {
                    page_array[next].prev = slab;
                } else {
                    pool->partial.tail = slab;
                }
            }
        }
    }
}

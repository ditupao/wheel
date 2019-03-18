#include <wheel.h>

#define NO_OBJ ((u32) -1)

void pool_init(pool_t * pool, u32 obj_size) {
    if (obj_size < 4) {
        obj_size = 4;
    }

    if (obj_size > (PAGE_SIZE / 8)) {
        dbg_print("pool obj-size too large!\r\n");
        return;
    }

    // TODO: define L1 cache-line size in configuration
    if (obj_size < 64) {
        obj_size = 1UL << (32 - CLZ32(obj_size - 1));
    } else {
        obj_size = ROUND_UP(obj_size, 64); 
    }

    // initialize member variables
    pool->lock         = SPIN_INIT;
    pool->obj_size     = obj_size;
    pool->full.head    = NO_PAGE;
    pool->full.tail    = NO_PAGE;
    pool->partial.head = NO_PAGE;
    pool->partial.tail = NO_PAGE;
    pool->empty.head   = NO_PAGE;
    pool->empty.tail   = NO_PAGE;
}

void pool_destroy(pool_t * pool) {
    pfn_t slab = pool->full.head;
    while (NO_PAGE != slab) {
        // page_block_free will change page_array, so get next item first
        pfn_t next = page_array[slab].next;
        page_block_free(slab, 0);
        slab = next;
    }
    pool->full.head = NO_PAGE;
    pool->full.tail = NO_PAGE;

    slab = pool->partial.head;
    while (NO_PAGE != slab) {
        pfn_t next = page_array[slab].next;
        page_block_free(slab, 0);
        slab = next;
    }
    pool->partial.head = NO_PAGE;
    pool->partial.tail = NO_PAGE;

    slab = pool->empty.head;
    while (NO_PAGE != slab) {
        pfn_t next = page_array[slab].next;
        page_block_free(slab, 0);
        slab = next;
    }
    pool->empty.head = NO_PAGE;
    pool->empty.tail = NO_PAGE;
}

void pool_shrink(pool_t * pool) {
    pfn_t slab = pool->full.head;
    while (NO_PAGE != slab) {
        pfn_t next = page_array[slab].next;
        page_block_free(slab, 0);
        slab = next;
    }
    pool->full.head = NO_PAGE;
    pool->full.tail = NO_PAGE;
}

// allocate an object from the pool
// first try to get a free obj from partial, then full, if still
// not available, then we create a new slab by requesting a new page.
void * pool_obj_alloc(pool_t * pool) {
    pfn_t slab = NO_PAGE;
    u8 *  va   = NULL;

    // first check if the partial list is empty
    if ((NO_PAGE == pool->partial.head) &&
        (NO_PAGE == pool->partial.tail)) {
        // no partial slabs available, check full slabs next
        if ((NO_PAGE == pool->full.head) && (NO_PAGE == pool->full.tail)) {
            // full list is also empty, request a new page
            slab = page_block_alloc(ZONE_DMA | ZONE_NORMAL, 0);
            page_array[slab].type    = PT_POOL;
            page_array[slab].inuse   = 0;
            page_array[slab].objects = 0;

            // construct freelist
            va = (u8 *) phys_to_virt((usize) slab << PAGE_SHIFT);
            int obj_size  = pool->obj_size;
            int obj_count = PAGE_SIZE / obj_size;
            for (int i = 0; i < obj_count; ++i) {
                * (u32 *) (va + i * obj_size) = (i + 1) * obj_size;
            }
            * (u32 *) (va + (obj_count - 1) * obj_size) = NO_OBJ;
        } else {
            // take a node from the list head
            pfn_t head = pool->full.head;
            pfn_t next = page_array[head].next;
            pool->full.head = next;
            if (NO_PAGE == next) {
                pool->full.tail = NO_PAGE;
            } else {
                page_array[next].prev = NO_PAGE;
            }
            slab = head;
        }

        // push this page into the partial list
        page_array[slab].prev    = NO_PAGE;
        page_array[slab].next    = NO_PAGE;
        pool->partial.head = NO_PAGE;
        pool->partial.tail = NO_PAGE;
    }

    // get the tail of partial list, but not remove from the list
    slab = pool->partial.tail;
    va = (u8 *) phys_to_virt((usize) slab << PAGE_SHIFT);

    // get the object in the freelist
    u32 obj  = page_array[slab].objects;
    u32 next = * (u32 *) (va + obj);
    page_array[slab].objects = next;
    page_array[slab].inuse += 1;

    if (next == NO_OBJ) {
        // all objects allocated, remove from partial slab list
        pfn_t prev = page_array[slab].prev;
        pool->partial.tail = prev;
        if (NO_PAGE == prev) {
            pool->partial.head = NO_PAGE;
        } else {
            page_array[prev].next = NO_PAGE;
        }

        // and push into the empty slab list
        pfn_t head = pool->empty.head;
        page_array[slab].prev = NO_PAGE;
        page_array[slab].next = head;
        pool->empty.head = slab;
        if (NO_PAGE == head) {
            pool->empty.tail = slab;
        } else {
            page_array[head].prev = slab;
        }
    }

    return (void *) (va + obj);
}

// return an object to the pool, add to corresponding slab
void pool_obj_free(pool_t * pool, void * obj) {
    u8 *  va   = (u8 *) ((usize) obj & ~(PAGE_SIZE - 1));
    usize pa   = virt_to_phys(va);
    pfn_t slab = (pfn_t) (pa >> PAGE_SHIFT);

    dbg_assert(PT_POOL == page_array[slab].type);
    dbg_assert(((usize) obj % (pool->obj_size)) == 0);

    // add object to the freelist, and substract 1 from inuse
    * (u32 *) obj = page_array[slab].objects;
    page_array[slab].objects = (u32) ((u8 *) obj - va);
    page_array[slab].inuse -= 1;

    if (NO_OBJ == * (u32 *) obj) {
        // slab state change from empty to partial
        pfn_t prev = page_array[slab].prev;
        pfn_t next = page_array[slab].next;
        if (NO_PAGE == prev) {
            pool->empty.head = next;
        } else {
            page_array[prev].next = next;
        }
        if (NO_PAGE == next) {
            pool->empty.tail = prev;
        } else {
            page_array[next].prev = prev;
        }

        // push to the tail of partial slabs
        pfn_t tail = pool->partial.tail;
        page_array[slab].prev = tail;
        page_array[slab].next = NO_PAGE;
        pool->partial.tail = slab;
        if (NO_PAGE == tail) {
            pool->partial.head = slab;
        } else {
            page_array[tail].next = slab;
        }
    } else if (page_array[slab].inuse == 0) {
        // slab turn from partial to full
        pfn_t prev = page_array[slab].prev;
        pfn_t next = page_array[slab].next;
        if (NO_PAGE == prev) {
            pool->partial.head = next;
        } else {
            page_array[prev].next = next;
        }
        if (NO_PAGE == next) {
            pool->partial.tail = prev;
        } else {
            page_array[next].prev = prev;
        }

        // insert into full slabs, order does not matter
        pfn_t head = pool->full.head;
        page_array[slab].prev = NO_PAGE;
        page_array[slab].next = head;
        pool->full.head = slab;
        if (NO_PAGE == head) {
            pool->full.tail = slab;
        } else {
            page_array[head].prev = slab;
        }
    } else {
        // slab state didn't change, but `inuse` became smaller, so we may
        // have to move slab forward.
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
            pfn_t p = page_array[slab].prev;
            pfn_t n = page_array[slab].next;
            dbg_assert(NO_PAGE != p);
            page_array[p].next = n;
            if (NO_PAGE == n) {
                pool->partial.tail = p;
            } else {
                page_array[n].prev = p;
            }

            // then insert `slab` after `prev`
            if (NO_PAGE == prev) {
                n = pool->partial.head;
                pool->partial.head = slab;
            } else {
                n = page_array[prev].next;
                page_array[prev].next = slab;
            }
            page_array[n].prev    = slab;
            page_array[slab].prev = prev;
            page_array[slab].next = n;
        }
    }
}

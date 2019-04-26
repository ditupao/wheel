#include <wheel.h>

page_t * page_array = NULL;     // start of page descriptors
usize    page_count = 0;        // number of page descriptors

typedef struct zone {
    spin_t   lock;
    usize    page_count;
    pglist_t list[ORDER_COUNT]; // block list of each order
} zone_t;

static zone_t zone_dma;
static zone_t zone_normal;
// static zone_t zone_highmem;

//------------------------------------------------------------------------------
// allocate a block from zone, no spinlock protection

static pfn_t zone_block_alloc(zone_t * zone, int order) {
    for (int o = order; o < ORDER_COUNT; ++o) {
        if ((NO_PAGE == zone->list[o].head) &&
            (NO_PAGE == zone->list[o].tail)) {
            // this order has no free block, try bigger ones
            continue;
        }

        // found an order with free block, remove its first element
        pfn_t blk = pglist_pop_head(&zone->list[o]);

        // split the block, and return the second half back
        // return second half, so base address remain unchanged
        for (; o > order; --o) {
            usize size = 1UL << (o - 1);
            pfn_t bud  = blk ^ size;
            page_array[bud].block = 1;
            page_array[bud].order = o - 1;

            // return buddy block to the list
            pglist_push_head(&zone->list[o-1], bud);
        }

        // mark this block as allocated
        usize size = 1U << order;
        zone->page_count -= size;
        for (pfn_t i = 0; i < size; ++i) {
            page_array[blk + i].type = PT_KERNEL;
        }

        page_array[blk].block = 1;
        page_array[blk].order = 1;
        return blk;
    }

    // no block that big
    return NO_PAGE;
}

static void zone_block_free(zone_t * zone, pfn_t blk, u32 order) {
    // mark this block as free
    usize size = 1U << order;
    for (pfn_t i = 0; i < size; ++i) {
        page_array[blk + i].type  = PT_FREE;
        page_array[blk + i].block = 0;
    }

    // merging into bigger block
    for (; order < ORDER_COUNT - 1; ++order) {
        size = 1U << order;
        pfn_t bud = blk ^ size;
        if ((PT_FREE != page_array[bud].type)  ||
            (1       != page_array[bud].block) ||
            (order   != page_array[bud].order)) {
            // cannot merge with buddy
            break;
        }

        // remove buddy from block list
        pglist_remove(&zone->list[order], bud);

        // merge with its buddy
        page_array[bud].block = 0;
        blk &= bud;
    }

    size = 1U << order;
    zone->page_count += size;
    page_array[blk].block = 1;
    page_array[blk].order = order;

    // put this block into the block list
    pglist_push_head(&zone->list[order], blk);
}

//------------------------------------------------------------------------------
// page frame allocator public routines

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
static inline zone_t * zone_for(usize start, usize end) {
    if ((DMA_START <= start) && (end <= DMA_END)) {
        return &zone_dma;
    }
    if ((NORMAL_START <= start) && (end <= NORMAL_END)) {
        return &zone_normal;
    }
    // if ((HIGHMEM_START <= start) && (end <= HIGHMEM_END)) {
    //     return &zone_highmem;
    // }
    return NULL;
}
#pragma GCC diagnostic pop

// allocate page block of size 2^order
// lock zone and interrupt, so ISR could alloc pages
pfn_t page_block_alloc(u32 zones, int order) {
    if ((order < 0) || (order >= ORDER_COUNT)) {
        return NO_PAGE; // invalid parameter
    }

    // if (zones & ZONE_HIGHMEM) {
    //     u32 key = irq_spin_take(&zone_highmem.lock);
    //     pfn_t blk = zone_block_alloc(&zone_highmem, order);
    //     irq_spin_give(&zone_highmem.lock, key);
    //     dbg_assert((blk & ((1U << order) - 1)) == 0);
    //     if (NO_PAGE != blk) {
    //         return blk;
    //     }
    // }

    if (zones & ZONE_NORMAL) {
        u32 key = irq_spin_take(&zone_normal.lock);
        pfn_t blk = zone_block_alloc(&zone_normal, order);
        irq_spin_give(&zone_normal.lock, key);
        dbg_assert((blk & ((1U << order) - 1)) == 0);
        if (NO_PAGE != blk) {
            return blk;
        }
    }

    if (zones & ZONE_DMA) {
        u32 key = irq_spin_take(&zone_dma.lock);
        pfn_t blk = zone_block_alloc(&zone_dma, order);
        irq_spin_give(&zone_dma.lock, key);
        dbg_assert((blk & ((1U << order) - 1)) == 0);
        if (NO_PAGE != blk) {
            return blk;
        }
    }

    // no block is large enough
    return NO_PAGE;
}

void page_block_free(pfn_t blk, int order) {
    usize size = 1UL << order;
    zone_t * zone = zone_for((usize)  blk         << PAGE_SHIFT,
                             (usize) (blk + size) << PAGE_SHIFT);
    dbg_assert(order >= 0);
    dbg_assert(order < ORDER_COUNT);
    if (0 != (blk & (size - 1))) {
        dbg_print("failed at page_free.\r\n");
        dbg_print("%x, %d.\r\n", blk, order);
    }
    dbg_assert(0 == (blk & (size - 1)));
    dbg_assert(NULL != zone);

    raw_spin_take(&zone->lock);
    zone_block_free(zone, blk, order);
    raw_spin_give(&zone->lock);
}

pfn_t page_range_alloc(u32 zones, int count) {
    // allocate a block that is large enough, then return the exceeding part
    int order = 32 - CLZ32(count - 1);
    pfn_t rng = page_block_alloc(zones, order);
    page_range_free(rng + count, (1U << order) - count);
    return rng;
}

void page_range_free(pfn_t rng, int count) {
    pfn_t from = rng;
    pfn_t to   = rng + count;

    while (from < to) {
        // compute best order for `from`
        int order = CTZ32(from);
        if ((order >= ORDER_COUNT) || (from == 0)) {
            order = ORDER_COUNT - 1;
        }
        while ((from + (1UL << order)) > to) {
            --order;
        }

        // return this block
        page_block_free(from, order);
        from += (1UL << order);
    }
}

pfn_t page_alloc(u32 zones) {
    return page_block_alloc(zones, 0);
}

void page_free(pfn_t page) {
    page_block_free(page, 0);
}

usize free_page_count(u32 zones) {
    usize count = 0;
    if (zones & ZONE_DMA) {
        count += zone_dma.page_count;
    }
    if (zones & ZONE_NORMAL) {
        count += zone_normal.page_count;
    }
    // if (zones & ZONE_HIGHMEM) {
    //     count += zone_highmem.page_count;
    // }
    return count;
}

static void dump_layout(zone_t * zone) {
    for (u32 o = 0; o < ORDER_COUNT; ++o) {
        pfn_t blk = zone->list[o].head;
        if (NO_PAGE == blk) {
            continue;
        }
        dbg_print("-- order %02u:", o);
        while (NO_PAGE != blk) {
            dbg_print(" 0x%x", blk);
            blk = page_array[blk].next;
        }
        dbg_print(".\r\n");
    }
}

void dump_page_layout(u32 zones) {
    if (zones & ZONE_DMA) {
        dbg_print("== zone dma:\r\n");
        dump_layout(&zone_dma);
    }
    if (zones & ZONE_NORMAL) {
        dbg_print("== zone normal:\r\n");
        dump_layout(&zone_normal);
    }
    // if (zones & ZONE_HIGHMEM) {
    //     dbg_print("== zone highmem:\r\n");
    //     dump_layout(&zone_highmem);
    // }
}

//------------------------------------------------------------------------------
// page list operations

void pglist_push_head(pglist_t * list, pfn_t page) {
    page_array[page].prev = NO_PAGE;
    page_array[page].next = list->head;
    if (NO_PAGE != list->head) {
        page_array[list->head].prev = page;
    } else {
        list->tail = page;
    }
    list->head = page;
}

void pglist_push_tail(pglist_t * list, pfn_t page) {
    page_array[page].prev = list->tail;
    page_array[page].next = NO_PAGE;
    if (NO_PAGE != list->tail) {
        page_array[list->tail].next = page;
    } else {
        list->head = page;
    }
    list->tail = page;
}

pfn_t pglist_pop_head(pglist_t * list) {
    pfn_t head = list->head;
    if (NO_PAGE != head) {
        list->head = page_array[head].next;
    }
    if (NO_PAGE == list->head) {
        list->tail = NO_PAGE;
    }
    return head;
}

pfn_t pglist_pop_tail(pglist_t * list) {
    pfn_t tail = list->tail;
    if (NO_PAGE != tail) {
        list->tail = page_array[tail].prev;
    }
    if (NO_PAGE == list->tail) {
        list->head = NO_PAGE;
    }
    return tail;
}

void pglist_remove(pglist_t * list, pfn_t page) {
    pfn_t prev = page_array[page].prev;
    pfn_t next = page_array[page].next;
    if (NO_PAGE != prev) {
        page_array[prev].next = next;
    } else {
        dbg_assert(list->head == page);
        list->head = next;
    }
    if (NO_PAGE != next) {
        page_array[next].prev = prev;
    } else {
        dbg_assert(list->tail == page);
        list->tail = prev;
    }
}

void pglist_free_all(pglist_t * list) {
    while (NO_PAGE != list->head) {
        dbg_assert(1 == page_array[list->head].block);
        pfn_t next = page_array[list->head].next;
        page_block_free(list->head, page_array[list->head].order);
        list->head = next;
    }
    list->tail = NO_PAGE;
}

//------------------------------------------------------------------------------
// initialize page frame allocator, initially no free page

__INIT void page_lib_init() {
    zone_dma.lock     = SPIN_INIT;
    zone_normal.lock  = SPIN_INIT;
    // zone_highmem.lock = SPIN_INIT;
    zone_dma.page_count     = 0;
    zone_normal.page_count  = 0;
    // zone_highmem.page_count = 0;
    for (int i = 0; i < ORDER_COUNT; ++i) {
        zone_dma.list[i].head     = NO_PAGE;
        zone_dma.list[i].tail     = NO_PAGE;
        zone_normal.list[i].head  = NO_PAGE;
        zone_normal.list[i].tail  = NO_PAGE;
        // zone_highmem.list[i].head = NO_PAGE;
        // zone_highmem.list[i].tail = NO_PAGE;
    }
}

// add a range of free memory
__INIT void page_range_add(usize start, usize end) {
    pfn_t from = (pfn_t) (start >> PAGE_SHIFT);
    pfn_t to   = (pfn_t) (end   >> PAGE_SHIFT);
    page_range_free(from, to - from);
}

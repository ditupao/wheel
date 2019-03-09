#include <wheel.h>

page_t * page_array = NULL;     // start of page descriptors
usize    page_count = 0;        // number of page descriptors

typedef struct level {
    pfn_t head;
    pfn_t tail;
} level_t;

typedef struct zone {
    spin_t  lock;
    usize   page_count;
    level_t list[ORDER_COUNT];  // block list of each order
} zone_t;

static zone_t zone_dma;
static zone_t zone_normal;
static zone_t zone_highmem;

//------------------------------------------------------------------------------
// allocate a block from zone, no spinlock protection

static pfn_t zone_block_alloc(zone_t * zone, u32 order) {
    for (u32 o = order; o < ORDER_COUNT; ++o) {
        if ((NO_PAGE == zone->list[o].head) &&
            (NO_PAGE == zone->list[o].tail)) {
            // this order has no free block, try bigger ones
            continue;
        }

        // found an order with free block, remove its first element
        pfn_t blk  = zone->list[o].head;
        pfn_t next = page_array[blk].next;
        zone->list[o].head = next;
        if (NO_PAGE == next) {
            zone->list[o].tail = NO_PAGE;
        } else {
            page_array[next].prev = NO_PAGE;
        }

        // split the block, and return the second half back
        // return second half, so base address remain unchanged
        for (o -= 1; o > order; --o) {
            usize size = 1U << o;
            pfn_t bud  = blk ^ size;
            page_array[bud].block = 1;
            page_array[bud].order = o;

            // return buddy
            pfn_t head = zone->list[o].head;
            page_array[bud].prev = NO_PAGE;
            page_array[bud].next = head;
            zone->list[o].head   = bud;
            if (NO_PAGE == head) {
                zone->list[o].tail = bud;
            } else {
                page_array[head].prev = bud;
            }
        }

        // mark this block as allocated
        usize size = 1U << order;
        zone->page_count -= size;
        for (pfn_t i = 0; i < size; ++i) {
            page_array[blk + i].type = PT_KERNEL;
        }
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
        pfn_t prev = page_array[bud].prev;
        pfn_t next = page_array[bud].next;
        if (NO_PAGE == prev) {
            zone->list[order].head = next;
        } else {
            page_array[prev].next = next;
        }
        if (NO_PAGE == next) {
            zone->list[order].tail = prev;
        } else {
            page_array[next].prev = prev;
        }

        // merge with its buddy
        page_array[bud].block = 0;
        blk &= bud;
    }

    size = 1U << order;
    zone->page_count += size;
    page_array[blk].block = 1;
    page_array[blk].order = order;

    // put this block into the block list
    pfn_t head = zone->list[order].head;
    page_array[blk].prev   = NO_PAGE;
    page_array[blk].next   = head;
    zone->list[order].head = blk;
    if (NO_PAGE == head) {
        zone->list[order].tail = blk;
    } else {
        page_array[head].prev = blk;
    }
}

//------------------------------------------------------------------------------
// public interface

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
static inline zone_t * zone_for(usize start, usize end) {
    if ((DMA_START <= start) && (end <= DMA_END)) {
        return &zone_dma;
    }
    if ((NORMAL_START <= start) && (end <= NORMAL_END)) {
        return &zone_normal;
    }
    if ((HIGHMEM_START <= start) && (end <= HIGHMEM_END)) {
        return &zone_highmem;
    }
    return NULL;
}
#pragma GCC diagnostic pop

// allocate page block of size 2^order
pfn_t page_block_alloc(u32 zones, u32 order) {
    if (order >= ORDER_COUNT) {
        return NO_PAGE; // invalid parameter
    }

    if (zones & ZONE_HIGHMEM) {
        raw_spin_take(&zone_highmem.lock);
        pfn_t blk = zone_block_alloc(&zone_highmem, order);
        raw_spin_give(&zone_highmem.lock);
        dbg_assert((blk & ((1U << order) - 1)) == 0);
        if (NO_PAGE != blk) {
            return blk;
        }
    }

    if (zones & ZONE_NORMAL) {
        raw_spin_take(&zone_normal.lock);
        pfn_t blk = zone_block_alloc(&zone_normal, order);
        raw_spin_give(&zone_normal.lock);
        dbg_assert((blk & ((1U << order) - 1)) == 0);
        if (NO_PAGE != blk) {
            return blk;
        }
    }

    if (zones & ZONE_DMA) {
        raw_spin_take(&zone_dma.lock);
        pfn_t blk = zone_block_alloc(&zone_dma, order);
        raw_spin_give(&zone_dma.lock);
        dbg_assert((blk & ((1U << order) - 1)) == 0);
        if (NO_PAGE != blk) {
            return blk;
        }
    }

    // no block is large enough
    return NO_PAGE;
}

void page_block_free(pfn_t blk, u32 order) {
    usize size = 1U << order;
    zone_t * zone = zone_for((usize)  blk         << PAGE_SHIFT,
                             (usize) (blk + size) << PAGE_SHIFT);
    dbg_assert(order < ORDER_COUNT);
    dbg_assert(0 == (blk & (size - 1)));
    dbg_assert(NULL != zone);

    raw_spin_take(&zone->lock);
    zone_block_free(zone, blk, order);
    raw_spin_give(&zone->lock);
}

pfn_t page_range_alloc(u32 zones, u32 count) {
    // allocate a block that is large enough, then return the exceeding part
    int order = 32 - CLZ32(count - 1);
    pfn_t rng = page_block_alloc(zones, order);
    page_range_free(rng + count, (1U << order) - count);
    return rng;
}

void page_range_free(pfn_t rng, u32 count) {
    pfn_t from = rng;
    pfn_t to   = rng + count;

    while (from < to) {
        // compute best order for `from`
        u32 order = CTZ32(from);
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

usize free_page_count(u32 zones) {
    usize count = 0;
    if (zones & ZONE_DMA) {
        count += zone_dma.page_count;
    }
    if (zones & ZONE_NORMAL) {
        count += zone_normal.page_count;
    }
    if (zones & ZONE_HIGHMEM) {
        count += zone_highmem.page_count;
    }
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
    if (zones & ZONE_HIGHMEM) {
        dbg_print("== zone highmem:\r\n");
        dump_layout(&zone_highmem);
    }
}

//------------------------------------------------------------------------------
// initialize page frame allocator, initially no free page

__INIT void page_lib_init() {
    zone_dma.lock     = SPIN_INIT;
    zone_normal.lock  = SPIN_INIT;
    zone_highmem.lock = SPIN_INIT;
    zone_dma.page_count     = 0;
    zone_normal.page_count  = 0;
    zone_highmem.page_count = 0;
    for (int i = 0; i < ORDER_COUNT; ++i) {
        zone_dma.list[i].head     = NO_PAGE;
        zone_dma.list[i].tail     = NO_PAGE;
        zone_normal.list[i].head  = NO_PAGE;
        zone_normal.list[i].tail  = NO_PAGE;
        zone_highmem.list[i].head = NO_PAGE;
        zone_highmem.list[i].tail = NO_PAGE;
    }
}

// add a range of free memory
__INIT void page_range_add(usize start, usize end) {
    pfn_t from = (pfn_t) (start >> PAGE_SHIFT);
    pfn_t to   = (pfn_t) (end   >> PAGE_SHIFT);
    page_range_free(from, to - from);
}

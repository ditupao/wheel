#include <wheel.h>

page_t * page_array = NULL;     // start of page descriptors
usize    page_count = 0;        // number of page descriptors

typedef struct zone {
    spin_t  lock;
    usize   page_count;
    pfn_t   list[ORDER_COUNT];   // head block of each order
} zone_t;

static zone_t zone_dma;
static zone_t zone_normal;
static zone_t zone_highmem;

//------------------------------------------------------------------------------
// allocate a block from zone, no spinlock protection

static pfn_t zone_block_alloc(zone_t * zone, u8 order) {
    for (u8 o = order; o < ORDER_COUNT; ++o) {
        if (NO_PAGE == zone->list[o]) {
            // this order has no free block, try bigger ones
            continue;
        }

        // found an order with free block, remove first element
        pfn_t blk = zone->list[o];
        zone->list[o] = page_array[blk].next;

        // split the block, and return the second half back
        for (; o > order; --o) {
            //
        }
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
        zone_dma.list[i]     = NO_PAGE;
        zone_normal.list[i]  = NO_PAGE;
        zone_highmem.list[i] = NO_PAGE;
    }
}

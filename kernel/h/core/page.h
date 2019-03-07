#ifndef CORE_PAGE_H
#define CORE_PAGE_H

#include <base.h>
#include <arch.h>

// page descriptor, one for each physical page frame
// we cram multiple fields into flags, making page_t more compact
typedef struct page {
    pfn_t prev;
    pfn_t next;
    u32   type : 4;
    union {
        struct {                // free
            u32 order : 4;      // only valid when block=1
            u32 block : 1;      // is it the first page in block
        };
        struct {                // pool
            u16 objects;        // first free object
            u16 inuse;          // number of allocated objects
        };
    };
} page_t;

// page types
#define PT_INVALID      0
#define PT_FREE         1
#define PT_CACHED       2
#define PT_KERNEL       3

// block order
#define ORDER_COUNT     16

// memory zone bit masks
#define ZONE_DMA        1
#define ZONE_NORMAL     2
#define ZONE_HIGHMEM    4

extern page_t * page_array;
extern usize    page_count;

extern pfn_t page_block_alloc(u32 zones, u32 order);
extern void  page_block_free (pfn_t blk, u32 order);
extern pfn_t page_range_alloc(u32 zones, u32 count);
extern void  page_range_free (pfn_t rng, u32 count);
extern usize free_page_count (u32 zones);
extern void  dump_page_layout(u32 zones);

extern __INIT void page_lib_init ();
extern __INIT void page_range_add(usize start, usize end);

#endif // CORE_PAGE_H

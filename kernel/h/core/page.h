#ifndef CORE_PAGE_H
#define CORE_PAGE_H

#include <base.h>
#include <arch.h>

// page descriptor, one for each physical page frame
// we cram multiple fields into flags, making page_t more compact
typedef struct page {
    pfn_t next;
    u32   type : 4;
    union {
        struct {    // free
            u32 order : 4;
            u32 block : 1;
        };
        struct {    // pool
            u16 objects;
            u16 inuse;
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

#endif // CORE_PAGE_H

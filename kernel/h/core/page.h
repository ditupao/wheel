#ifndef CORE_PAGE_H
#define CORE_PAGE_H

#include <base.h>
#include <arch.h>

// page descriptor, one for each physical page frame
typedef struct page {
    pfn_t prev;
    pfn_t next;
    u8    type;             // page type
    u8    order;            // the order of this block
    union {
        struct {            // slab
#if PAGE_SHIFT > 16
    #error "PAGE_SHIFT is larger than 16"
#endif
            u16 inuse   : PAGE_SHIFT;
            u16 objects : PAGE_SHIFT;
        };
        struct {            // mmu page table, can be shared
            u32 ref_count;
        };
    };
} __ALIGNED(sizeof(usize)) page_t;

// page types
#define PT_INVALID      0       // memory hole or mapped device
#define PT_FREE         1
#define PT_KERNEL       2       // generic kernel usage (code/data)
#define PT_MMU          3       // mmu page table
#define PT_POOL         4       // used by pool_t
#define PT_HEAP         5       // used by heap_t
#define PT_STACK        6       // (kernel) stack

// block order
#define ORDER_COUNT     16
#define NO_ORDER        ((u8) -1)

// memory zone bit masks
#define ZONE_DMA        1
#define ZONE_NORMAL     2
#define ZONE_HIGHMEM    4

extern page_t * page_array;

#endif // CORE_PAGE_H

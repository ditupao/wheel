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

typedef struct pglist {
    spin_t lock;
    pfn_t  head;
    pfn_t  tail;
} pglist_t;

// page types
#define PT_INVALID      0       // memory hole or mapped device
#define PT_FREE         1       // not allocated
#define PT_CACHED       2       // percpu cache
#define PT_KERNEL       3       // generic kernel usage
#define PT_POOL         4       // memory pool
#define PT_KSTACK       5       // task's kernel stack page

// block order
#define ORDER_COUNT     16

// page list initializer
#define PGLIST_INIT     ((pglist_t) { SPIN_INIT, NO_PAGE, NO_PAGE })

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
extern pfn_t page_alloc      (u32 zones);
extern void  page_free       (pfn_t page);
extern usize free_page_count (u32 zones);
extern void  dump_page_layout(u32 zones);

extern void  pglist_push_head(pglist_t * list, pfn_t page);
extern void  pglist_push_tail(pglist_t * list, pfn_t page);
extern pfn_t pglist_pop_head (pglist_t * list);
extern pfn_t pglist_pop_tail (pglist_t * list);
extern void  pglist_join_head(pglist_t * list, pglist_t * from);
extern void  pglist_join_tail(pglist_t * list, pglist_t * from);

extern __INIT void page_lib_init ();
extern __INIT void page_range_add(usize start, usize end);

#endif // CORE_PAGE_H

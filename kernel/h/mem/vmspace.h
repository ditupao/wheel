#ifndef MEM_VMSPACE_H
#define MEM_VMSPACE_H

#include <base.h>
#include <mem/page.h>

// represents a process
typedef struct vmspace {
    spin_t   lock;
    usize    ctx;
    dllist_t ranges;
} vmspace_t;

// represents a continuous range in the process 
typedef struct vmrange {
    dlnode_t dl;        // node in vmspace.ranges
    usize    addr;      // start address, aligned to page size
    usize    size;      // range size, aligned to page size
    u32      type;      // free or used
    pglist_t pages;     // list of mapped pages
} vmrange_t;

// range type
#define RT_FREE 0
#define RT_USED 1

extern void        vmspace_init    (vmspace_t * space);
extern void        vmspace_destroy (vmspace_t * space);
extern int         vmspace_add_free(vmspace_t * space, usize addr, usize size);
extern int         vmspace_add_used(vmspace_t * space, usize addr, usize size);
extern vmrange_t * vmspace_alloc   (vmspace_t * space, usize size);
extern vmrange_t * vmspace_alloc_at(vmspace_t * space, usize addr, usize size);
extern void        vmspace_free    (vmspace_t * space, vmrange_t * range);
extern int         vmspace_is_free (vmspace_t * space, usize addr, usize size);
extern int         vmspace_map     (vmspace_t * space, vmrange_t * range);
extern void        vmspace_unmap   (vmspace_t * space, vmrange_t * range);

// requires: nothing
extern __INIT void vmspace_lib_init();

#endif // MEM_VMSPACE_H

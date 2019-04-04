#ifndef CORE_VMSPACE_H
#define CORE_VMSPACE_H

#include <base.h>

typedef struct vmspace {
    usize    ctx;
    dllist_t ranges;
} vmspace_t;

typedef struct vmrange {
    dlnode_t dl;        // node in vmspace.ranges
    usize    addr;
    usize    size;
    u32      type;
    pfn_t    pages;     // (single linked list) mapped page
} vmrange_t;

// range type
#define RT_FREE 0
#define RT_USED 1

extern void        vmspace_init    (vmspace_t * space);
extern int         vmspace_add_free(vmspace_t * space, usize addr, usize size);
extern int         vmspace_add_used(vmspace_t * space, usize addr, usize size);
extern vmrange_t * vmspace_alloc   (vmspace_t * space, usize size);
extern vmrange_t * vmspace_alloc_at(vmspace_t * space, usize addr, usize size);
extern void        vmspace_free    (vmspace_t * space, vmrange_t * range);
extern int         vmspace_is_free (vmspace_t * space, usize addr, usize size);

extern int  vmrange_pages_alloc(vmrange_t * range);
extern void vmrange_pages_free (vmrange_t * range);

#endif // CORE_VMSPACE_H

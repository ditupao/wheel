#ifndef CORE_VMSPACE_H
#define CORE_VMSPACE_H

#include <base.h>

// `addr` and `size` must be page-aligned, we can
// use lower bits as range type

typedef struct vmspace {
    dllist_t ranges;
} vmspace_t;

typedef struct vmrange {
    dlnode_t dl;        // node in vmspace.ranges
    usize    addr;
    usize    size_type;
} vmrange_t;

// range type
#define RANGE_SIZE(st)  ((st) & ~(PAGE_SIZE-1U))
#define RANGE_TYPE(st)  ((st) &  (PAGE_SIZE-1U))
#define RT_FREE         0
#define RT_USED         1

extern void vmspace_init(vmspace_t * space);
extern int  vmspace_add_free(vmspace_t * space, usize addr, usize size);
extern int  vmspace_add_used(vmspace_t * space, usize addr, usize size);
extern vmrange_t * vmspace_alloc(vmspace_t * space, usize size);
extern vmrange_t * vmspace_alloc_at(vmspace_t * space, usize addr, usize size);
extern void vmspace_free(vmspace_t * space, vmrange_t * range);

#endif // CORE_VMSPACE_H

#if !defined(CORE_VMSPACE_H)
#define CORE_VMSPACE_H

#include <base.h>
#include <libk/rbtree.h>

typedef struct vmspace {
    rbtree_t size_tree;
    rbtree_t addr_tree;
} vmspace_t;

extern void  vmspace_init(vmspace_t * space);
extern void  vmspace_add(vmspace_t * space, usize addr, usize size);
extern usize vmspace_alloc(vmspace_t * space, usize size);
extern usize vmspace_alloc_at(vmspace_t * space, usize addr, usize size);

extern __INIT void vmspace_lib_init();

#endif // CORE_VMSPACE_H

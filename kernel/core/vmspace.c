#include <wheel.h>

// virtual address space manager
// similar to an interval-tree

// block type
#define BT_FREE 0           // not allocated
#define BT_USED 1           // already allocated

typedef struct vmsize {
    rbnode_t rb;
    dllist_t free_list;
    usize    size;
} vmsize_t;

typedef struct vmblob {
    rbnode_t   rb;          // node in addr_tree
    dlnode_t   dl;          // node in free_list
    usize      addr;        // start address of this blob
    usize      size;        // total size of this blob
    u32        type;
    union {
        struct {            // BT_FREE
            dllist_t * list;
        };
        struct {            // BT_USED
            pfn_t   first;
        };
    };
} vmblob_t;

// used for both vmsize and vmblob
static pool_t vm_pool;

void vmspace_init(vmspace_t * space) {
    space->addr_tree = RBTREE_INIT;
    space->size_tree = RBTREE_INIT;
}

void vmspace_add(vmspace_t * space, usize addr, usize size) {
    // TODO: check if under user-mode allowed range
    dbg_assert((addr & (PAGE_SIZE - 1)) == 0);
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    // first find the corresponding size node
    rbnode_t ** size_link = &space->size_tree.root;
    vmsize_t *  size_node = NULL;
    while (NULL != *size_link) {
        size_node = PARENT(*size_link, vmsize_t, rb);
        if (size < size_node->size) {
            size_link = &(*size_link)->left;
            continue;
        }
        if (size > size_node->size) {
            size_link = &(*size_link)->right;
            continue;
        }
        break;
    }

    if (NULL == *size_link) {
        // no such size, create new size node and put into rbtree
        // now `size_node` is the parent of new size node
        vmsize_t * new_size = (vmsize_t *) pool_obj_alloc(&vm_pool);
        new_size->rb        = RBNODE_INIT;
        new_size->free_list = DLLIST_INIT;
        new_size->size      = size;

        rb_link_node(&new_size->rb, *size_link, size_link);
        rb_insert_fixup(&new_size->rb, &space->size_tree);
        size_node = new_size;
    }

    // then find the closest address node
    rbnode_t ** addr_link = &space->addr_tree.root;
    vmblob_t *  addr_node = NULL;
    usize       end       = addr + size;
    while (NULL != *addr_link) {
        addr_node = PARENT(*addr_link, vmblob_t, rb);
        if (end <= addr_node->addr) {
            addr_link = &(*addr_link)->left;
            continue;
        }
        if (addr >= addr_node->addr + addr_node->size) {
            addr_link = &(*addr_link)->right;
            continue;
        }
        break;
    }

    if (NULL == *addr_link) {
        // create new address node for the range
        vmblob_t * new_blob = (vmblob_t *) pool_obj_alloc(&vm_pool);
        new_blob->rb        = RBNODE_INIT;
        new_blob->dl        = DLNODE_INIT;
        new_blob->addr      = addr;
        new_blob->size      = size;
        new_blob->list      = &size_node->free_list;

        rb_link_node(&new_blob->rb, *addr_link, addr_link);
        rb_insert_fixup(&new_blob->rb, &space->addr_tree);
        dl_push_head(&size_node->free_list, &new_blob->dl);
        addr_node = new_blob;
    } else {
        // overlapping is not allowed
        dbg_print("address space overlapping!!!!\r\n");
        if ((NULL == size_node->free_list.head) &&
            (NULL == size_node->free_list.tail)) {
            rb_erase(&size_node->rb, &space->size_tree);
            pool_obj_free(&vm_pool, size_node);
        }
        return;
    }
}

usize vmspace_alloc(vmspace_t * space, usize size) {
    // find smallest node larger than or equal to `size`
    rbnode_t * rb   = space->size_tree.root;
    vmsize_t * cand = NULL;
    while (NULL != rb) {
        vmsize_t * node = PARENT(rb, vmsize_t, rb);
        if (node->size > size) {
            cand = node;
            rb = rb->left;
        } else {
            rb = rb->right;
        }
    }

    if (NULL == cand) {
        // no enough space
        return NO_ADDR;
    }

    // retrieve first free blob, take it out from freelist
    vmblob_t * blob = PARENT(dl_pop_head(&cand->free_list), vmblob_t, dl);
    if ((NULL == cand->free_list.head) &&
        (NULL == cand->free_list.tail)) {
        rb_erase(&cand->rb, &space->size_tree);
        pool_obj_free(&vm_pool, cand);
    }

    if (blob->size > size) {
        // TODO: return the rest back
    }
    
    return NO_ADDR;
}

usize vmspace_alloc_at(vmspace_t * space, usize addr, usize size) {
    // find smallest node larger than or equal to `size`
    rbnode_t * rb   = space->addr_tree.root;
    vmblob_t * node = NULL;
    usize      end  = addr + size;
    while (NULL != rb) {
        node = PARENT(rb, vmblob_t, rb);
        if (end <= node->addr) {
            rb = rb->left;
            continue;
        }
        if (addr >= node->addr + node->size) {
            rb = rb->right;
            continue;
        }
        break;
    }

    if (NULL == rb) {
        return NO_ADDR;
    }

    usize      blob_addr = node->addr;
    usize      blob_end  = node->addr + node->size;
    vmsize_t * size_node = PARENT(node->list, vmsize_t, free_list);
    dbg_assert(size_node->size == node->size);

    if ((addr < blob_addr) || (blob_end < end)) {
        return NO_ADDR;
    }

    // take this node out
    rb_erase(&node->rb, &space->addr_tree);
    dl_remove(&size_node->free_list, &node->dl);
    if ((NULL == size_node->free_list.head) &&
        (NULL == size_node->free_list.tail)) {
        rb_erase(&size_node->rb, &space->size_tree);
        pool_obj_free(&vm_pool, size_node);
    }

    // add the remaining back
    if (blob_addr < addr) {
        vmspace_add(space, blob_addr, addr - blob_addr);
    }
    if (end < blob_end) {
        vmspace_add(space, end, blob_end - end);
    }

    return addr;
}

__INIT void vmspace_lib_init() {
    pool_init(&vm_pool, MAX(sizeof(vmsize_t), sizeof(vmblob_t)));
}

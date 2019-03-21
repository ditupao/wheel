#include <wheel.h>

static pool_t range_pool;

void vmspace_init(vmspace_t * space) {
    space->ranges = DLLIST_INIT;
}

// add a new region into the virtual memory space, and mark as free
int vmspace_add_free(vmspace_t * space, usize addr, usize size) {
    dbg_assert((addr & (PAGE_SIZE - 1)) == 0);
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    // search addr_list, looking for the first range after the new resion
    dlnode_t  * dl   = space->ranges.head;
    vmrange_t * prev = NULL;
    vmrange_t * next = NULL;
    for (; NULL != dl; dl = dl->next) {
        next = PARENT(dl, vmrange_t, dl);
        if (addr + size <= next->addr) {
            // this is the next range
            break;
        }
        if (next->addr + next->size_type > addr) {
            // overlap with existing range
            return ERROR;
        }
        prev = next;
        next = NULL;
    }

    vmrange_t * range = NULL;

    if ((NULL != prev) &&
        (addr == prev->addr + RANGE_SIZE(prev->size_type)) &&
        (RT_FREE == RANGE_TYPE(prev->size_type))) {
        // merge with prev range
        range = prev;
        range->size_type += size;
    }

    if ((NULL != next) &&
        (addr + size == next->addr) &&
        (RT_FREE == RANGE_TYPE(next->size_type))) {
        // merge with next range
        if (NULL != range) {
            range->size_type += RANGE_SIZE(range->size_type);
            dl_remove(&space->ranges, &next->dl);
            pool_obj_free(&range_pool, next);
        } else {
            range = next;
            range->addr = addr;
            range->size_type += size;
        }
    }

    if (NULL == range) {
        // cannot merge, create new node and insert before `next`
        range = (vmrange_t *) pool_obj_alloc(&range_pool);
        range->dl         = DLNODE_INIT;
        range->addr       = addr;
        range->size_type  = size | RT_FREE;
        dl_insert_before(&space->ranges, &range->dl, dl);
    }

    return OK;
}

// add a new region into the virtual memory space, and mark as used
int vmspace_add_used(vmspace_t * space, usize addr, usize size) {
    dbg_assert((addr & (PAGE_SIZE - 1)) == 0);
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    // search addr_list, looking for the first range after the new resion
    dlnode_t  * dl   = space->ranges.head;
    for (; NULL != dl; dl = dl->next) {
        vmrange_t * next = PARENT(dl, vmrange_t, dl);
        if (addr + size <= next->addr) {
            // this is the next range
            break;
        }
        if (next->addr + next->size_type > addr) {
            // overlap with existing range
            return ERROR;
        }
    }

    // used ranges cannot merge
    vmrange_t * range = range = (vmrange_t *) pool_obj_alloc(&range_pool);
    range->dl         = DLNODE_INIT;
    range->addr       = addr;
    range->size_type  = size | RT_USED;
    dl_insert_before(&space->ranges, &range->dl, dl);

    return OK;
}

vmrange_t * vmspace_alloc(vmspace_t * space, usize size) {
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    // search for the smallest free range that is large enough
    usize       min_size  = (usize) -1;
    vmrange_t * min_range = NULL;
    for (dlnode_t * dl = space->ranges.head; NULL != dl; dl = dl->next) {
        vmrange_t * range = PARENT(dl, vmrange_t, dl);
        if ((RT_FREE != RANGE_TYPE(range->size_type)) &&
            (size     > RANGE_SIZE(range->size_type))) {
            continue;
        }

        if (RANGE_SIZE(range->size_type) < min_size) {
            min_size  = RANGE_SIZE(range->size_type);
            min_range = range;
        }
    }

    if (NULL == min_range) {
        return NULL;
    }

    if (RANGE_SIZE(min_range->size_type) > size) {
        vmrange_t * rest = (vmrange_t *) pool_obj_alloc(&range_pool);
        rest->dl         = DLNODE_INIT;
        rest->addr       = min_range->addr + RANGE_SIZE(min_range->size_type);
        rest->size_type  = (RANGE_SIZE(min_range->size_type) - size) | RT_FREE;
        dl_insert_after(&space->ranges, &rest->dl, &min_range->dl);
    }

    min_range->size_type = size | RT_USED;
    return min_range;
}

vmrange_t * vmspace_alloc_at(vmspace_t * space, usize addr, usize size) {
    dbg_assert((addr & (PAGE_SIZE - 1)) == 0);
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    usize end = addr + size;

    for (dlnode_t * dl = space->ranges.head; NULL != dl; dl = dl->next) {
        vmrange_t * range = PARENT(dl, vmrange_t, dl);
        if (range->addr > addr) {
            break;
        }
        if ((RT_FREE != RANGE_TYPE(range->size_type)) &&
            (end > range->addr + RANGE_SIZE(range->size_type))) {
            continue;
        }

        // found a valid range

        if (range->addr < addr) {
            vmrange_t * prev = (vmrange_t *) pool_obj_alloc(&range_pool);
            prev->dl        = DLNODE_INIT;
            prev->addr      = range->addr;
            prev->size_type = (addr - range->addr) | RT_FREE;
            dl_insert_before(&space->ranges, &prev->dl, dl);
        }

        if (end < range->addr + RANGE_SIZE(range->size_type)) {
            vmrange_t * next = (vmrange_t *) pool_obj_alloc(&range_pool);
            next->dl        = DLNODE_INIT;
            next->addr      = end;
            next->size_type = (range->addr + RANGE_SIZE(range->size_type) - end) | RT_FREE;
            dl_insert_after(&space->ranges, &next->dl, dl);
        }

        range->addr      = addr;
        range->size_type = size | RT_USED;
        return range;
    }

    return NULL;
}

void vmspace_free(vmspace_t * space, vmrange_t * range) {
    dbg_assert(RT_USED == RANGE_TYPE(range->size_type));
    range->size_type = RANGE_SIZE(range->size_type) | RT_FREE;

    if (NULL != range->dl.prev) {
        vmrange_t * prev = PARENT(range->dl.prev, vmrange_t, dl);
        if ((RT_FREE == RANGE_TYPE(prev->size_type)) &&
            (range->addr == prev->addr + RANGE_SIZE(prev->size_type))) {
            range->addr       = prev->addr;
            range->size_type += RANGE_SIZE(prev->size_type);
            dl_remove(&space->ranges, &prev->dl);
        }
    }

    if (NULL != range->dl.next) {
        vmrange_t * next = PARENT(range->dl.next, vmrange_t, dl);
        if ((RT_FREE == RANGE_TYPE(next->size_type)) &&
            (next->addr == range->addr + RANGE_SIZE(range->size_type))) {
            range->size_type += RANGE_SIZE(range->size_type);
            dl_remove(&space->ranges, &next->dl);
        }
    }
}

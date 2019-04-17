#include <wheel.h>

static pool_t range_pool;

void vmspace_init(vmspace_t * space) {
    space->lock   = SPIN_INIT;
    space->ctx    = mmu_ctx_create();
    space->ranges = DLLIST_INIT;
    vmspace_add_free(space, USER_START, USER_END - USER_START);
}

void vmspace_destroy(vmspace_t * space) {
    dlnode_t * dl;
    while (NULL != (dl = dl_pop_head(&space->ranges))) {
        vmrange_t * range = PARENT(dl, vmrange_t, dl);
        vmrange_unmap(space, range);
        pool_obj_free(&range_pool, range);
    }
}

// add a new region into the virtual memory space, and mark as free
int vmspace_add_free(vmspace_t * space, usize addr, usize size) {
    dbg_assert((addr & (PAGE_SIZE - 1)) == 0);
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    u32 key = irq_spin_take(&space->lock);

    // search addr_list, looking for the first range after the new region
    dlnode_t  * dl   = space->ranges.head;
    vmrange_t * prev = NULL;
    vmrange_t * next = NULL;
    for (; NULL != dl; dl = dl->next) {
        next = PARENT(dl, vmrange_t, dl);
        if (addr + size <= next->addr) {
            // this is the next range
            break;
        }
        if (next->addr + next->size > addr) {
            // overlap with existing range
            irq_spin_give(&space->lock, key);
            return ERROR;
        }
        prev = next;
        next = NULL;
    }

    vmrange_t * range = NULL;

    if ((NULL != prev) &&
        (addr == prev->addr + prev->size) &&
        (RT_FREE == prev->type)) {
        // merge with prev range
        range = prev;
        range->size += size;
    }

    if ((NULL != next) &&
        (addr + size == next->addr) &&
        (RT_FREE == next->type)) {
        // merge with next range
        if (NULL != range) {
            range->size += range->size;
            dl_remove(&space->ranges, &next->dl);
            pool_obj_free(&range_pool, next);
        } else {
            range = next;
            range->addr = addr;
            range->size += size;
        }
    }

    if (NULL == range) {
        // cannot merge, create new node and insert before `next`
        range = (vmrange_t *) pool_obj_alloc(&range_pool);
        range->dl    = DLNODE_INIT;
        range->addr  = addr;
        range->size  = size;
        range->type  = RT_FREE;
        range->pages = NO_PAGE;
        dl_insert_before(&space->ranges, &range->dl, dl);
    }

    irq_spin_give(&space->lock, key);
    return OK;
}

// add a new region into the virtual memory space, and mark as used
int vmspace_add_used(vmspace_t * space, usize addr, usize size) {
    dbg_assert((addr & (PAGE_SIZE - 1)) == 0);
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    u32 key = irq_spin_take(&space->lock);

    // search addr_list, looking for the first range after the new resion
    dlnode_t  * dl   = space->ranges.head;
    for (; NULL != dl; dl = dl->next) {
        vmrange_t * next = PARENT(dl, vmrange_t, dl);
        if (addr + size <= next->addr) {
            // this is the next range
            break;
        }
        if (next->addr + next->size > addr) {
            // overlap with existing range
            irq_spin_give(&space->lock, key);
            return ERROR;
        }
    }

    // used ranges cannot merge
    vmrange_t * range = (vmrange_t *) pool_obj_alloc(&range_pool);
    range->dl    = DLNODE_INIT;
    range->addr  = addr;
    range->size  = size;
    range->type  = RT_USED;
    range->pages = NO_PAGE;
    dl_insert_before(&space->ranges, &range->dl, dl);

    irq_spin_give(&space->lock, key);
    return OK;
}

vmrange_t * vmspace_alloc(vmspace_t * space, usize size) {
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    u32 key = irq_spin_take(&space->lock);

    // search for the smallest free range that is large enough
    usize       min_size  = (usize) -1;
    vmrange_t * min_range = NULL;
    for (dlnode_t * dl = space->ranges.head; NULL != dl; dl = dl->next) {
        vmrange_t * range = PARENT(dl, vmrange_t, dl);
        if ((RT_FREE != range->type) ||
            (size     > range->size)) {
            continue;
        }

        if (range->size < min_size) {
            min_size  = range->size;
            min_range = range;
        }
    }

    if (NULL == min_range) {
        irq_spin_give(&space->lock, key);
        return NULL;
    }

    if (min_range->size > size) {
        vmrange_t * rest = (vmrange_t *) pool_obj_alloc(&range_pool);
        rest->dl    = DLNODE_INIT;
        rest->addr  = min_range->addr + min_range->size;
        rest->size  = min_range->size - size;
        rest->type  = RT_FREE;
        rest->pages = NO_PAGE;
        dl_insert_after(&space->ranges, &rest->dl, &min_range->dl);
    }

    min_range->size = size;
    min_range->type = RT_USED;

    irq_spin_give(&space->lock, key);
    return min_range;
}

vmrange_t * vmspace_alloc_at(vmspace_t * space, usize addr, usize size) {
    dbg_assert((addr & (PAGE_SIZE - 1)) == 0);
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    u32   key = irq_spin_take(&space->lock);
    usize end = addr + size;

    for (dlnode_t * dl = space->ranges.head; NULL != dl; dl = dl->next) {
        vmrange_t * range = PARENT(dl, vmrange_t, dl);
        if ((RT_FREE != range->type) ||
            (end > range->addr + range->size)) {
            continue;
        }

        if (range->addr > addr) {
            // already passed the end, so target range is not free
            break;
        }

        // we've found a valid range, return extra space
        if (range->addr < addr) {
            vmrange_t * prev = (vmrange_t *) pool_obj_alloc(&range_pool);
            prev->dl    = DLNODE_INIT;
            prev->addr  = range->addr;
            prev->size  = addr - range->addr;
            prev->type  = RT_FREE;
            prev->pages = NO_PAGE;
            dl_insert_before(&space->ranges, &prev->dl, dl);
        }

        if (end < range->addr + range->size) {
            vmrange_t * next = (vmrange_t *) pool_obj_alloc(&range_pool);
            next->dl    = DLNODE_INIT;
            next->addr  = end;
            next->size  = range->addr + range->size - end;
            next->type  = RT_FREE;
            next->pages = NO_PAGE;
            dl_insert_after(&space->ranges, &next->dl, dl);
        }

        range->addr= addr;
        range->size = size;
        range->type = RT_USED;

        irq_spin_give(&space->lock, key);
        return range;
    }

    irq_spin_give(&space->lock, key);
    return NULL;
}

void vmspace_free(vmspace_t * space, vmrange_t * range) {
    dbg_assert(RT_USED == range->type);

    u32 key = irq_spin_take(&space->lock);

    range->size = range->size;
    range->type = RT_FREE;

    if (NULL != range->dl.prev) {
        vmrange_t * prev = PARENT(range->dl.prev, vmrange_t, dl);
        if ((RT_FREE     == prev->type) &&
            (range->addr == prev->addr + (prev->size))) {
            range->addr  = prev->addr;
            range->size += prev->size;
            dl_remove(&space->ranges, &prev->dl);
        }
    }

    if (NULL != range->dl.next) {
        vmrange_t * next = PARENT(range->dl.next, vmrange_t, dl);
        if ((RT_FREE    == next->type) &&
            (next->addr == range->addr + range->size)) {
            range->size += range->size;
            dl_remove(&space->ranges, &next->dl);
        }
    }

    irq_spin_give(&space->lock, key);
}

// check whether the given range is free
int vmspace_is_free(vmspace_t * space, usize addr, usize size) {
    dbg_assert((addr & (PAGE_SIZE - 1)) == 0);
    dbg_assert((size & (PAGE_SIZE - 1)) == 0);

    u32   key = irq_spin_take(&space->lock);
    usize end = addr + size;

    for (dlnode_t * dl = space->ranges.head; NULL != dl; dl = dl->next) {
        vmrange_t * range = PARENT(dl, vmrange_t, dl);
        if ((RT_FREE != range->type) &&
            (end > range->addr + range->size)) {
            continue;
        }

        if (range->addr > addr) {
            // already passed the end, so target range is not free
            break;
        }

        // found a valid range
        irq_spin_give(&space->lock, key);
        return YES;
    }

    // no such range, or range is not free
    irq_spin_give(&space->lock, key);
    return NO;
}

int vmrange_map(vmspace_t * space, vmrange_t * range) {
    dbg_assert(RT_USED == range->type);
    dbg_assert(NO_PAGE == range->pages);

    u32   key        = irq_spin_take(&space->lock);
    usize page_count = range->size >> PAGE_SHIFT;

    for (usize i = 0; i < page_count; ++i) {
        pfn_t p = page_block_alloc(ZONE_DMA|ZONE_NORMAL, 0);
        if (NO_PAGE == p) {
            irq_spin_give(&space->lock, key);
            return ERROR;
        }
        page_array[p].next = range->pages;
        range->pages = p;

        usize va = range->addr + PAGE_SIZE * i;
        usize pa = (usize) p << PAGE_SHIFT;
        mmu_map(space->ctx, va, pa, 1, 0);
    }

    irq_spin_give(&space->lock, key);
    return OK;
}

void vmrange_unmap(vmspace_t * space, vmrange_t * range) {
    u32 key = irq_spin_take(&space->lock);

    if (RT_USED != range->type) {
        irq_spin_give(&space->lock, key);
        return;
    }

    int page_count = range->size >> PAGE_SHIFT;
    mmu_unmap(space->ctx, range->addr, page_count);

    pfn_t p = range->pages;
    while (NO_PAGE != p) {
        page_block_free(p, 0);
        p = page_array[p].next;
    }
    range->pages = NO_PAGE;

    irq_spin_give(&space->lock, key);
}

__INIT void vmspace_lib_init() {
    pool_init(&range_pool, sizeof(vmrange_t));
}

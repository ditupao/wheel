#include <wheel.h>

static pool_t pipe_pool;

pipe_t * pipe_create() {
    pipe_t * pipe = (pipe_t *) pool_obj_alloc(&pipe_pool);
    pipe->lock         = SPIN_INIT;
    pipe->pages        = PGLIST_INIT;
    pipe->offset_read  = 0;
    pipe->offset_write = 0;
    semaphore_init(&pipe->sem, 1, 0);   // initial state is empty

    pfn_t pn = page_block_alloc(ZONE_NORMAL|ZONE_DMA, 0);
    pglist_push_tail(&pipe->pages, pn);
    page_array[pn].block = 1;
    page_array[pn].order = 0;
    page_array[pn].type  = PT_PIPE;

    return pipe;
}

void pipe_delete(pipe_t * pipe) {
    pglist_free_all(&pipe->pages);
    pool_obj_free(&pipe_pool, pipe);
}

usize pipe_read(pipe_t * pipe, u8 * buf, usize len) {
    usize backup_len = len;

    if ((pipe->pages.head  == pipe->pages.tail) &&
        (pipe->offset_read == pipe->offset_write)) {
        // pipe is empty, pend and wait for content
        semaphore_take(&pipe->sem, SEM_WAIT_FOREVER);
    }

    while (len) {
        if (pipe->pages.head == pipe->pages.tail) {
            // this is the last page, make sure read_offset
            // does not exceeds write_offset
            pfn_t head = pipe->pages.head;
            u8  * addr = phys_to_virt((usize) head << PAGE_SHIFT);
            usize copy = MIN(len, (usize) pipe->offset_write - pipe->offset_read);
            memcpy(buf, addr + pipe->offset_read, copy);
            pipe->offset_read += copy;
            buf               += copy;
            len               -= copy;

            // we've come to the end
            return backup_len - len;
        }

        // offset_read and offset_write in different pages
        pfn_t head = pipe->pages.head;
        u8  * addr = phys_to_virt((usize) head << PAGE_SHIFT);
        usize copy = MIN(len, (usize) PAGE_SIZE - pipe->offset_read);
        memcpy(buf, addr + pipe->offset_read, copy);
        pipe->offset_read += copy;
        buf               += copy;
        len               -= copy;

        // free head page if all content have been read
        if (PAGE_SIZE == pipe->offset_read) {
            head = pglist_pop_head(&pipe->pages);
            page_block_free(head, 0);
            pipe->offset_read = 0;
        }
    }

    return backup_len;
}

usize pipe_write(pipe_t * pipe, u8 * buf, usize len) {
    while (len) {
        // write to `pages.tail + write_offset`
        pfn_t tail = pipe->pages.tail;
        u8  * addr = phys_to_virt((usize) tail << PAGE_SHIFT);
        usize copy = MIN(len, (usize) PAGE_SIZE - pipe->offset_write);
        memcpy(addr + pipe->offset_write, buf, copy);
        pipe->offset_write += copy;
        buf                += copy;
        len                -= copy;

        // allocate new space if current tail page is used up
        if (PAGE_SIZE == pipe->offset_write) {
            tail = page_block_alloc(ZONE_NORMAL|ZONE_DMA, 0);
            pglist_push_tail(&pipe->pages, tail);
            page_array[tail].block = 1;
            page_array[tail].order = 0;
            page_array[tail].type  = PT_PIPE;
            pipe->offset_write = 0;
        }
    }

    // we can always write all content
    semaphore_give(&pipe->sem);
    return len;
}

__INIT void pipe_lib_init() {
    pool_init(&pipe_pool, sizeof(pipe_t));
}

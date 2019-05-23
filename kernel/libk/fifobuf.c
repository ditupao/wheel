#include <wheel.h>

int fifobuf_init(fifobuf_t * buf) {
    buf->pages   = PGLIST_INIT;
    buf->roffset = 0;
    buf->woffset = 0;

    pfn_t pn = page_block_alloc(ZONE_NORMAL|ZONE_DMA, 0);
    if (NO_PAGE == pn) {
        return ERROR;
    }

    page_array[pn].block = 1;
    page_array[pn].order = 0;
    page_array[pn].type  = PT_FIFOBUF;
    pglist_push_tail(&buf->pages, pn);

    return OK;
}

void fifobuf_destroy(fifobuf_t * buf) {
    pglist_free_all(&buf->pages);
}

// return the number of bytes read
usize fifobuf_read(fifobuf_t * buf, u8 * data, usize len) {
    usize backup_len = len;

    while (len) {
        if (buf->pages.head == buf->pages.tail) {
            // this is the last page, make sure read_offset
            // does not exceeds write_offset
            pfn_t head = buf->pages.head;
            u8  * addr = phys_to_virt((usize) head << PAGE_SHIFT);
            usize copy = MIN(len, (usize) buf->woffset - buf->roffset);
            memcpy(data, addr + buf->roffset, copy);
            buf->roffset += copy;
            data         += copy;
            len          -= copy;

            // there's nothing more to read
            return backup_len - len;
        }

        // roffset and woffset in different pages
        pfn_t head = buf->pages.head;
        u8  * addr = phys_to_virt((usize) head << PAGE_SHIFT);
        usize copy = MIN(len, (usize) PAGE_SIZE - buf->roffset);
        memcpy(data, addr + buf->roffset, copy);
        buf->roffset += copy;
        data         += copy;
        len          -= copy;

        // free head page if all content have been read
        if (PAGE_SIZE == buf->roffset) {
            head = pglist_pop_head(&buf->pages);
            page_block_free(head, 0);
            buf->roffset = 0;
        }
    }

    return backup_len;
}

// return the number of bytes written
usize fifobuf_write(fifobuf_t * buf, u8 * data, usize len) {
    usize backup_len = len;

    while (len) {
        pfn_t tail = buf->pages.tail;
        u8  * addr = phys_to_virt((usize) tail << PAGE_SHIFT);
        usize copy = MIN(len, (usize) PAGE_SIZE - buf->woffset);
        memcpy(addr + buf->woffset, data, copy);
        buf->woffset += copy;
        data         += copy;
        len          -= copy;

        // allocate new space if current tail page is used up
        if (PAGE_SIZE == buf->woffset) {
            tail = page_block_alloc(ZONE_NORMAL|ZONE_DMA, 0);
            pglist_push_tail(&buf->pages, tail);
            page_array[tail].block = 1;
            page_array[tail].order = 0;
            page_array[tail].type  = PT_FIFOBUF;
            buf->woffset = 0;
        }
    }

    return backup_len;
}

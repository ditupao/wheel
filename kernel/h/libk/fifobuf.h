#ifndef LIBK_FIFOBUF_H
#define LIBK_FIFOBUF_H

#include <base.h>
#include <mem/page.h>

typedef struct fifobuf {
    pglist_t pages;
    int      roffset;
    int      woffset;
} fifobuf_t;

extern int   fifobuf_init   (fifobuf_t * buf);
extern void  fifobuf_destroy(fifobuf_t * buf);
extern usize fifobuf_read   (fifobuf_t * buf, u8 * data, usize len);
extern usize fifobuf_write  (fifobuf_t * buf, u8 * data, usize len);

#endif // LIBK_FIFOBUF_H

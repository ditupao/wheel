#ifndef LIBK_TAR_H
#define LIBK_TAR_H

#include <base.h>

extern int  tar_find(u8 * tar, const char * name, u8 ** buff, usize * size);
extern void tar_dump(u8 * tar);

#endif // LIBK_TAR_H

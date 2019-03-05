#ifndef LIBK_VSPRINTF_H
#define LIBK_VSPRINTF_H

#include <base.h>

extern int vsnprintf(char * buf, usize size, const char * fmt, va_list args);
extern int  snprintf(char * buf, usize size, const char * fmt, ...);

#endif // LIBK_VSPRINTF_H

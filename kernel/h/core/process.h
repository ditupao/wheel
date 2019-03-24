#ifndef CORE_PROCESS_H
#define CORE_PROCESS_H

#include <base.h>
#include "vmspace.h"

typedef struct process {
    vmspace_t   vmspace;
    usize       ctx;
} process_t;

extern void process_init(process_t * pid);

#endif // CORE_PROCESS_H

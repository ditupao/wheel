#ifndef CORE_PROCESS_H
#define CORE_PROCESS_H

#include <base.h>
#include "vmspace.h"

// TODO: use double linked list for pages?
//       so that we can free memory during process lifetime.

typedef struct process {
    vmspace_t   vmspace;
    usize       ctx;
    pfn_t       pages;  // (single linked list) allocated pages
    dllist_t    tasks;  // (double linked list) child tasks
} process_t;

extern void process_init(process_t * pid);

#endif // CORE_PROCESS_H

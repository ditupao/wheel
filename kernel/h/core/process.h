#ifndef CORE_PROCESS_H
#define CORE_PROCESS_H

#include <base.h>
#include "spin.h"
#include "vmspace.h"

// TODO: use double linked list for pages?
//       so that we can free memory during process lifetime.

typedef struct process {
    spin_t      lock;
    dllist_t    tasks;  // (double linked list) child tasks
    vmspace_t   vm;     // virtual address space, and page table
    usize       entry;
} process_t;

extern void process_init(process_t * pid);

#endif // CORE_PROCESS_H

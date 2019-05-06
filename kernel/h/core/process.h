#ifndef CORE_PROCESS_H
#define CORE_PROCESS_H

#include <base.h>
#include <mem/vmspace.h>
#include <libk/spin.h>
#include <libk/list.h>

// TODO: use double linked list for pages?
//       so that we can free memory during process lifetime.

typedef struct process {
    spin_t      lock;
    usize       entry;
    dllist_t    tasks;  // (double linked list) child tasks
    vmspace_t   vm;     // virtual address space, and page table
} process_t;

extern process_t * process_create();
extern void process_delete(process_t * pid);

// requires: nothing
extern __INIT void process_lib_init();

#endif // CORE_PROCESS_H

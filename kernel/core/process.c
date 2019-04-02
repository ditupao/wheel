#include <wheel.h>

// create new (empty) process, no child task
void process_init(process_t * pid) {
    vmspace_init(&pid->vmspace);
    pid->ctx   = mmu_ctx_create();
    pid->pages = NO_PAGE;
    pid->tasks = DLLIST_INIT;
}


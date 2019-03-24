#include <wheel.h>

// create new process, no child task
void process_init(process_t * pid) {
    vmspace_init(&pid->vmspace);
    pid->ctx = mmu_ctx_create();
}

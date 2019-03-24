#include <wheel.h>

typedef struct process {
    vmspace_t   vmspace;
    usize       ctx;
} process_t;

__INIT void process_init(process_t * pid) {
    vmspace_init(&pid->vmspace);
    pid->ctx = mmu_ctx_create();
}

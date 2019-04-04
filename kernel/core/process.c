#include <wheel.h>

// create new (empty) process, no child task
void process_init(process_t * pid) {
    pid->lock  = SPIN_INIT;
    pid->tasks = DLLIST_INIT;
    vmspace_init(&pid->vm);
}

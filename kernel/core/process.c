#include <wheel.h>

static pool_t pcb_pool;

process_t * process_create() {
    process_t * pid = (process_t *) pool_obj_alloc(&pcb_pool);
    pid->lock  = SPIN_INIT;
    pid->entry = NO_ADDR;
    pid->tasks = DLLIST_INIT;
    vmspace_init(&pid->vm);

    semaphore_init(&pid->fd_sem, 1, 1);
    memset(pid->fd_array, 0, 32 * sizeof(fdesc_t *));

    // dbg_print("allocating process at %llx.\n", pid);
    return pid;
}

void process_delete(process_t * pid) {
    // dbg_print("reclaiming process at %llx.\n", pid);

    u32 key = irq_spin_take(&pid->lock);
    dbg_assert(NULL == pid->tasks.head);
    dbg_assert(NULL == pid->tasks.tail);
    vmspace_destroy(&pid->vm);

    semaphore_destroy(&pid->fd_sem);
    for (int i = 0; i < 32; ++i) {
        if (NULL == pid->fd_array[i]) {
            continue;
        }
        ios_close(pid->fd_array[i]);
        pid->fd_array[i] = NULL;
    }

    pool_obj_free(&pcb_pool, pid);
    int_unlock(key);
}

__INIT void process_lib_init() {
    pool_init(&pcb_pool, sizeof(process_t));
}

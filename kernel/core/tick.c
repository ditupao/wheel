#include <wheel.h>

typedef struct tick_q {
    spin_t   lock;
    dllist_t q;
} tick_q_t;

// make tick_q cpu local? NO
// if we use percpu tick queue, then `wdog_cancel` would need to figure
// out which cpu wdog is on. That might lead to race condition.
// or we'll have to lock wdog, making things even more complex.
static          tick_q_t tick_q;
static volatile usize    tick_count;

void wdog_init(wdog_t * wd) {
    memset(wd, 0, sizeof(wdog_t));
    wd->node.prev = &wd->node;
    wd->node.next = &wd->node;
}

void wdog_start(wdog_t * wd, int ticks, void * proc,
                void * a1, void * a2, void * a3, void * a4) {
    dbg_assert(NULL != wd);
    dbg_assert(ticks >= 0);
    if ((wd->node.prev != &wd->node) && (wd->node.next != &wd->node)) {
        return;
    }

    wd->proc = proc;
    wd->arg1 = a1;
    wd->arg2 = a2;
    wd->arg3 = a3;
    wd->arg4 = a4;
    ticks += 1;

    u32 key = irq_spin_take(&tick_q.lock);
    dlnode_t * node = tick_q.q.head;
    wdog_t   * wdog = PARENT(node, wdog_t, node);

    while ((NULL != node) && (wdog->ticks <= ticks)) {
        ticks -= wdog->ticks;
        node   = node->next;
        wdog   = PARENT(node, wdog_t, node);
    }

    wd->ticks = ticks;
    if (wdog) {
        wdog->ticks -= ticks;
    }

    dl_insert_before(&tick_q.q, &wd->node, node);
    irq_spin_give(&tick_q.lock, key);
}

void wdog_cancel(wdog_t * wd) {
    u32 key = irq_spin_take(&tick_q.lock);
    if ((wd->node.prev != &wd->node) && (wd->node.next != &wd->node)) {
        dlnode_t * node = wd->node.next;
        wdog_t *   next = PARENT(node, wdog_t, node);
        dl_remove(&tick_q.q, &wd->node);
        wd->node.prev = &wd->node;
        wd->node.next = &wd->node;
        if (NULL != node) {
            next->ticks += wd->ticks;
        }
    }
    irq_spin_give(&tick_q.lock, key);
}

// clock interrupt handler
void tick_advance() {
    if (0 == cpu_index()) {
        atomic_inc((atomic_t *) &tick_count);

        u32 k = irq_spin_take(&tick_q.lock);
        dlnode_t * node = tick_q.q.head;
        wdog_t   * wd   = PARENT(node, wdog_t, node);
        if (NULL != node) {
            --wd->ticks;
        }

        while ((NULL != node) && (wd->ticks <= 0)) {
            dl_pop_head(&tick_q.q);
            wd->node.prev = &wd->node;
            wd->node.next = &wd->node;

            irq_spin_give(&tick_q.lock, k);
            wd->proc(wd->arg1, wd->arg2, wd->arg3, wd->arg4);
            k = irq_spin_take(&tick_q.lock);

            node = tick_q.q.head;
            wd   = PARENT(node, wdog_t, node);
        }

        irq_spin_give(&tick_q.lock, k);
    }

    // round robin (not only for non-rt tasks)
    task_t * tid = thiscpu_var(tid_prev);
    --tid->remaining;
    // dbg_print("-%ds", tid->remaining);
    if (tid->remaining <= 0) {
        tid->remaining = tid->timeslice;
        task_yield();
    }
}

void tick_delay(int ticks) {
    usize start = tick_count;
    while ((tick_count - start) < (usize) ticks) {
        cpu_relax();
    }
}

__INIT void tick_lib_init() {
    tick_count  = 0;
    tick_q.lock = SPIN_INIT;
    tick_q.q    = DLLIST_INIT;
}

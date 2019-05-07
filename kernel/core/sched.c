#include <wheel.h>

#if 1

// we have three scheduling policies:
// - FIFO (0~29)
// - fair (30)
// - idle (31)

typedef struct sched_fifo {
    //
} sched_fifo_t;

// per-cpu ready queue of FIFO tasks
typedef struct fifo_queue {
    u32      priorities;
    dllist_t fair[PRIORITY_COUNT-2];
} fifo_queue_t;

static __PERCPU fifo_queue_t fifo_readyq;

static __PERCPU rbtree_t    fair_tree;


typedef struct sched_entry {
    int priority;
    union {
        struct {    // FIFO tasks
            dlnode_t dl_sched;
        };
        struct {    // fair tasks
            rbnode_t rb_sched;
        };
    };
} sched_entry_t;


#endif
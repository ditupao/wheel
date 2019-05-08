#include <wheel.h>

#if 1

// __PERCPU spin_t   sched_lock;
// __PERCPU task_t * tid_prev;
// __PERCPU task_t * tid_next;

// different levels of priorities:
// - realtime (0~28)
// - normal   (29)
// - daemon   (30)
// - idle     (31)

typedef struct sched_entry {
    int priority;
    int last_cpu;           // on which cpu this task was last scheduled
    union {
        struct {
            int         timeslice;  // total timeslice
            int         remaining;  // remaining timeslice
            dlnode_t    dl_sched;   // node in ready_q/pend_q
            dllist_t  * queue;      // current ready_q/pend_q
        } realtime;
        struct {
            int         start_time; // when this task was last scheduled
            int         vruntime;   // virtual run time
            rbnode_t    rb_sched;   // node in ready queue
        } normal;
    };
} sched_entry_t;

#endif
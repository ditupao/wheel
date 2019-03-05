#include <wheel.h>

__INITDATA u32 cpu_installed = 0;  // number of cpu installed
           u32 cpu_activated = 0;  // number of cpu activated
           u64 percpu_base   = 0;  // cpu0's offset to its percpu area
           u64 percpu_size   = 0;  // length of one per-cpu area
__PERCPU   u32 int_depth;
__PERCPU   u64 int_stack_ptr;
__PERCPU   u8  int_stack[16*PAGE_SIZE];

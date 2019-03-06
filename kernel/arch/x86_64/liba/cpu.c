#include <wheel.h>

__INITDATA u32 cpu_installed = 0;  // number of cpu installed
           u32 cpu_activated = 0;  // number of cpu activated
           u64 percpu_base   = 0;  // cpu0's offset to its percpu area
           u64 percpu_size   = 0;  // length of one per-cpu area
__PERCPU   u32 int_depth;
__PERCPU   u64 int_stack_ptr;
__PERCPU   u8  int_stack[16*PAGE_SIZE];

//------------------------------------------------------------------------------
// essential cpu features

u32 int_lock() {
    u64 key;
    ASM("pushfq; cli; popq %0" : "=r"(key));
    return (u32) key & 0x200;
}

void int_unlock(u32 key) {
    if (key & 0x200) {
        ASM("sti");
    }
}

#include <stddef.h>
#include <stdint.h>

// generate syscall id values
typedef enum syscall_id {
    #define DEFINE_SYSCALL(id, name, proc, ...) name = id,
    #include SYSCALL_DEF
    #undef DEFINE_SYSCALL
} syscall_id_t;

// generate syscall wrapper function prototypes
#define DEFINE_SYSCALL(id, name, proc, ...) extern int proc (__VA_ARGS__);
#include SYSCALL_DEF
#undef DEFINE_SYSCALL

int main(int argc, const char * argv[]) {
    // for (int i = 0; i < argc; ++i) {
    //     write(1, argv[i], 0);
    // }
    write(1, "hello world from main.c.\r\n", 0);
    return 0;
}

#include <stddef.h>
#include <stdint.h>
#include <syscall.h>

// // generate syscall wrapper function prototypes
// #define DEFINE_SYSCALL(id, name, ...) extern int __ ## name (__VA_ARGS__);
// #include SYSCALL_DEF
// #undef DEFINE_SYSCALL

extern int syscall(int func, ...);

int main(int argc, const char * argv[], const char * envp[]) {
    for (int i = 0; i < argc; ++i) {
        write(1, "got argument: ", 0);
        write(1, argv[i], 0);
        write(1, ".\r\n", 0);
    }
    for (int i = 0; envp[i]; ++i) {
        write(1, "got environment: ", 0);
        write(1, envp[i], 0);
        write(1, ".\r\n", 0);
    }
    write(1, "hello world from main.c.\r\n", 0);
    return 0;
}

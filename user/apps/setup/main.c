#include <wheel.h>

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

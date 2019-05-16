#include <system.h>

int main(int argc, const char * argv[], const char * envp[]) {
    for (int i = 0; i < argc; ++i) {
        write(1, "-- got argument: ", 0);
        write(1, argv[i], 0);
        write(1, ".\n", 0);
    }

    const char * new_argv[] = {
        "hello.app",
        "world",
        NULL
    };

    write(1, "spawning hello.app:\n", 0);
    spawn_process(new_argv[0], new_argv, envp);

    write(1, "dumping all tasks in the system.\n", 0);
    magic();

    return 0;
}

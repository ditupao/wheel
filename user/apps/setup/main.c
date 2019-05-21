#include <system.h>

void print(const char * s) {
    int len;
    for (len = 0; s[len]; ++len) {}
    write(1, s, len);
}

int main(int argc, const char * argv[], const char * envp[]) {
    for (int i = 0; i < argc; ++i) {
        print("-- got argument: ");
        print(argv[i]);
        print(".\n");
    }

    const char * new_argv[] = {
        "hello.app",
        "world",
        NULL
    };

    print("spawning hello.app:\n");
    spawn_process(new_argv[0], new_argv, envp);

    // print("dumping all tasks in the system.\n");
    // magic();

    return 0;
}

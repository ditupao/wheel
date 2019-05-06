#include <system.h>

int main(int argc, const char * argv[]) {
    for (int i = 0; i < argc; ++i) {
        write(1, ">> argv: ", 0);
        write(1, argv[i], 0);
        write(1, ".\n", 0);
    }
    write(1, "hello world!\n", 0);
    return 0;
}

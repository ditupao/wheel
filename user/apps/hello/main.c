#include <system.h>

void print(const char * s) {
    int len;
    for (len = 0; s[len]; ++len) {}
    write(1, s, len);
}

int main(int argc, const char * argv[]) {
    print("hello world from hello.app!\n");
    return 0;
}

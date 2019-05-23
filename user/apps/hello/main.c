#include <system.h>

void print(const char * s) {
    int len;
    for (len = 0; s[len]; ++len) {}
    write(1, (void *) s, len);
}

int main(int argc, const char * argv[]) {
    print("hello world from hello.app!\n");
    print("try typing something:\n");

    char buf[64];
    int len = read(1, buf, 32);
    buf[len] = '\0';

    char * s = "we got XX bytes: ";
    s[7] = '0' + (len / 10);
    s[8] = '0' + (len % 10);
    print(s);
    print(buf);
    print("\n");

    print("try typing something:\n");
    len = read(1, buf, 32);
    buf[len] = '\0';

    s[7] = '0' + (len / 10);
    s[8] = '0' + (len % 10);
    print(s);
    print(buf);
    print("\n");

    return 0;
}

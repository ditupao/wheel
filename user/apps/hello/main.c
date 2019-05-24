#include <system.h>

void print(const char * s) {
    int len;
    for (len = 0; s[len]; ++len) {}
    write(1, (void *) s, len);
}

void gets(char * s, int len) {
    while (len) {
        size_t got = read(0, s, len);
        for (int i = 0; i < got; ++i) {
            if (s[i] == '\n') {
                s[i] = '\0';
                return;
            }
        }

        s   += got;
        len -= got;
    }

    s[-1] = '\0';
}

int main(int argc, const char * argv[]) {
    char buf[64];

    print("hello world from hello.app!\n");
    print("try typing something: ");

    gets(buf, 64);
    print("we got: ");
    print(buf);
    print(".\n");

    return 0;
}

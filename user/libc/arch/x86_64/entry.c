extern int main(int argc, const char * argv[], const char * envp[]);

int entry(const char ** sp) {
    int argc = 0;
    const char ** argv = sp;
    while (argv[argc]) {
        ++argc;
    }

    const char ** envp = &argv[argc+1];

    return main(argc, argv, envp);
}

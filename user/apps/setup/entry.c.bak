extern int main(int argc, const char * argv[], const char * envp[]);

void entry(const char ** sp) {
    int argc = 0;
    const char ** argv = sp;
    while (argv[argc]) {
        ++argc;
    }

    const char ** envp = &argv[argc+1];

    main(argc, argv, envp);
}

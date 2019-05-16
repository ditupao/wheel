#include <wheel.h>

syscall_proc_t syscall_tbl[SYSCALL_NUM_COUNT];

// entry of the newly spawned thread, old vmspace, start in kernel mode
static void thread_entry(void * entry) {
    task_t    * self  = thiscpu_var(tid_prev);
    vmrange_t * range = vmspace_alloc(&self->process->vm, 16 * PAGE_SIZE);
    vmspace_map(&self->process->vm, range);

    dbg_assert(NULL == self->ustack);
    self->ustack = range;

    // jump into user mode, won't return
    return_to_user((usize) entry, range->addr + 16 * PAGE_SIZE);
}

// entry of the newly spawned process, new vmspace, start in kernel mode
static void process_entry(void * entry, void * sp) {
    dbg_assert(((usize) sp % sizeof(usize)) == 0);
    dbg_print("new process running on cpu-%d, ip=%llx sp=%llx:\n", cpu_index(), entry, sp);
    return_to_user((usize) entry, (usize) sp);
}

//------------------------------------------------------------------------------
// system call handler functions

int do_unsupported() {
    dbg_print("unsupported syscall.\n");
    return 0;
}

int do_exit(int exitcode) {
    task_t * self = thiscpu_var(tid_prev);
    self->ret_val = exitcode;
    task_exit();

    dbg_print("[panic] task_exit() returned!\n");
    return 0;
}

int do_wait(int subpid __UNUSED) {
    // TODO
    return 0;
}

int do_spawn_thread(void * entry) {
    task_t * cur = thiscpu_var(tid_prev);
    task_t * tid = task_create("new-thread", cur->priority, thread_entry, entry, 0,0,0);
    task_resume(tid);
    return 0;
}

extern u8 _ramfs_addr;

int do_spawn_process(const char * filename,
                     const char * argv[],
                     const char * envp[]) {
    // filename, argv and envp are not accessible from the new vmspace
    // so we need to copy them to the user stack

    // validate parameters
    if ((NULL == filename) ||
        (NULL == argv)     ||
        (NULL == envp)) {
        return 0;
    }

    // calculate total size of argv[]
    int num_argv = 0;
    int len_argv = 0;
    for (int i = 0; (NULL != argv[i]) && (i < 1024); ++i) {
        num_argv += 1;                      // space for argv pointer
        len_argv += strlen(argv[i]) + 1;    // space for argv string
    }

    // calculate total size of envp[]
    int num_envp = 0;
    int len_envp = 0;
    for (int i = 0; (NULL != envp[i]) && (i < 1024); ++i) {
        num_envp += 1;                      // space for envp pointer
        len_envp += strlen(envp[i]) + 1;    // space for envp string
    }

    // check for kernel stack overflow
    int total = ROUND_UP(len_argv + len_envp, sizeof(usize))
              + (num_argv + num_envp + 2) * sizeof(char *);
    if (total > PAGE_SIZE) {
        return -1;
    }

    // read file from file system
    u8  * bin_addr;
    usize bin_size;
    tar_find(&_ramfs_addr, filename, &bin_addr, &bin_size);

    // return if not found
    if ((NULL == bin_addr) && (0 == bin_size)) {
        dbg_print("[spawn_process] %s not found!\n", filename);
        return -2;
    }

    // create new process
    process_t * pid = process_create();

    // return if elf load failed
    if (OK != elf64_load(pid, bin_addr, bin_size)) {
        dbg_print("[spawn_process] %s load failed!\n", filename);
        process_delete(pid);
        return -3;
    }

    // allocate pages for user-mode stack
    vmrange_t * stk = vmspace_alloc(&pid->vm, 16 * PAGE_SIZE);
    vmspace_map(&pid->vm, stk);

    // put argv and envp at the beginning of user stack
    // TODO: also support architectures where stack grows downwards
    usize u_addr = stk->addr + 16 * PAGE_SIZE - total - sizeof(usize);
    usize k_addr = (usize) phys_to_virt((usize) stk->pages.tail << PAGE_SHIFT)
                 + (PAGE_SIZE << page_array[stk->pages.tail].order)
                 - total - sizeof(usize);
    u8 *  sp     = (u8 *) u_addr;

    // space for argv pointer array
    const char ** k_argv = (const char **) k_addr;
    u_addr += (num_argv + 1) * sizeof(const char *);
    k_addr += (num_argv + 1) * sizeof(const char *);

    // space for envp pointer array
    const char ** k_envp = (const char **) k_addr;
    u_addr += (num_envp + 1) * sizeof(const char *);
    k_addr += (num_envp + 1) * sizeof(const char *);

    // fill argv array and copy string
    for (int i = 0; i < num_argv; ++i) {
        k_argv[i] = (const char *) u_addr;
        strcpy((char *) k_addr, argv[i]);
        u_addr += strlen(argv[i]) + 1;
        k_addr += strlen(argv[i]) + 1;
    }
    k_argv[num_argv] = NULL;
    dbg_assert((u_addr - (usize) k_argv[0]) == (usize) len_argv);

    // fill envp array and copy string
    for (int i = 0; i < num_envp; ++i) {
        k_envp[i] = (const char *) u_addr;
        strcpy((char *) k_addr, envp[i]);
        u_addr += strlen(envp[i]) + 1;
        k_addr += strlen(envp[i]) + 1;
    }
    k_envp[num_envp] = NULL;
    dbg_assert((u_addr - (usize) k_envp[0]) == (usize) len_envp);

    // create new task, put it under the newly created process
    task_t * tid = task_create(filename, PRIORITY_NONRT, process_entry,
                               (void *) pid->entry, (void *) sp, k_argv, 0);
    dl_push_tail(&pid->tasks, &tid->dl_proc);
    regs_ctx_set(&tid->regs, pid->vm.ctx);
    tid->process = pid;
    tid->ustack  = stk;

    // start the new task
    task_resume(tid);
    return 0;
}

int do_open(const char * filename __UNUSED) {
    return 0;
}

int do_close(int fd __UNUSED) {
    return 0;
}

int do_read(int fd __UNUSED, const char * buf __UNUSED, size_t count __UNUSED) {
    return 0;
}

int do_write(int fd __UNUSED, const char * buf, size_t count __UNUSED) {
    dbg_print(buf);
    return 0;
}

int do_magic() {
    task_dump();
    return 0xdeadbeef;
}

//------------------------------------------------------------------------------
// fill system call table

__INIT void syscall_lib_init() {
    for (int i = 0; i < SYSCALL_NUM_COUNT; ++i) {
        syscall_tbl[i] = (syscall_proc_t) do_unsupported;
    }

    #define DEFINE_SYSCALL(id, name, ...)   \
        syscall_tbl[id] = (syscall_proc_t) do_ ## name;
    #include SYSCALL_DEF
    #undef DEFINE_SYSCALL
}

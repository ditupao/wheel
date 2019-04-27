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
static void process_entry(void * entry, void * sp, const char ** kp) {
    dbg_assert(((usize) sp % sizeof(usize)) == 0);
    dbg_print("new process, ip=%llx sp=%llx:\r\n", entry, sp);

    // const char ** argv = (const char **) sp;
    // while (*argv) {
    //     dbg_print("arg: %s.\r\n", *argv);
    //     ++argv;
    // }
    // ++argv;
    // while (*argv) {
    //     dbg_print("env: %s.\r\n", *argv);
    //     ++argv;
    // }

    for (int i = 0; kp[i]; ++i) {
        dbg_print("kernel space argv: %s.\r\n", kp[i]);
    }
    for (int i = 1; kp[i]; ++i) {
        dbg_print("kernel space envp: %s.\r\n", kp[i]);
    }

    return_to_user((usize) entry, (usize) sp);
}

//------------------------------------------------------------------------------
// system call handler functions

int do_unsupported() {
    dbg_print("unsupported syscall.\r\n");
    return 0;
}

int do_exit(int exitcode) {
    task_t * self = thiscpu_var(tid_prev);
    self->ret_val = exitcode;
    task_exit();

    dbg_print("[panic] task_exit() returned!\r\n");
    return 0;
}

int do_wait(int subpid __UNUSED) {
    // TODO
    return 0;
}

int do_spawn_thread(void * entry) {
    task_t * cur = thiscpu_var(tid_prev);
    task_t * tid = task_create(cur->priority, 0, thread_entry, entry, 0,0,0);
    task_resume(tid);
    return 0;
}

extern u8 _ramfs_addr;

int do_spawn_process(const char * filename,
                     const char * argv[],
                     const char * envp[]) {
    // filename, argv and envp are not accessible from the new vmspace
    // so we need to copy them to kernel memory first
    // copy filename to kernel stack, copy argv and envp to user stack

    // validate parameters
    if ((NULL == filename) ||
        (NULL == argv)     ||
        (NULL == envp)) {
        return 0;
    }

    // copy filename to kernel space (on stack)
    int  len_filename = strlen(filename);
    char bak_filename[len_filename + 1];
    memcpy(bak_filename, filename, len_filename + 1);

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
    int total = ROUND_UP((len_argv + len_envp) * sizeof(char), sizeof(usize))
              + (num_argv + num_envp + 2) * sizeof(char *);
    if (total > PAGE_SIZE) {
        return -1;
    }

    // copy argv[] to kernel space (on stack)
    char * bak_argv[num_argv + 1];          // includes trailing NULL
    char   buf_argv[len_argv];
    int    off_argv = 0;
    for (int i = 0; i < num_argv; ++i) {
        bak_argv[i] = &buf_argv[off_argv];
        off_argv += strlen(argv[i]) + 1;
        strcpy(bak_argv[i], argv[i]);
    }
    dbg_assert(off_argv == len_argv);
    bak_argv[num_argv] = NULL;

    // copy envp[] to kernel space (on stack)
    char * bak_envp[num_envp + 1];          // includes trailing NULL
    char   buf_envp[len_envp];
    int    off_envp = 0;
    for (int i = 0; i < num_envp; ++i) {
        bak_envp[i] = &buf_envp[off_envp];
        off_envp += strlen(envp[i]) + 1;
        strcpy(bak_envp[i], envp[i]);
    }
    dbg_assert(off_envp == len_envp);
    bak_envp[num_envp] = NULL;

    // for (int i = 0; bak_argv[i]; ++i) {
    //     dbg_print("backed argv: %s.\r\n", bak_argv[i]);
    // }
    // for (int i = 0; bak_envp[i]; ++i) {
    //     dbg_print("backed envp: %s.\r\n", bak_envp[i]);
    // }

    // read file from file system
    u8  * bin_addr;
    usize bin_size;
    tar_find(&_ramfs_addr, bak_filename, &bin_addr, &bin_size);

    // return if not found
    if ((NULL == bin_addr) && (0 == bin_size)) {
        dbg_print("[spawn_process] %s not found!\r\n", bak_filename);
        return -2;
    }

    // create new process
    process_t * pid = process_create();

    // return if elf load failed
    if (OK != elf64_load(pid, bin_addr, bin_size)) {
        dbg_print("[spawn_process] %s load failed!\r\n", bak_filename);
        process_delete(pid);
        return -3;
    }

    // allocate pages for user-mode stack
    vmrange_t * stk = vmspace_alloc(&pid->vm, 16 * PAGE_SIZE);
    vmspace_map(&pid->vm, stk);

    // put argv and envp at the beginning of user stack
    // TODO: also support architectures where stack grows downwards
    u8 * u_addr = (u8 *) stk->addr + 16 * PAGE_SIZE - sizeof(usize);
    u8 * k_addr = (u8 *) phys_to_virt((usize) stk->pages.tail << PAGE_SHIFT)
                + (PAGE_SIZE << page_array[stk->pages.tail].order) - sizeof(usize);
    u_addr -= total;
    k_addr -= total;
    u8 * sp = u_addr;

    // space for argv pointer array
    // const char ** u_argv = (const char **) u_addr;
    const char ** k_argv = (const char **) k_addr;
    u_addr += (num_argv + 1) * sizeof(const char *);
    k_addr += (num_argv + 1) * sizeof(const char *);

    // space for envp pointer array
    // const char ** u_envp = (const char **) u_addr;
    const char ** k_envp = (const char **) k_addr;
    u_addr += (num_envp + 1) * sizeof(const char *);
    k_addr += (num_argv + 1) * sizeof(const char *);

    // fill argv array and copy string
    memcpy(k_addr, buf_argv, len_argv);
    for (int i = 0; i < num_argv; ++i) {
        dbg_print("copying argv %s.\r\n", k_addr);
        k_argv[i] = (const char *) u_addr;
        u_addr += strlen((const char *) k_addr) + 1;
        k_addr += strlen((const char *) k_addr) + 1;
    }
    k_argv[num_argv] = NULL;

    // fill envp array and copy string
    memcpy(k_addr, buf_envp, len_envp);
    for (int i = 0; i < num_envp; ++i) {
        dbg_print("copying envp %s.\r\n", k_addr);
        k_envp[i] = (const char *) u_addr;
        u_addr += strlen((const char *) k_addr) + 1;
        k_addr += strlen((const char *) k_addr) + 1;
    }
    k_envp[num_argv] = NULL;

    // create new task, put it under the newly created process
    task_t * tid = task_create(PRIORITY_NONRT, 0, process_entry,
                               (void *) pid->entry, (void *) sp, k_argv, 0);
    dl_push_tail(&pid->tasks, &tid->dl_proc);
    regs_ctx_set(&tid->regs, pid->vm.ctx);
    tid->process = pid;
    tid->ustack  = stk;

    // start the new task
    task_resume(tid);
    return 0;



    // // old process and context
    // task_t    * tid = thiscpu_var(tid_prev);
    // process_t * old = tid->process;

    // // create new process, temporarily switch to the new context
    // process_t * new = process_create();
    // regs_ctx_set(&tid->regs, new->vm.ctx);
    // mmu_ctx_set(new->vm.ctx);

    // // read file from file system
    // u8  * bin_addr;
    // usize bin_size;
    // tar_find(&_ramfs_addr, bak_filename, &bin_addr, &bin_size);

    // if ((NULL == bin_addr) && (0 == bin_size)) {
    //     // executable file not found
    //     dbg_print("[spawn_process] %s not found!\r\n", bak_filename);
    //     process_delete(new);
    //     regs_ctx_set(&tid->regs, old->vm.ctx);
    //     mmu_ctx_set(old->vm.ctx);
    //     return -2;
    // } else if (OK != elf64_load(new, bin_addr, bin_size)) {
    //     // elf load failed
    //     dbg_print("[spawn_process] %s load failed!\r\n", bak_filename);
    //     process_delete(new);
    //     regs_ctx_set(&tid->regs, old->vm.ctx);
    //     mmu_ctx_set(old->vm.ctx);
    //     return -3;
    // }
    
    // // allocate pages for user-mode stack
    // vmrange_t * stk = vmspace_alloc(&new->vm, 16 * PAGE_SIZE);
    // vmspace_map(&new->vm, stk);

    // // carve out space for argv and envp
    // u8 * sp = (u8 *) (stk->addr + 16 * PAGE_SIZE);
    // sp -= len_envp;

    // // create new task
    // // TODO: mark this new task as a subprocess of current one
    // task_t * root = task_create(tid->priority, 0, process_entry,
    //                             (void *) new->entry, (void *) sp, 0,0);
    // root->ustack = stk;

    // // switch back to old context
    // regs_ctx_set(&tid->regs, old->vm.ctx);
    // mmu_ctx_set(old->vm.ctx);

    // // start new task and return
    // task_resume(root);
    // return 0;
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

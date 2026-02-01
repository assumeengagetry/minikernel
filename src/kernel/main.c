/*
 * MicroKernel - Main Entry Point
 * 
 * This file contains the kernel's main initialization and entry point.
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/sched.h"
#include "../../kernel/include/mm.h"
#include "../../kernel/include/list.h"
#include "../../kernel/include/spinlock.h"
#include "../../kernel/include/shell.h"

/* Kernel version information */
#define KERNEL_VERSION "0.1.0"
#define KERNEL_NAME "MicroKernel"

/* Variadic argument support (freestanding implementation) */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)

/* Forward declarations */
static void kernel_init(void);
static struct task_struct *create_init_process(void);
int printk(const char *fmt, ...);
void console_write(const char *buffer, size_t len);
void serial_putc(char c);
static inline unsigned char inb(unsigned short port);
static inline void outb(unsigned short port, unsigned char value);
void panic(const char *fmt, ...);
static inline void halt(void);
static void timer_interrupt_handler(void);
static void keyboard_interrupt_handler(void);
static void handle_page_fault(unsigned long error_code);

/* External declarations for assembly functions */
extern void local_irq_disable(void);
extern void local_irq_enable(void);
extern unsigned long local_irq_save(void);
extern void local_irq_restore(unsigned long flags);
extern u32 smp_processor_id(void);
extern void cpu_relax(void);
extern void local_bh_enable(void);
extern void local_bh_disable(void);

/* Weak stubs for functions not yet implemented */
void __attribute__((weak)) mm_init(void) { }
void __attribute__((weak)) buddy_init(void) { }
void __attribute__((weak)) sched_init(void) { }
void __attribute__((weak)) ipc_init(void) { }
void __attribute__((weak)) vfs_init(void) { }
void __attribute__((weak)) net_init(void) { }
void __attribute__((weak)) driver_init(void) { }
void __attribute__((weak)) schedule(void) { }
void __attribute__((weak)) yield(void) { }
void __attribute__((weak)) sched_fork(struct task_struct *p) { (void)p; }
void __attribute__((weak)) wake_up_new_task(struct task_struct *p) { (void)p; }
void __attribute__((weak)) scheduler_tick(void) { }
void __attribute__((weak)) do_signal(void) { }
void __attribute__((weak)) run_timer_softirq(void) { }
void __attribute__((weak)) handle_keyboard_input(unsigned char scancode) { (void)scancode; }
void __attribute__((weak)) do_page_fault(unsigned long address, unsigned long error_code) 
    { (void)address; (void)error_code; }
long __attribute__((weak)) do_fork(unsigned long flags, unsigned long sp, unsigned long stack_size,
                                   int *parent_tidptr, int *child_tidptr)
    { (void)flags; (void)sp; (void)stack_size; (void)parent_tidptr; (void)child_tidptr; return -ENOSYS; }
void __attribute__((weak)) do_exit(long code) { (void)code; for(;;) halt(); }
long __attribute__((weak)) do_wait(pid_t pid, int *stat_addr, int options, struct rusage *ru)
    { (void)pid; (void)stat_addr; (void)options; (void)ru; return -ENOSYS; }
long __attribute__((weak)) do_kill(pid_t pid, int sig) { (void)pid; (void)sig; return -ENOSYS; }
long __attribute__((weak)) do_brk(unsigned long brk) { (void)brk; return -ENOSYS; }
long __attribute__((weak)) do_mmap(unsigned long addr, unsigned long len, unsigned long prot,
                                   unsigned long flags, unsigned long fd, unsigned long off)
    { (void)addr; (void)len; (void)prot; (void)flags; (void)fd; (void)off; return -ENOSYS; }
long __attribute__((weak)) do_munmap(unsigned long addr, size_t len)
    { (void)addr; (void)len; return -ENOSYS; }

/* NR_CPUS if not defined */
#ifndef NR_CPUS
#define NR_CPUS 8
#endif

/* Global state */
static bool kernel_initialized = false;
static struct task_struct *init_task = NULL;
struct task_struct *current_task = NULL;

/* System call numbers */
#define __NR_read       0
#define __NR_write      1
#define __NR_open       2
#define __NR_close      3
#define __NR_getpid     39
#define __NR_clone      56
#define __NR_fork       57
#define __NR_vfork      58
#define __NR_execve     59
#define __NR_exit       60
#define __NR_wait4      61
#define __NR_kill       62
#define __NR_uname      63
#define __NR_sched_yield 24
#define __NR_brk        12
#define __NR_mmap       9
#define __NR_munmap     11
#define __NR_sysinfo    99

#define NR_syscalls     256

/*
 * I/O port operations
 */
static inline unsigned char inb(unsigned short port)
{
    unsigned char result;
    __asm__ __volatile__("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

/*
 * CPU control
 */
static inline void halt(void)
{
    __asm__ __volatile__("hlt");
}

/*
 * Serial port output
 */
#define SERIAL_PORT 0x3F8

void serial_putc(char c)
{
    /* Wait for transmit buffer to be empty */
    while (!(inb(SERIAL_PORT + 5) & 0x20)) {
        /* spin */
    }
    outb(SERIAL_PORT, c);
}

/*
 * Console output
 */
void console_write(const char *buffer, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        serial_putc(buffer[i]);
    }
}

/*
 * Simple number to string conversion
 */
static int int_to_str(char *buf, long value, int base, int uppercase)
{
    static const char *digits_lower = "0123456789abcdef";
    static const char *digits_upper = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;
    char tmp[32];
    int i = 0;
    int negative = 0;
    unsigned long uval;
    
    if (value < 0 && base == 10) {
        negative = 1;
        uval = -value;
    } else {
        uval = (unsigned long)value;
    }
    
    if (uval == 0) {
        tmp[i++] = '0';
    } else {
        while (uval > 0) {
            tmp[i++] = digits[uval % base];
            uval /= base;
        }
    }
    
    int len = 0;
    if (negative) {
        buf[len++] = '-';
    }
    
    while (i > 0) {
        buf[len++] = tmp[--i];
    }
    
    buf[len] = '\0';
    return len;
}

static int uint_to_str(char *buf, unsigned long value, int base, int uppercase)
{
    static const char *digits_lower = "0123456789abcdef";
    static const char *digits_upper = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;
    char tmp[32];
    int i = 0;
    
    if (value == 0) {
        tmp[i++] = '0';
    } else {
        while (value > 0) {
            tmp[i++] = digits[value % base];
            value /= base;
        }
    }
    
    int len = 0;
    while (i > 0) {
        buf[len++] = tmp[--i];
    }
    
    buf[len] = '\0';
    return len;
}

/*
 * Simple vsnprintf implementation
 */
static int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    char *str = buf;
    char *end = buf + size - 1;
    char numbuf[32];
    
    if (size == 0)
        return 0;
    
    while (*fmt && str < end) {
        if (*fmt != '%') {
            *str++ = *fmt++;
            continue;
        }
        
        fmt++; /* skip '%' */
        
        /* Handle format specifiers */
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(args, const char *);
            if (s == NULL) s = "(null)";
            while (*s && str < end)
                *str++ = *s++;
            break;
        }
        case 'd':
        case 'i': {
            long val = va_arg(args, long);
            int len = int_to_str(numbuf, val, 10, 0);
            for (int i = 0; i < len && str < end; i++)
                *str++ = numbuf[i];
            break;
        }
        case 'u': {
            unsigned long val = va_arg(args, unsigned long);
            int len = uint_to_str(numbuf, val, 10, 0);
            for (int i = 0; i < len && str < end; i++)
                *str++ = numbuf[i];
            break;
        }
        case 'x': {
            unsigned long val = va_arg(args, unsigned long);
            int len = uint_to_str(numbuf, val, 16, 0);
            for (int i = 0; i < len && str < end; i++)
                *str++ = numbuf[i];
            break;
        }
        case 'X': {
            unsigned long val = va_arg(args, unsigned long);
            int len = uint_to_str(numbuf, val, 16, 1);
            for (int i = 0; i < len && str < end; i++)
                *str++ = numbuf[i];
            break;
        }
        case 'p': {
            unsigned long val = (unsigned long)va_arg(args, void *);
            if (str < end) *str++ = '0';
            if (str < end) *str++ = 'x';
            int len = uint_to_str(numbuf, val, 16, 0);
            for (int i = 0; i < len && str < end; i++)
                *str++ = numbuf[i];
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            if (str < end)
                *str++ = c;
            break;
        }
        case 'l':
            fmt++;
            if (*fmt == 'u' || *fmt == 'd' || *fmt == 'x') {
                unsigned long val = va_arg(args, unsigned long);
                int len;
                if (*fmt == 'd') {
                    len = int_to_str(numbuf, (long)val, 10, 0);
                } else if (*fmt == 'x') {
                    len = uint_to_str(numbuf, val, 16, 0);
                } else {
                    len = uint_to_str(numbuf, val, 10, 0);
                }
                for (int i = 0; i < len && str < end; i++)
                    *str++ = numbuf[i];
            }
            break;
        case '%':
            if (str < end)
                *str++ = '%';
            break;
        default:
            if (str < end)
                *str++ = '%';
            if (str < end && *fmt)
                *str++ = *fmt;
            break;
        }
        
        if (*fmt)
            fmt++;
    }
    
    *str = '\0';
    return str - buf;
}

/*
 * Kernel print function
 */
int printk(const char *fmt, ...)
{
    va_list args;
    char buffer[1024];
    int len;

    va_start(args, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    console_write(buffer, len);

    return len;
}

/*
 * Kernel panic - halt the system
 */
void panic(const char *fmt, ...)
{
    va_list args;
    char buffer[1024];

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    printk("KERNEL PANIC: %s\n", buffer);

    local_irq_disable();

    for (;;) {
        halt();
    }
}

/*
 * String functions
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

char *strcpy(char *dest, const char *src)
{
    char *ret = dest;
    while ((*dest++ = *src++))
        ;
    return ret;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++)
        len++;
    return len;
}

/*
 * Jiffies counter
 */
static u64 jiffies_counter = 0;

u64 get_jiffies_64(void)
{
    return jiffies_counter;
}

static void update_jiffies_internal(void)
{
    jiffies_counter++;
}

/*
 * CPU identification
 */
u32 smp_processor_id(void)
{
    return 0; /* Single CPU for now */
}

/* cpu_relax is defined in switch.S, use inline version here */
static inline void cpu_relax_inline(void)
{
    __asm__ __volatile__("pause" ::: "memory");
}

/*
 * Interrupt control
 */
unsigned long local_irq_save(void)
{
    unsigned long flags;
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

void local_irq_restore(unsigned long flags)
{
    __asm__ __volatile__("pushq %0; popfq" :: "r"(flags) : "memory");
}

void local_irq_disable(void)
{
    __asm__ __volatile__("cli" ::: "memory");
}

void local_irq_enable(void)
{
    __asm__ __volatile__("sti" ::: "memory");
}

void local_bh_disable(void) { }
void local_bh_enable(void) { }

/*
 * Memory allocation stubs (simple version)
 */
struct task_struct *alloc_task_struct(void)
{
    /* For now, return NULL - proper implementation needs working memory allocator */
    static struct task_struct init_task_storage;
    static int allocated = 0;
    
    if (!allocated) {
        allocated = 1;
        memset(&init_task_storage, 0, sizeof(init_task_storage));
        return &init_task_storage;
    }
    
    return NULL;
}

void free_task_struct(struct task_struct *tsk)
{
    (void)tsk;
}

struct mm_struct *mm_alloc(void)
{
    static struct mm_struct init_mm_storage;
    static int allocated = 0;
    
    if (!allocated) {
        allocated = 1;
        memset(&init_mm_storage, 0, sizeof(init_mm_storage));
        return &init_mm_storage;
    }
    
    return NULL;
}

/* set_current is defined as macro in sched.h, use directly */

/*
 * Create the init process (PID 1)
 */
static struct task_struct *create_init_process(void)
{
    struct task_struct *task;
    struct mm_struct *mm;

    task = alloc_task_struct();
    if (!task) {
        panic("Cannot allocate init task");
    }

    mm = mm_alloc();
    if (!mm) {
        free_task_struct(task);
        panic("Cannot allocate init mm");
    }

    /* Process IDs */
    task->pid = 1;
    task->tgid = 1;
    task->ppid = 0;
    task->pgrp = 1;
    task->session = 1;

    /* User/Group IDs (root) */
    task->uid = 0;
    task->gid = 0;
    task->euid = 0;
    task->egid = 0;
    task->suid = 0;
    task->sgid = 0;
    task->fsuid = 0;
    task->fsgid = 0;

    /* Process name */
    strcpy(task->comm, "init");

    /* Memory management */
    task->mm = mm;
    task->active_mm = mm;

    /* Scheduling */
    task->state = TASK_RUNNING;
    task->prio = DEFAULT_PRIO;
    task->static_prio = DEFAULT_PRIO;
    task->normal_prio = DEFAULT_PRIO;
    task->policy = SCHED_NORMAL;

    /* CPU affinity */
    task->cpus_allowed = (1UL << NR_CPUS) - 1;
    task->nr_cpus_allowed = NR_CPUS;

    /* Process relationships - init is its own parent */
    task->real_parent = task;
    task->parent = task;
    task->group_leader = task;

    /* Timing */
    task->start_time = get_jiffies_64();
    task->real_start_time = task->start_time;

    /* Initialize scheduler data */
    sched_fork(task);

    return task;
}

/*
 * Kernel initialization
 */
static void kernel_init(void)
{
    printk("Initializing %s %s\n", KERNEL_NAME, KERNEL_VERSION);

    /* Initialize memory management */
    printk("  Initializing memory management...\n");
    mm_init();
    buddy_init();

    /* Initialize scheduler */
    printk("  Initializing scheduler...\n");
    sched_init();

    /* Initialize IPC */
    printk("  Initializing IPC...\n");
    ipc_init();

    /* Initialize VFS */
    printk("  Initializing VFS...\n");
    vfs_init();

    /* Initialize networking */
    printk("  Initializing network...\n");
    net_init();

    /* Initialize drivers */
    printk("  Initializing drivers...\n");
    driver_init();

    /* Create init process */
    printk("  Creating init process...\n");
    init_task = create_init_process();

    /* Set as current task */
    set_current(init_task);

    /* Wake up init */
    wake_up_new_task(init_task);

    kernel_initialized = true;
    
    printk("Kernel initialization complete.\n");
}

/*
 * Kernel main entry point - called from assembly boot code
 */
void kernel_main(void)
{
    /* Initialize the kernel */
    kernel_init();

    /* Start the interactive shell */
    printk("Starting shell...\n");
    shell_run();

    /* Should never reach here */
    panic("Shell exited unexpectedly!");
}

/*
 * System call implementations
 */
long sys_getpid(void)
{
    return current_task ? current_task->pid : 0;
}

long sys_sched_yield(void)
{
    yield();
    return 0;
}

long sys_exit(int error_code)
{
    do_exit(error_code);
    return 0; /* Never reached */
}

long sys_fork(void)
{
    return do_fork(SIGCHLD, 0, 0, NULL, NULL);
}

long sys_vfork(void)
{
    return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, 0, 0, NULL, NULL);
}

long sys_clone(unsigned long clone_flags, unsigned long newsp,
               int __user *parent_tidptr, int __user *child_tidptr)
{
    return do_fork(clone_flags, newsp, 0, parent_tidptr, child_tidptr);
}

long sys_wait4(pid_t upid, int __user *stat_addr, int options,
               struct rusage __user *ru)
{
    return do_wait(upid, stat_addr, options, ru);
}

long sys_kill(pid_t pid, int sig)
{
    return do_kill(pid, sig);
}

long sys_brk(unsigned long brk)
{
    return do_brk(brk);
}

long sys_mmap(unsigned long addr, unsigned long len,
              unsigned long prot, unsigned long flags,
              unsigned long fd, unsigned long off)
{
    return do_mmap(addr, len, prot, flags, fd, off);
}

long sys_munmap(unsigned long addr, size_t len)
{
    return do_munmap(addr, len);
}

long sys_sysinfo(struct sysinfo __user *info)
{
    struct sysinfo val;
    
    memset(&val, 0, sizeof(val));
    si_meminfo(&val);
    si_swapinfo(&val);

    if (copy_to_user(info, &val, sizeof(struct sysinfo)))
        return -EFAULT;

    return 0;
}

long sys_uname(struct utsname __user *name)
{
    struct utsname kernel_info;

    strcpy(kernel_info.sysname, "MicroKernel");
    strcpy(kernel_info.nodename, "localhost");
    strcpy(kernel_info.release, KERNEL_VERSION);
    strcpy(kernel_info.version, "1");
    strcpy(kernel_info.machine, "x86_64");
    strcpy(kernel_info.domainname, "localdomain");

    if (copy_to_user(name, &kernel_info, sizeof(struct utsname)))
        return -EFAULT;

    return 0;
}

long sys_read(unsigned int fd, char __user *buf, size_t count)
{
    (void)fd; (void)buf; (void)count;
    return -ENOSYS;
}

long sys_write(unsigned int fd, const char __user *buf, size_t count)
{
    (void)fd; (void)buf; (void)count;
    return -ENOSYS;
}

long sys_open(const char __user *filename, int flags, umode_t mode)
{
    (void)filename; (void)flags; (void)mode;
    return -ENOSYS;
}

long sys_close(unsigned int fd)
{
    (void)fd;
    return -ENOSYS;
}

long sys_execve(const char __user *filename,
                const char __user *const __user *argv,
                const char __user *const __user *envp)
{
    (void)filename; (void)argv; (void)envp;
    return -ENOSYS;
}

/*
 * System call dispatcher
 */
long do_syscall(unsigned long syscall_nr, unsigned long arg0,
                unsigned long arg1, unsigned long arg2,
                unsigned long arg3, unsigned long arg4,
                unsigned long arg5)
{
    (void)arg5; /* Unused for most syscalls */
    
    if (syscall_nr >= NR_syscalls) {
        printk("Invalid syscall number: %lu\n", syscall_nr);
        return -ENOSYS;
    }

    switch (syscall_nr) {
    case __NR_read:
        return sys_read((unsigned int)arg0, (char __user *)arg1, (size_t)arg2);
    case __NR_write:
        return sys_write((unsigned int)arg0, (const char __user *)arg1, (size_t)arg2);
    case __NR_open:
        return sys_open((const char __user *)arg0, (int)arg1, (umode_t)arg2);
    case __NR_close:
        return sys_close((unsigned int)arg0);
    case __NR_getpid:
        return sys_getpid();
    case __NR_clone:
        return sys_clone(arg0, arg1, (int __user *)arg2, (int __user *)arg3);
    case __NR_fork:
        return sys_fork();
    case __NR_vfork:
        return sys_vfork();
    case __NR_execve:
        return sys_execve((const char __user *)arg0,
                         (const char __user *const __user *)arg1,
                         (const char __user *const __user *)arg2);
    case __NR_exit:
        return sys_exit((int)arg0);
    case __NR_wait4:
        return sys_wait4((pid_t)arg0, (int __user *)arg1, (int)arg2,
                        (struct rusage __user *)arg3);
    case __NR_kill:
        return sys_kill((pid_t)arg0, (int)arg1);
    case __NR_sched_yield:
        return sys_sched_yield();
    case __NR_brk:
        return sys_brk(arg0);
    case __NR_mmap:
        return sys_mmap(arg0, arg1, arg2, arg3, arg4, arg5);
    case __NR_munmap:
        return sys_munmap(arg0, (size_t)arg1);
    case __NR_sysinfo:
        return sys_sysinfo((struct sysinfo __user *)arg0);
    case __NR_uname:
        return sys_uname((struct utsname __user *)arg0);
    default:
        printk("Unimplemented syscall: %lu\n", syscall_nr);
        return -ENOSYS;
    }
}

/*
 * Interrupt handlers
 */
void handle_interrupt(int irq)
{
    printk("IRQ %d received\n", irq);

    switch (irq) {
    case 0:
        timer_interrupt_handler();
        break;
    case 1:
        keyboard_interrupt_handler();
        break;
    default:
        printk("Unknown interrupt: %d\n", irq);
        break;
    }
}

void handle_exception(int exception, unsigned long error_code)
{
    printk("Exception %d (error code: 0x%lx)\n", exception, error_code);

    switch (exception) {
    case 0:
        printk("Division by zero\n");
        break;
    case 6:
        printk("Invalid instruction\n");
        break;
    case 13:
        printk("General protection fault\n");
        break;
    case 14:
        handle_page_fault(error_code);
        return;
    default:
        printk("Unknown exception: %d\n", exception);
        break;
    }

    panic("Unhandled exception in kernel");
}

static void timer_interrupt_handler(void)
{
    update_jiffies_internal();
    scheduler_tick();
    run_timer_softirq();
}

static void keyboard_interrupt_handler(void)
{
    unsigned char scancode = inb(0x60);
    handle_keyboard_input(scancode);
}

static void handle_page_fault(unsigned long error_code)
{
    unsigned long address;

    __asm__ __volatile__("movq %%cr2, %0" : "=r"(address));

    printk("Page fault at address 0x%lx, error code: 0x%lx\n",
           address, error_code);

    do_page_fault(address, error_code);
}

/*
 * Kernel entry point from assembly (alternate name)
 */
void start_kernel(void)
{
    kernel_main();
}
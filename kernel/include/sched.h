#ifndef SCHED_H
#define SCHED_H

#include "types.h"
#include "list.h"
#include "spinlock.h"

/*
 * Task states
 */
#define TASK_RUNNING            0x0000
#define TASK_INTERRUPTIBLE      0x0001
#define TASK_UNINTERRUPTIBLE    0x0002
#define TASK_STOPPED            0x0004
#define TASK_TRACED             0x0008
#define TASK_ZOMBIE             0x0010
#define TASK_DEAD               0x0020
#define TASK_WAKEKILL           0x0040
#define TASK_WAKING             0x0080
#define TASK_PARKED             0x0100
#define TASK_NEW                0x0200

/*
 * Scheduling priorities
 */
#define MAX_NICE        19
#define MIN_NICE        (-20)
#define NICE_WIDTH      (MAX_NICE - MIN_NICE + 1)
#define MAX_USER_RT_PRIO 100
#define MAX_RT_PRIO     MAX_USER_RT_PRIO
#define MAX_PRIO        (MAX_RT_PRIO + NICE_WIDTH)
#define DEFAULT_PRIO    (MAX_RT_PRIO + NICE_WIDTH / 2)
#define NICE_0_LOAD     1024
#define NICE_0_SHIFT    10

/*
 * Scheduling policies
 */
#define SCHED_NORMAL    0
#define SCHED_FIFO      1
#define SCHED_RR        2
#define SCHED_BATCH     3
#define SCHED_IDLE      5
#define SCHED_DEADLINE  6

/*
 * Process flags
 */
#define PF_KTHREAD              0x00000001
#define PF_IDLE                 0x00000002
#define PF_EXITING              0x00000004
#define PF_EXITPIDONE           0x00000008
#define PF_VCPU                 0x00000010
#define PF_WQ_WORKER            0x00000020
#define PF_FORKNOEXEC           0x00000040
#define PF_MCE_PROCESS          0x00000080
#define PF_SUPERPRIV            0x00000100
#define PF_DUMPCORE             0x00000200
#define PF_SIGNALED             0x00000400
#define PF_MEMALLOC             0x00000800
#define PF_NPROC_EXCEEDED       0x00001000
#define PF_USED_MATH            0x00002000
#define PF_USED_ASYNC           0x00004000
#define PF_NOFREEZE             0x00008000
#define PF_FROZEN               0x00010000
#define PF_FREEZER_SKIP         0x00020000

/*
 * Clone flags
 */
#define CLONE_VM                0x00000100
#define CLONE_FS                0x00000200
#define CLONE_FILES             0x00000400
#define CLONE_SIGHAND           0x00000800
#define CLONE_PTRACE            0x00002000
#define CLONE_VFORK             0x00004000
#define CLONE_PARENT            0x00008000
#define CLONE_THREAD            0x00010000
#define CLONE_NEWNS             0x00020000
#define CLONE_SYSVSEM           0x00040000
#define CLONE_SETTLS            0x00080000
#define CLONE_PARENT_SETTID     0x00100000
#define CLONE_CHILD_CLEARTID    0x00200200
#define CLONE_DETACHED          0x00400000
#define CLONE_UNTRACED          0x00800000
#define CLONE_CHILD_SETTID      0x01000000
#define CLONE_NEWCGROUP         0x02000000
#define CLONE_NEWUTS            0x04000000
#define CLONE_NEWIPC            0x08000000
#define CLONE_NEWUSER           0x10000000
#define CLONE_NEWPID            0x20000000
#define CLONE_NEWNET            0x40000000
#define CLONE_IO                0x80000000

/* Spinlock is defined in spinlock.h */

/*
 * Load weight for scheduling
 */
struct load_weight {
    unsigned long weight;
    u32 inv_weight;
};

/*
 * Scheduling entity
 */
struct sched_entity {
    struct load_weight load;
    struct rb_node run_node;
    struct list_head group_node;
    unsigned int on_rq;

    u64 exec_start;
    u64 sum_exec_runtime;
    u64 vruntime;
    u64 prev_sum_exec_runtime;
    u64 nr_migrations;

    /* Statistics */
    u64 wait_start;
    u64 wait_max;
    u64 wait_count;
    u64 wait_sum;
    u64 iowait_count;
    u64 iowait_sum;

    u64 slice_max;
    u64 run_max;
};

/*
 * Real-time scheduling entity
 */
struct sched_rt_entity {
    struct list_head run_list;
    unsigned long timeout;
    unsigned long watchdog_stamp;
    unsigned int time_slice;
    unsigned short on_rq;
    unsigned short on_list;

    struct sched_rt_entity *back;
    struct sched_rt_entity *parent;
};

/*
 * Resource limits
 */
struct rlimit {
    unsigned long rlim_cur;
    unsigned long rlim_max;
};

#define RLIMIT_CPU      0
#define RLIMIT_FSIZE    1
#define RLIMIT_DATA     2
#define RLIMIT_STACK    3
#define RLIMIT_CORE     4
#define RLIMIT_RSS      5
#define RLIMIT_NPROC    6
#define RLIMIT_NOFILE   7
#define RLIMIT_MEMLOCK  8
#define RLIMIT_AS       9
#define RLIMIT_LOCKS    10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE 12
#define RLIMIT_NICE     13
#define RLIMIT_RTPRIO   14
#define RLIMIT_RTTIME   15
#define RLIM_NLIMITS    16

/*
 * Signal action
 */
struct k_sigaction {
    void (*sa_handler)(int);
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    sigset_t sa_mask;
};

/*
 * Signal pending
 */
struct sigpending {
    struct list_head list;
    sigset_t signal;
};

/*
 * Signal structure
 */
struct signal_struct {
    u32 count;
    u32 live;
    struct k_sigaction action[64];
    spinlock_t siglock;
    
    pid_t pgrp;
    pid_t session;
    pid_t tty_old_pgrp;
    
    u64 utime;
    u64 stime;
    u64 cutime;
    u64 cstime;
    
    struct rlimit rlim[RLIM_NLIMITS];
};

/*
 * Forward declarations
 */
struct vfsmount;
struct dentry;
struct file;

/*
 * Path structure
 */
struct path {
    struct vfsmount *mnt;
    struct dentry *dentry;
};

/*
 * Filesystem info
 */
struct fs_struct {
    int users;
    spinlock_t lock;
    int umask;
    int in_exec;
    struct path root;
    struct path pwd;
};

/*
 * Open files structure
 */
struct files_struct {
    u32 count;
    spinlock_t file_lock;
    u32 next_fd;
    u32 max_fds;
    struct file **fdt;
    struct file *fd_array[32];
};

/*
 * Memory management structure
 */
struct mm_struct {
    struct list_head mmap_list;
    struct rb_root mm_rb;
    u32 map_count;
    spinlock_t page_table_lock;
    spinlock_t mmap_lock;

    unsigned long mmap_base;
    unsigned long task_size;
    unsigned long highest_vm_end;

    phys_addr_t pgd;

    u32 mm_users;
    u32 mm_count;

    unsigned long total_vm;
    unsigned long locked_vm;
    unsigned long pinned_vm;
    unsigned long data_vm;
    unsigned long exec_vm;
    unsigned long stack_vm;

    unsigned long start_code, end_code;
    unsigned long start_data, end_data;
    unsigned long start_brk, brk;
    unsigned long start_stack;
    unsigned long arg_start, arg_end;
    unsigned long env_start, env_end;
};

/*
 * CPU registers (x86_64)
 */
struct pt_regs {
    unsigned long r15;
    unsigned long r14;
    unsigned long r13;
    unsigned long r12;
    unsigned long r11;
    unsigned long r10;
    unsigned long r9;
    unsigned long r8;
    unsigned long rbp;
    unsigned long rdi;
    unsigned long rsi;
    unsigned long rdx;
    unsigned long rcx;
    unsigned long rbx;
    unsigned long rax;

    /* Exception/interrupt frame */
    unsigned long orig_rax;
    unsigned long rip;
    unsigned long cs;
    unsigned long eflags;
    unsigned long rsp;
    unsigned long ss;
};

/*
 * Thread structure
 */
struct thread_struct {
    unsigned long sp;
    unsigned long ip;
    unsigned long fs;
    unsigned long gs;
    unsigned long cr2;
    unsigned long trap_nr;
    unsigned long error_code;
};

/*
 * Task structure - the main process/thread descriptor
 */
struct task_struct {
    /* Scheduler state */
    volatile long state;
    void *stack;
    u32 flags;
    u32 ptrace;

    /* Process IDs */
    pid_t pid;
    pid_t tgid;
    pid_t ppid;
    pid_t pgrp;
    pid_t session;

    /* User/Group IDs */
    uid_t uid, euid, suid, fsuid;
    gid_t gid, egid, sgid, fsgid;

    /* Scheduling */
    int prio;
    int static_prio;
    int normal_prio;
    unsigned int rt_priority;
    unsigned int policy;
    
    struct sched_entity se;
    struct sched_rt_entity rt;

    u64 utime;
    u64 stime;
    u64 start_time;
    u64 real_start_time;

    /* CPU affinity */
    unsigned long cpus_allowed;
    int nr_cpus_allowed;
    int on_cpu;
    int recent_used_cpu;

    /* Process relationships */
    struct task_struct *real_parent;
    struct task_struct *parent;
    struct list_head children;
    struct list_head sibling;
    struct task_struct *group_leader;

    /* Task lists */
    struct list_head tasks;
    struct list_head run_list;

    /* Memory management */
    struct mm_struct *mm;
    struct mm_struct *active_mm;

    /* Filesystem */
    struct fs_struct *fs;
    struct files_struct *files;

    /* Signals */
    struct signal_struct *signal;
    struct sigpending pending;
    sigset_t blocked;
    sigset_t real_blocked;

    /* Thread info */
    struct thread_struct thread;
    struct pt_regs *regs;

    /* Exit info */
    int exit_state;
    int exit_code;
    int exit_signal;

    /* Process name */
    char comm[16];

    /* Accounting */
    unsigned long min_flt;
    unsigned long maj_flt;
    unsigned long nvcsw;
    unsigned long nivcsw;

    /* Thread function (for kernel threads) */
    int (*thread_fn)(void *);
    void *thread_data;
};

/*
 * Wait queue
 */
typedef struct wait_queue_head {
    spinlock_t lock;
    struct list_head head;
} wait_queue_head_t;

typedef struct wait_queue_entry {
    unsigned int flags;
    void *private;
    int (*func)(struct wait_queue_entry *wq, unsigned mode, int flags, void *key);
    struct list_head entry;
} wait_queue_entry_t;

#define INIT_WAIT_QUEUE_HEAD(wq) do {           \
    spin_lock_init(&(wq)->lock);                \
    INIT_LIST_HEAD(&(wq)->head);                \
} while (0)

/*
 * Current task pointer
 */
extern struct task_struct *current_task;
#define current (current_task)

#define get_current() (current_task)
#define set_current(task) do { current_task = (task); } while (0)

/*
 * Task state helpers
 */
#define task_is_running(task)   ((task)->state == TASK_RUNNING)
#define task_is_stopped(task)   ((task)->state & TASK_STOPPED)
#define task_is_traced(task)    ((task)->state & TASK_TRACED)

/*
 * Function declarations
 */

/* Scheduler initialization */
void sched_init(void);
void scheduler_tick(void);
void schedule(void);
void yield(void);

/* Task management */
struct task_struct *alloc_task_struct(void);
void free_task_struct(struct task_struct *task);
struct task_struct *dup_task_struct(struct task_struct *orig);

/* Task state */
void wake_up_process(struct task_struct *task);
void wake_up_new_task(struct task_struct *task);
void set_task_state(struct task_struct *task, long state);

/* Fork/Clone */
long do_fork(unsigned long clone_flags, unsigned long stack_start,
             unsigned long stack_size, int __user *parent_tidptr,
             int __user *child_tidptr);
void sched_fork(struct task_struct *task);

/* Exit */
void do_exit(long code);
long do_wait(pid_t pid, int __user *stat_addr, int options,
             struct rusage __user *ru);

/* Signals */
long do_kill(pid_t pid, int sig);

/* Memory */
long do_brk(unsigned long brk);
long do_mmap(unsigned long addr, unsigned long len, unsigned long prot,
             unsigned long flags, unsigned long fd, unsigned long offset);
long do_munmap(unsigned long addr, size_t len);

/* Memory management */
struct mm_struct *mm_alloc(void);
void mm_free(struct mm_struct *mm);

/* Context switch (assembly) */
extern void switch_to(struct task_struct *prev, struct task_struct *next);
extern void ret_from_fork(void);

/* Run queue */
struct rq {
    spinlock_t lock;
    unsigned int nr_running;
    u64 nr_switches;
    u64 clock;
    u64 clock_task;

    struct task_struct *curr;
    struct task_struct *idle;

    struct list_head cfs_tasks;

    int cpu;
    int online;
};

/* Per-CPU run queue */
extern struct rq runqueues[];
#define cpu_rq(cpu) (&runqueues[(cpu)])
#define this_rq()   cpu_rq(0)

/* Scheduler classes */
extern const struct sched_class fair_sched_class;
extern const struct sched_class rt_sched_class;
extern const struct sched_class idle_sched_class;

struct sched_class {
    const struct sched_class *next;

    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)(struct rq *rq);

    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);

    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    void (*set_next_task)(struct rq *rq, struct task_struct *p);

    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)(struct task_struct *p);
    void (*task_dead)(struct task_struct *p);
};

#endif /* SCHED_H */
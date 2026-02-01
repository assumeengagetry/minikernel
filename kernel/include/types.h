#ifndef TYPES_H
#define TYPES_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef void *ptr_t;
typedef u64 phys_addr_t;
typedef u64 virt_addr_t;

typedef s32 pid_t;
typedef u32 uid_t;
typedef u32 gid_t;
typedef u16 umode_t;
typedef u32 gfp_t;

typedef u64 ino_t;
typedef s64 off_t;
typedef u64 dev_t;
typedef u32 mode_t;
typedef u32 nlink_t;
typedef u64 size_t;
typedef s64 ssize_t;
typedef s64 time_t;
typedef u64 sigset_t;

typedef enum {
    false = 0,
    true = 1
} bool;

#define NULL ((void *)0)

#define ENOSYS      38
#define EFAULT      14
#define EINVAL      22
#define ENOMEM      12
#define ENOENT      2
#define EACCES      13
#define EEXIST      17
#define EBUSY       16
#define EAGAIN      11
#define EINTR       4
#define EIO         5
#define EPERM       1
#define ESRCH       3
#define ECHILD      10
#define EDEADLK     35
#define ENOMSG      42
#define EIDRM       43
#define ENOSPC      28
#define ENODEV      19
#define ENOTDIR     20
#define EISDIR      21
#define EMFILE      24
#define ENFILE      23
#define ENOTTY      25
#define ETXTBSY     26
#define EFBIG       27
#define ENOTEMPTY   39
#define ENAMETOOLONG 36
#define ELOOP       40
#define ENOTSOCK    88
#define EADDRINUSE  98
#define ECONNREFUSED 111
#define ENETUNREACH 101
#define ETIMEDOUT   110

#define SIGCHLD     17

/* PAGE_SIZE, PAGE_SHIFT, PAGE_MASK are defined in mm.h */

#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) (MIN(hi, MAX(lo, x)))

#define BIT(n) (1UL << (n))
#define SET_BIT(x, n) ((x) |= BIT(n))
#define CLEAR_BIT(x, n) ((x) &= ~BIT(n))
#define TEST_BIT(x, n) (((x) & BIT(n)) != 0)

#define mb()  __asm__ __volatile__("mfence" ::: "memory")
#define rmb() __asm__ __volatile__("lfence" ::: "memory")
#define wmb() __asm__ __volatile__("sfence" ::: "memory")

#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))
#define __section(x) __attribute__((section(x)))
#define __noreturn __attribute__((noreturn))
#define __weak __attribute__((weak))
#define __init __attribute__((section(".init.text")))
#define __initdata __attribute__((section(".init.data")))
#define __user

#define offsetof(type, member) ((size_t)&((type *)0)->member)

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

#define KERNEL_VIRTUAL_BASE 0xFFFF800000000000UL
#define USER_VIRTUAL_BASE   0x0000000000000000UL
#define USER_VIRTUAL_END    0x0000800000000000UL

#define __pa(x) ((phys_addr_t)(x) - KERNEL_VIRTUAL_BASE)
#define __va(x) ((void *)((phys_addr_t)(x) + KERNEL_VIRTUAL_BASE))

/* File mode bits */
#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_IRWXU  0000700
#define S_IRUSR  0000400
#define S_IWUSR  0000200
#define S_IXUSR  0000100

#define S_IRWXG  0000070
#define S_IRGRP  0000040
#define S_IWGRP  0000020
#define S_IXGRP  0000010

#define S_IRWXO  0000007
#define S_IROTH  0000004
#define S_IWOTH  0000002
#define S_IXOTH  0000001

#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* Scheduling policies and NR_CPUS are defined in sched.h */

/* Forward declarations for commonly used structs */
struct rusage;
struct sysinfo;
struct utsname;

#endif /* TYPES_H */
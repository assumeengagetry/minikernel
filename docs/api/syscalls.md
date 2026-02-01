# 系统调用 API 参考

## 目录

1. [概述](#1-概述)
2. [调用约定](#2-调用约定)
3. [文件操作](#3-文件操作)
4. [进程管理](#4-进程管理)
5. [内存管理](#5-内存管理)
6. [调度控制](#6-调度控制)
7. [系统信息](#7-系统信息)
8. [错误码参考](#8-错误码参考)

---

## 1. 概述

系统调用（System Call）是用户空间程序请求内核服务的标准接口。MiniKernel 使用 x86_64 `syscall` 指令实现系统调用，遵循 Linux/AMD64 ABI。

### 1.1 系统调用列表

| 编号 | 名称 | 描述 | 类别 |
|------|------|------|------|
| 0 | read | 从文件描述符读取 | 文件 |
| 1 | write | 写入文件描述符 | 文件 |
| 2 | open | 打开文件 | 文件 |
| 3 | close | 关闭文件描述符 | 文件 |
| 9 | mmap | 内存映射 | 内存 |
| 11 | munmap | 解除内存映射 | 内存 |
| 12 | brk | 调整堆边界 | 内存 |
| 24 | sched_yield | 让出 CPU | 调度 |
| 39 | getpid | 获取进程 ID | 进程 |
| 56 | clone | 创建进程/线程 | 进程 |
| 57 | fork | 创建子进程 | 进程 |
| 58 | vfork | 创建子进程（共享地址空间） | 进程 |
| 59 | execve | 执行程序 | 进程 |
| 60 | exit | 退出进程 | 进程 |
| 61 | wait4 | 等待子进程 | 进程 |
| 62 | kill | 发送信号 | 进程 |
| 63 | uname | 获取系统名称 | 系统 |
| 99 | sysinfo | 获取系统状态信息 | 系统 |

---

## 2. 调用约定

### 2.1 寄存器使用

MiniKernel 遵循 AMD64 System V ABI 系统调用约定：

| 寄存器 | 用途 |
|--------|------|
| RAX | 系统调用号（输入）/ 返回值（输出） |
| RDI | 第 1 个参数 |
| RSI | 第 2 个参数 |
| RDX | 第 3 个参数 |
| R10 | 第 4 个参数（注意：不是 RCX） |
| R8 | 第 5 个参数 |
| R9 | 第 6 个参数 |
| RCX | `syscall` 指令会覆盖（保存返回地址） |
| R11 | `syscall` 指令会覆盖（保存 RFLAGS） |

### 2.2 返回值

- **成功**：返回非负值（具体含义取决于系统调用）
- **失败**：返回负的错误码（如 `-EINVAL`）

### 2.3 汇编示例

```asm
# 示例：write(1, "Hello\n", 6)
mov $1, %rax        # 系统调用号：write
mov $1, %rdi        # fd = 1 (stdout)
lea message(%rip), %rsi  # buf = "Hello\n"
mov $6, %rdx        # count = 6
syscall             # 执行系统调用
# 返回值在 RAX 中
```

### 2.4 C 语言包装

```c
static inline long syscall0(long nr)
{
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall1(long nr, long arg1)
{
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall3(long nr, long arg1, long arg2, long arg3)
{
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "0"(nr), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}
```

---

## 3. 文件操作

### 3.1 read - 读取文件

从文件描述符读取数据到缓冲区。

**系统调用号**：0

**函数原型**：
```c
ssize_t read(int fd, void *buf, size_t count);
```

**参数**：

| 参数 | 描述 |
|------|------|
| fd | 文件描述符 |
| buf | 接收数据的缓冲区指针 |
| count | 要读取的最大字节数 |

**返回值**：
- 成功：返回实际读取的字节数（0 表示文件末尾）
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -EBADF | 无效的文件描述符 |
| -EFAULT | buf 指向无效地址 |
| -EINVAL | fd 不支持读取 |
| -EIO | I/O 错误 |

**示例**：
```c
char buffer[1024];
ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
if (bytes_read < 0) {
    // 处理错误
}
```

---

### 3.2 write - 写入文件

将缓冲区数据写入文件描述符。

**系统调用号**：1

**函数原型**：
```c
ssize_t write(int fd, const void *buf, size_t count);
```

**参数**：

| 参数 | 描述 |
|------|------|
| fd | 文件描述符 |
| buf | 要写入数据的缓冲区指针 |
| count | 要写入的字节数 |

**返回值**：
- 成功：返回实际写入的字节数
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -EBADF | 无效的文件描述符 |
| -EFAULT | buf 指向无效地址 |
| -EINVAL | fd 不支持写入 |
| -ENOSPC | 设备空间不足 |
| -EIO | I/O 错误 |

**示例**：
```c
const char *msg = "Hello, World!\n";
ssize_t bytes_written = write(1, msg, strlen(msg));  // 写入 stdout
```

---

### 3.3 open - 打开文件

打开或创建文件，返回文件描述符。

**系统调用号**：2

**函数原型**：
```c
int open(const char *pathname, int flags, mode_t mode);
```

**参数**：

| 参数 | 描述 |
|------|------|
| pathname | 文件路径名 |
| flags | 打开标志 |
| mode | 创建文件时的权限（仅当 O_CREAT 时使用） |

**flags 标志**：

| 标志 | 值 | 描述 |
|------|-----|------|
| O_RDONLY | 0x0000 | 只读 |
| O_WRONLY | 0x0001 | 只写 |
| O_RDWR | 0x0002 | 读写 |
| O_CREAT | 0x0040 | 不存在时创建 |
| O_EXCL | 0x0080 | 与 O_CREAT 一起使用，文件存在则失败 |
| O_TRUNC | 0x0200 | 截断文件 |
| O_APPEND | 0x0400 | 追加模式 |

**返回值**：
- 成功：返回新的文件描述符（非负整数）
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -ENOENT | 文件不存在 |
| -EACCES | 权限不足 |
| -EEXIST | 文件已存在（O_CREAT | O_EXCL） |
| -EMFILE | 进程打开文件数达到上限 |
| -ENFILE | 系统打开文件数达到上限 |

**示例**：
```c
int fd = open("/path/to/file", O_RDWR | O_CREAT, 0644);
if (fd < 0) {
    // 处理错误
}
```

---

### 3.4 close - 关闭文件

关闭文件描述符，释放相关资源。

**系统调用号**：3

**函数原型**：
```c
int close(int fd);
```

**参数**：

| 参数 | 描述 |
|------|------|
| fd | 要关闭的文件描述符 |

**返回值**：
- 成功：返回 0
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -EBADF | 无效的文件描述符 |
| -EIO | I/O 错误 |

**示例**：
```c
if (close(fd) < 0) {
    // 处理错误
}
```

---

## 4. 进程管理

### 4.1 getpid - 获取进程 ID

返回当前进程的进程 ID。

**系统调用号**：39

**函数原型**：
```c
pid_t getpid(void);
```

**参数**：无

**返回值**：
- 总是成功，返回当前进程的 PID

**示例**：
```c
pid_t my_pid = getpid();
printf("My PID is %d\n", my_pid);
```

---

### 4.2 fork - 创建子进程

创建当前进程的副本。

**系统调用号**：57

**函数原型**：
```c
pid_t fork(void);
```

**参数**：无

**返回值**：
- 父进程：返回子进程的 PID
- 子进程：返回 0
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -ENOMEM | 内存不足 |
| -EAGAIN | 进程数达到上限 |

**示例**：
```c
pid_t pid = fork();
if (pid < 0) {
    // 错误处理
} else if (pid == 0) {
    // 子进程代码
    printf("I am the child, PID = %d\n", getpid());
} else {
    // 父进程代码
    printf("I am the parent, child PID = %d\n", pid);
}
```

**注意事项**：
- fork 后，子进程拥有父进程地址空间的副本
- 文件描述符在父子进程间共享
- 子进程不继承父进程的锁

---

### 4.3 vfork - 创建子进程（共享地址空间）

创建子进程，与父进程共享地址空间，父进程阻塞直到子进程调用 exec 或 exit。

**系统调用号**：58

**函数原型**：
```c
pid_t vfork(void);
```

**参数**：无

**返回值**：
- 父进程：返回子进程的 PID
- 子进程：返回 0
- 失败：返回负的错误码

**注意事项**：
- vfork 后子进程**必须**立即调用 `execve()` 或 `_exit()`
- 子进程不应修改任何内存或返回
- 主要用于优化 fork + exec 场景

---

### 4.4 clone - 创建进程或线程

灵活创建新的执行上下文，可用于创建进程或线程。

**系统调用号**：56

**函数原型**：
```c
long clone(unsigned long flags, void *stack, int *parent_tidptr,
           int *child_tidptr, unsigned long tls);
```

**参数**：

| 参数 | 描述 |
|------|------|
| flags | 克隆标志，控制共享哪些资源 |
| stack | 新任务的栈指针 |
| parent_tidptr | 存储 TID 的位置（父进程） |
| child_tidptr | 存储 TID 的位置（子进程） |
| tls | 线程本地存储描述符 |

**flags 标志**：

| 标志 | 描述 |
|------|------|
| CLONE_VM | 共享虚拟内存 |
| CLONE_FS | 共享文件系统信息 |
| CLONE_FILES | 共享文件描述符表 |
| CLONE_SIGHAND | 共享信号处理程序 |
| CLONE_THREAD | 放入同一线程组 |
| CLONE_PARENT | 共享父进程 |
| CLONE_VFORK | vfork 语义 |

**返回值**：
- 父进程：返回子进程/线程的 TID
- 子进程：返回 0
- 失败：返回负的错误码

---

### 4.5 execve - 执行程序

用新程序替换当前进程映像。

**系统调用号**：59

**函数原型**：
```c
int execve(const char *pathname, char *const argv[], char *const envp[]);
```

**参数**：

| 参数 | 描述 |
|------|------|
| pathname | 可执行文件路径 |
| argv | 参数数组（以 NULL 结尾） |
| envp | 环境变量数组（以 NULL 结尾） |

**返回值**：
- 成功：不返回
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -ENOENT | 文件不存在 |
| -EACCES | 没有执行权限 |
| -ENOEXEC | 不是有效的可执行文件 |
| -ENOMEM | 内存不足 |

**示例**：
```c
char *argv[] = { "/bin/ls", "-l", NULL };
char *envp[] = { "PATH=/bin", NULL };

if (execve("/bin/ls", argv, envp) < 0) {
    // execve 失败，处理错误
    perror("execve");
}
// 成功时不会执行到这里
```

---

### 4.6 exit - 退出进程

终止当前进程，返回退出状态给父进程。

**系统调用号**：60

**函数原型**：
```c
void exit(int status);
```

**参数**：

| 参数 | 描述 |
|------|------|
| status | 退出状态码（0-255） |

**返回值**：不返回

**示例**：
```c
// 正常退出
exit(0);

// 错误退出
exit(1);
```

**注意事项**：
- 该系统调用不会返回
- 退出状态可通过 wait/waitpid 获取
- 进程变为僵尸状态，直到父进程调用 wait

---

### 4.7 wait4 - 等待子进程

等待子进程状态改变。

**系统调用号**：61

**函数原型**：
```c
pid_t wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage);
```

**参数**：

| 参数 | 描述 |
|------|------|
| pid | 等待的进程 ID（-1 表示任意子进程） |
| wstatus | 存储状态信息的指针 |
| options | 选项标志 |
| rusage | 资源使用信息（可为 NULL） |

**pid 值**：

| 值 | 描述 |
|-----|------|
| < -1 | 等待进程组 ID 为 |pid| 的任意子进程 |
| -1 | 等待任意子进程 |
| 0 | 等待同进程组的任意子进程 |
| > 0 | 等待指定 PID 的子进程 |

**options 标志**：

| 标志 | 描述 |
|------|------|
| WNOHANG | 非阻塞，无子进程退出立即返回 |
| WUNTRACED | 也报告停止的子进程 |
| WCONTINUED | 也报告继续的子进程 |

**返回值**：
- 成功：返回状态改变的子进程 PID
- WNOHANG 且无子进程退出：返回 0
- 失败：返回负的错误码

**状态检查宏**：

```c
WIFEXITED(status)   // 子进程正常退出
WEXITSTATUS(status) // 获取退出码
WIFSIGNALED(status) // 子进程被信号杀死
WTERMSIG(status)    // 获取终止信号
WIFSTOPPED(status)  // 子进程被停止
WSTOPSIG(status)    // 获取停止信号
```

**示例**：
```c
int status;
pid_t child_pid = wait4(-1, &status, 0, NULL);
if (child_pid > 0) {
    if (WIFEXITED(status)) {
        printf("Child %d exited with status %d\n", 
               child_pid, WEXITSTATUS(status));
    }
}
```

---

### 4.8 kill - 发送信号

向进程发送信号。

**系统调用号**：62

**函数原型**：
```c
int kill(pid_t pid, int sig);
```

**参数**：

| 参数 | 描述 |
|------|------|
| pid | 目标进程 ID |
| sig | 信号编号 |

**pid 值**：

| 值 | 描述 |
|-----|------|
| > 0 | 发送到指定进程 |
| 0 | 发送到同进程组的所有进程 |
| -1 | 发送到所有有权限的进程 |
| < -1 | 发送到进程组 |pid| 的所有进程 |

**常用信号**：

| 信号 | 编号 | 描述 |
|------|------|------|
| SIGHUP | 1 | 挂起 |
| SIGINT | 2 | 中断（Ctrl+C） |
| SIGQUIT | 3 | 退出 |
| SIGKILL | 9 | 强制杀死 |
| SIGTERM | 15 | 终止 |
| SIGCHLD | 17 | 子进程状态改变 |
| SIGSTOP | 19 | 停止 |
| SIGCONT | 18 | 继续 |

**返回值**：
- 成功：返回 0
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -ESRCH | 进程不存在 |
| -EPERM | 没有权限 |
| -EINVAL | 无效的信号 |

**示例**：
```c
// 终止进程 1234
if (kill(1234, SIGTERM) < 0) {
    perror("kill");
}

// 强制杀死进程
kill(1234, SIGKILL);
```

---

## 5. 内存管理

### 5.1 brk - 调整堆边界

修改数据段（堆）的结束地址。

**系统调用号**：12

**函数原型**：
```c
int brk(void *addr);
```

**参数**：

| 参数 | 描述 |
|------|------|
| addr | 新的堆结束地址 |

**返回值**：
- 成功：返回 0
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -ENOMEM | 内存不足 |

**注意事项**：
- 通常不直接使用，而是通过 malloc/free
- addr = 0 时返回当前堆结束地址

---

### 5.2 mmap - 内存映射

将文件或设备映射到内存，或分配匿名内存。

**系统调用号**：9

**函数原型**：
```c
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```

**参数**：

| 参数 | 描述 |
|------|------|
| addr | 建议的映射地址（NULL 表示由内核选择） |
| length | 映射长度 |
| prot | 保护标志 |
| flags | 映射标志 |
| fd | 文件描述符（匿名映射时为 -1） |
| offset | 文件偏移（必须页对齐） |

**prot 标志**：

| 标志 | 值 | 描述 |
|------|-----|------|
| PROT_NONE | 0x0 | 不可访问 |
| PROT_READ | 0x1 | 可读 |
| PROT_WRITE | 0x2 | 可写 |
| PROT_EXEC | 0x4 | 可执行 |

**flags 标志**：

| 标志 | 描述 |
|------|------|
| MAP_SHARED | 与其他进程共享映射 |
| MAP_PRIVATE | 写时复制私有映射 |
| MAP_ANONYMOUS | 匿名映射（不关联文件） |
| MAP_FIXED | 必须使用指定地址 |

**返回值**：
- 成功：返回映射的起始地址
- 失败：返回 MAP_FAILED (-1)

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -EINVAL | 参数无效 |
| -ENOMEM | 内存不足 |
| -EACCES | 文件权限不匹配 |
| -EBADF | fd 无效 |

**示例**：
```c
// 分配 4KB 匿名内存
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
if (ptr == MAP_FAILED) {
    perror("mmap");
}

// 映射文件
int fd = open("file.txt", O_RDONLY);
void *file_map = mmap(NULL, file_size, PROT_READ,
                      MAP_PRIVATE, fd, 0);
```

---

### 5.3 munmap - 解除内存映射

解除内存映射。

**系统调用号**：11

**函数原型**：
```c
int munmap(void *addr, size_t length);
```

**参数**：

| 参数 | 描述 |
|------|------|
| addr | 映射的起始地址（必须页对齐） |
| length | 解除映射的长度 |

**返回值**：
- 成功：返回 0
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -EINVAL | 地址未对齐或长度无效 |

**示例**：
```c
if (munmap(ptr, 4096) < 0) {
    perror("munmap");
}
```

---

## 6. 调度控制

### 6.1 sched_yield - 让出 CPU

主动让出 CPU，允许其他进程运行。

**系统调用号**：24

**函数原型**：
```c
int sched_yield(void);
```

**参数**：无

**返回值**：
- 总是返回 0

**示例**：
```c
// 在忙等待循环中让出 CPU
while (!condition) {
    sched_yield();
}
```

**注意事项**：
- 用于协作式多任务或减少忙等待开销
- 如果运行队列中没有其他可运行任务，会立即返回

---

## 7. 系统信息

### 7.1 uname - 获取系统名称

获取系统标识信息。

**系统调用号**：63

**函数原型**：
```c
int uname(struct utsname *buf);
```

**参数**：

| 参数 | 描述 |
|------|------|
| buf | 接收系统信息的结构体指针 |

**结构体定义**：
```c
struct utsname {
    char sysname[65];    // 操作系统名称
    char nodename[65];   // 网络节点名
    char release[65];    // 发行版本
    char version[65];    // 版本信息
    char machine[65];    // 硬件架构
    char domainname[65]; // 域名
};
```

**返回值**：
- 成功：返回 0
- 失败：返回负的错误码

**示例**：
```c
struct utsname buf;
if (uname(&buf) == 0) {
    printf("System: %s\n", buf.sysname);
    printf("Release: %s\n", buf.release);
    printf("Machine: %s\n", buf.machine);
}
```

---

### 7.2 sysinfo - 获取系统状态信息

获取系统整体状态信息，包括内存使用、进程数等。

**系统调用号**：99

**函数原型**：
```c
int sysinfo(struct sysinfo *info);
```

**参数**：

| 参数 | 描述 |
|------|------|
| info | 接收系统信息的结构体指针 |

**结构体定义**：
```c
struct sysinfo {
    long uptime;             // 系统运行秒数
    unsigned long loads[3];  // 1/5/15 分钟平均负载
    unsigned long totalram;  // 总内存（页数）
    unsigned long freeram;   // 空闲内存（页数）
    unsigned long sharedram; // 共享内存
    unsigned long bufferram; // 缓冲区内存
    unsigned long totalswap; // 总交换空间
    unsigned long freeswap;  // 空闲交换空间
    unsigned short procs;    // 进程数
    unsigned long totalhigh; // 高端内存总量
    unsigned long freehigh;  // 空闲高端内存
    unsigned int mem_unit;   // 内存单位（字节）
};
```

**返回值**：
- 成功：返回 0
- 失败：返回负的错误码

**错误码**：

| 错误码 | 描述 |
|--------|------|
| -EFAULT | info 指向无效地址 |

**示例**：
```c
struct sysinfo info;
if (sysinfo(&info) == 0) {
    printf("Uptime: %ld seconds\n", info.uptime);
    printf("Total RAM: %lu bytes\n", info.totalram * info.mem_unit);
    printf("Free RAM: %lu bytes\n", info.freeram * info.mem_unit);
    printf("Processes: %u\n", info.procs);
}
```

---

## 8. 错误码参考

### 8.1 常用错误码

| 错误码 | 值 | 描述 |
|--------|-----|------|
| EPERM | 1 | 操作不允许 |
| ENOENT | 2 | 文件或目录不存在 |
| ESRCH | 3 | 进程不存在 |
| EINTR | 4 | 系统调用被中断 |
| EIO | 5 | I/O 错误 |
| ENXIO | 6 | 设备或地址不存在 |
| E2BIG | 7 | 参数列表过长 |
| ENOEXEC | 8 | 执行格式错误 |
| EBADF | 9 | 无效的文件描述符 |
| ECHILD | 10 | 没有子进程 |
| EAGAIN | 11 | 资源暂时不可用（重试） |
| ENOMEM | 12 | 内存不足 |
| EACCES | 13 | 权限被拒绝 |
| EFAULT | 14 | 无效的地址 |
| EBUSY | 16 | 设备或资源忙 |
| EEXIST | 17 | 文件已存在 |
| ENODEV | 19 | 设备不存在 |
| ENOTDIR | 20 | 不是目录 |
| EISDIR | 21 | 是目录 |
| EINVAL | 22 | 无效的参数 |
| ENFILE | 23 | 系统文件表已满 |
| EMFILE | 24 | 打开的文件过多 |
| ENOTTY | 25 | 不是终端设备 |
| EFBIG | 27 | 文件过大 |
| ENOSPC | 28 | 设备空间不足 |
| ESPIPE | 29 | 非法 seek 操作 |
| EROFS | 30 | 只读文件系统 |
| EPIPE | 32 | 管道破裂 |
| EDOM | 33 | 数学参数超出范围 |
| ERANGE | 34 | 结果超出范围 |
| EDEADLK | 35 | 资源死锁 |
| ENAMETOOLONG | 36 | 文件名过长 |
| ENOSYS | 38 | 系统调用未实现 |
| ENOTEMPTY | 39 | 目录非空 |
| ELOOP | 40 | 符号链接层数过多 |

### 8.2 错误处理最佳实践

```c
/* 检查系统调用返回值 */
long ret = syscall(...);
if (ret < 0) {
    /* 返回值是负的错误码 */
    int error = -ret;
    
    switch (error) {
        case EINTR:
            /* 被信号中断，可以重试 */
            goto retry;
        case EAGAIN:
            /* 资源暂时不可用，稍后重试 */
            sleep(1);
            goto retry;
        case ENOMEM:
            /* 内存不足 */
            handle_oom();
            break;
        default:
            /* 其他错误 */
            printk("Error: %d\n", error);
            break;
    }
}
```

### 8.3 用户空间错误处理

```c
/* 标准 C 库风格 */
#include <errno.h>

int fd = open("/path/to/file", O_RDONLY);
if (fd < 0) {
    /* errno 被设置为错误码 */
    perror("open");  /* 打印 "open: No such file or directory" */
    
    if (errno == ENOENT) {
        /* 文件不存在的特殊处理 */
    }
}
```

---

## 9. 系统调用扩展

### 9.1 添加新系统调用的步骤

1. **定义系统调用号**
   ```c
   // kernel/include/types.h
   #define __NR_my_syscall  200
   ```

2. **实现系统调用函数**
   ```c
   // kernel/syscall.c
   long sys_my_syscall(int arg1, const char *arg2)
   {
       // 验证参数
       if (arg1 < 0)
           return -EINVAL;
       
       // 实现功能
       // ...
       
       return 0;  // 或返回结果
   }
   ```

3. **在分发表中注册**
   ```c
   // do_syscall() 函数中
   case __NR_my_syscall:
       return sys_my_syscall(arg0, (const char *)arg1);
   ```

4. **提供用户空间包装**
   ```c
   // user/libc/syscall.c
   int my_syscall(int arg1, const char *arg2)
   {
       return syscall2(__NR_my_syscall, arg1, (long)arg2);
   }
   ```

### 9.2 系统调用安全注意事项

```c
/* 1. 验证用户指针 */
long sys_read(int fd, char __user *buf, size_t count)
{
    if (!access_ok(VERIFY_WRITE, buf, count))
        return -EFAULT;
    // ...
}

/* 2. 复制用户数据到内核 */
long sys_write(int fd, const char __user *buf, size_t count)
{
    char *kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    // 使用 kbuf...
    kfree(kbuf);
}

/* 3. 检查权限 */
long sys_kill(pid_t pid, int sig)
{
    struct task_struct *task = find_task_by_pid(pid);
    if (!task)
        return -ESRCH;
    
    if (!capable(CAP_KILL) && current->euid != task->euid)
        return -EPERM;
    // ...
}
```

---

## 参考资料

- [Linux System Call Table (x86_64)](https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/)
- [Linux man-pages](https://man7.org/linux/man-pages/)
- [System V Application Binary Interface - AMD64](https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf)
- [OSDev Wiki - System Calls](https://wiki.osdev.org/System_Calls)
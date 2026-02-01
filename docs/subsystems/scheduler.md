# 进程调度器详解

## 目录

1. [概述](#1-概述)
2. [任务状态](#2-任务状态)
3. [调度策略与调度类](#3-调度策略与调度类)
4. [CFS 完全公平调度器](#4-cfs-完全公平调度器)
5. [运行队列](#5-运行队列)
6. [任务结构体](#6-任务结构体)
7. [上下文切换](#7-上下文切换)
8. [调度时机](#8-调度时机)
9. [API 参考](#9-api-参考)

---

## 1. 概述

MiniKernel 的调度器负责管理 CPU 资源，决定哪个任务（进程/线程）获得 CPU 执行时间。调度器采用模块化设计，支持多种调度策略。

### 1.1 设计目标

- **公平性**：确保所有任务获得合理的 CPU 时间
- **响应性**：交互式任务能快速得到响应
- **吞吐量**：最大化系统整体工作效率
- **可扩展性**：支持多核 CPU（SMP）

### 1.2 调度器层次结构

```
                    ┌─────────────────────────┐
                    │      schedule()         │
                    │      调度器核心          │
                    └───────────┬─────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
        ▼                       ▼                       ▼
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│  Stop Class   │ ──► │   RT Class    │ ──► │  Fair Class   │
│  (最高优先级)  │     │  (实时调度)    │     │   (CFS)       │
└───────────────┘     └───────────────┘     └───────┬───────┘
                                                    │
                                                    ▼
                                            ┌───────────────┐
                                            │  Idle Class   │
                                            │  (空闲调度)    │
                                            └───────────────┘
```

### 1.3 相关文件

| 文件 | 描述 |
|------|------|
| `kernel/core/sched/sched.c` | 调度器核心实现 |
| `kernel/core/sched/sched_fair.c` | CFS 公平调度器 |
| `kernel/include/sched.h` | 调度器头文件 |
| `arch/x86_64/cpu/switch.S` | 上下文切换汇编 |

---

## 2. 任务状态

### 2.1 状态定义

```c
#define TASK_RUNNING            0x0000  // 正在运行或就绪
#define TASK_INTERRUPTIBLE      0x0001  // 可中断睡眠
#define TASK_UNINTERRUPTIBLE    0x0002  // 不可中断睡眠
#define TASK_STOPPED            0x0004  // 已停止
#define TASK_TRACED             0x0008  // 被调试
#define TASK_ZOMBIE             0x0010  // 僵尸状态
#define TASK_DEAD               0x0020  // 已死亡
#define TASK_WAKEKILL           0x0040  // 可被致命信号唤醒
#define TASK_WAKING             0x0080  // 正在唤醒
#define TASK_PARKED             0x0100  // 已暂停
#define TASK_NEW                0x0200  // 新建任务
```

### 2.2 状态转换图

```
                            创建
                              │
                              ▼
                       ┌──────────────┐
                       │  TASK_NEW    │
                       └──────┬───────┘
                              │ wake_up_new_task()
                              ▼
    ┌──────────────────────────────────────────────────────┐
    │                    TASK_RUNNING                       │
    │                   (运行/就绪)                          │
    └──────────────────────┬──────────────────┬────────────┘
           │               │                  │
           │ schedule()    │ sleep()          │ 收到信号
           ▼               ▼                  ▼
    ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
    │   CPU 上     │ │INTERRUPTIBLE │ │   STOPPED    │
    │   执行       │ │  (可中断睡眠) │ │   (停止)     │
    └──────────────┘ └──────┬───────┘ └──────────────┘
                           │
                           │ 不可中断 I/O
                           ▼
                    ┌──────────────┐
                    │UNINTERRUPTIBLE│
                    │ (不可中断睡眠) │
                    └──────────────┘
                              
           │
           │ exit()
           ▼
    ┌──────────────┐
    │ TASK_ZOMBIE  │ ──────► 父进程 wait() ──────► TASK_DEAD
    │  (僵尸)       │
    └──────────────┘
```

### 2.3 状态检查宏

```c
#define task_is_running(task)   ((task)->state == TASK_RUNNING)
#define task_is_stopped(task)   ((task)->state & TASK_STOPPED)
#define task_is_traced(task)    ((task)->state & TASK_TRACED)
```

---

## 3. 调度策略与调度类

### 3.1 调度策略

```c
#define SCHED_NORMAL    0   // 普通分时调度（CFS）
#define SCHED_FIFO      1   // 实时先进先出
#define SCHED_RR        2   // 实时轮转
#define SCHED_BATCH     3   // 批处理调度
#define SCHED_IDLE      5   // 空闲调度
#define SCHED_DEADLINE  6   // 截止时间调度
```

### 3.2 调度类结构

```c
struct sched_class {
    const struct sched_class *next;  // 下一个调度类（优先级链）

    /* 入队/出队操作 */
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    
    /* 主动让出 CPU */
    void (*yield_task)(struct rq *rq);

    /* 检查是否需要抢占 */
    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);

    /* 选择下一个任务 */
    struct task_struct *(*pick_next_task)(struct rq *rq);
    
    /* 放回当前任务 */
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    
    /* 设置下一个任务 */
    void (*set_next_task)(struct rq *rq, struct task_struct *p);

    /* 时钟中断处理 */
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    
    /* fork 时初始化 */
    void (*task_fork)(struct task_struct *p);
    
    /* 任务死亡时清理 */
    void (*task_dead)(struct task_struct *p);
};
```

### 3.3 调度类优先级

调度类按优先级链接，调度器按顺序查询直到找到可运行任务：

```
stop_sched_class      (最高) - 停止任务
       ↓
dl_sched_class                - 截止时间调度
       ↓
rt_sched_class                - 实时调度 (FIFO/RR)
       ↓
fair_sched_class              - CFS 公平调度
       ↓
idle_sched_class      (最低) - 空闲任务
```

### 3.4 CFS 调度类定义

```c
const struct sched_class fair_sched_class = {
    .next                   = &idle_sched_class,
    .enqueue_task           = enqueue_task_fair,
    .dequeue_task           = dequeue_task_fair,
    .yield_task             = yield_task_fair,
    .check_preempt_curr     = check_preempt_wakeup,
    .pick_next_task         = pick_next_task_fair,
    .put_prev_task          = put_prev_task_fair,
    .set_curr_task          = set_curr_task_fair,
    /* ... */
};
```

---

## 4. CFS 完全公平调度器

### 4.1 设计理念

CFS (Completely Fair Scheduler) 的核心理念是：**理想情况下，每个任务应该获得相等的 CPU 时间**。

为了实现这一目标，CFS 引入了"虚拟运行时间"（Virtual Runtime, vruntime）的概念：
- 每个任务维护一个 vruntime
- 任务运行时，vruntime 增加
- 调度器总是选择 vruntime 最小的任务运行
- 通过权重调整 vruntime 增长速度，实现优先级差异

### 4.2 核心数据结构

**调度实体 (Scheduling Entity)**：

```c
struct sched_entity {
    struct load_weight load;        // 负载权重
    struct rb_node run_node;        // 红黑树节点
    struct list_head group_node;    // 组调度链表
    unsigned int on_rq;             // 是否在运行队列

    u64 exec_start;                 // 开始执行时间
    u64 sum_exec_runtime;           // 总执行时间
    u64 vruntime;                   // 虚拟运行时间
    u64 prev_sum_exec_runtime;      // 上次总执行时间

    u64 nr_migrations;              // 迁移次数

    /* 统计信息 */
    u64 wait_start;                 // 等待开始时间
    u64 wait_max;                   // 最大等待时间
    u64 wait_count;                 // 等待次数
    u64 wait_sum;                   // 总等待时间
};
```

**CFS 运行队列**：

```c
struct cfs_rq {
    struct load_weight load;        // 总负载权重
    unsigned int nr_running;        // 运行中的任务数
    unsigned int h_nr_running;      // 层次任务数

    u64 exec_clock;                 // 执行时钟
    u64 min_vruntime;               // 最小虚拟运行时间

    struct rb_root tasks_timeline;  // 红黑树根节点
    struct rb_node *rb_leftmost;    // 最左节点（vruntime 最小）

    struct sched_entity *curr;      // 当前运行的调度实体
    struct sched_entity *next;      // 下一个调度实体
    struct sched_entity *last;      // 上一个调度实体
    struct sched_entity *skip;      // 跳过的调度实体
};
```

### 4.3 权重与优先级

nice 值（-20 到 +19）通过权重表转换为实际权重：

```c
static const int prio_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */  9548,  7620,  6100,  4904,  3906,
    /*  -5 */  3121,  2501,  1991,  1586,  1277,
    /*   0 */  1024,   820,   655,   526,   423,
    /*   5 */   335,   272,   215,   172,   137,
    /*  10 */   110,    87,    70,    56,    45,
    /*  15 */    36,    29,    23,    18,    15,
};
```

**权重计算规则**：
- nice 0 的权重为 1024（`NICE_0_LOAD`）
- 每降低 1 个 nice 值，权重约增加 25%
- 每提高 1 个 nice 值，权重约减少 20%

### 4.4 虚拟运行时间计算

**基本公式**：

```
                    NICE_0_LOAD
vruntime += delta × ──────────────
                     task_weight
```

其中：
- `delta` = 实际运行的时间
- `NICE_0_LOAD` = 1024
- `task_weight` = 任务的权重

**代码实现**：

```c
static u64 calc_delta_fair(u64 delta, struct sched_entity *se)
{
    if (unlikely(se->load.weight != NICE_0_LOAD))
        delta = __calc_delta(delta, NICE_0_LOAD, &se->load);
    return delta;
}

static void update_curr(struct cfs_rq *cfs_rq)
{
    struct sched_entity *curr = cfs_rq->curr;
    u64 now = sched_clock_cpu(cpu_of(rq_of(cfs_rq)));
    u64 delta_exec;

    if (unlikely(!curr))
        return;

    delta_exec = now - curr->exec_start;
    if (unlikely((s64)delta_exec <= 0))
        return;

    curr->exec_start = now;
    curr->sum_exec_runtime += delta_exec;
    
    /* 更新虚拟运行时间 */
    curr->vruntime += calc_delta_fair(delta_exec, curr);
    
    /* 更新最小虚拟运行时间 */
    update_min_vruntime(cfs_rq);
}
```

### 4.5 红黑树操作

CFS 使用红黑树按 vruntime 排序存储可运行任务，保证 O(log n) 时间复杂度。

**入队**：

```c
static void __enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
    struct rb_node **link = &cfs_rq->tasks_timeline.rb_node;
    struct rb_node *parent = NULL;
    struct sched_entity *entry;
    int leftmost = 1;

    /* 查找插入位置 */
    while (*link) {
        parent = *link;
        entry = rb_entry(parent, struct sched_entity, run_node);
        
        if (entity_key(cfs_rq, se) < entity_key(cfs_rq, entry)) {
            link = &parent->rb_left;
        } else {
            link = &parent->rb_right;
            leftmost = 0;
        }
    }

    /* 更新最左节点缓存 */
    if (leftmost)
        cfs_rq->rb_leftmost = &se->run_node;

    /* 插入并平衡 */
    rb_link_node(&se->run_node, parent, link);
    rb_insert_color(&se->run_node, &cfs_rq->tasks_timeline);
}
```

**选择下一个任务**：

```c
static struct sched_entity *__pick_first_entity(struct cfs_rq *cfs_rq)
{
    struct rb_node *left = cfs_rq->rb_leftmost;

    if (!left)
        return NULL;

    return rb_entry(left, struct sched_entity, run_node);
}

static struct task_struct *pick_next_task_fair(struct rq *rq, 
                                               struct task_struct *prev)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    struct sched_entity *se;

    if (!cfs_rq->nr_running)
        return NULL;

    /* 获取 vruntime 最小的任务 */
    se = pick_next_entity(cfs_rq, NULL);
    if (!se)
        return NULL;

    set_next_entity(cfs_rq, se);

    return task_of(se);
}
```

### 4.6 时间片计算

CFS 不使用固定时间片，而是根据运行队列中的任务数动态计算：

```
                        task_weight
time_slice = sched_period × ────────────────────
                            total_rq_weight
```

**调度周期**：

```c
#define SCHED_LATENCY_NS        (6 * 1000000ULL)     // 6ms
#define SCHED_MIN_GRANULARITY_NS (750000ULL)         // 0.75ms

/* 调度周期 = max(SCHED_LATENCY, nr_running * MIN_GRANULARITY) */
```

### 4.7 抢占判断

```c
static void check_preempt_wakeup(struct rq *rq, struct task_struct *p, 
                                 int wake_flags)
{
    struct task_struct *curr = rq->curr;
    struct sched_entity *se = &curr->se, *pse = &p->se;

    /* 如果是同一个任务，不抢占 */
    if (unlikely(se == pse))
        return;

    /* 检查是否需要抢占 */
    update_curr(cfs_rq_of(se));
    
    if (wakeup_preempt_entity(se, pse) == 1) {
        /* 设置抢占标志 */
        resched_curr(rq);
    }
}

/* 判断 curr 是否应该被 p 抢占 */
static int wakeup_preempt_entity(struct sched_entity *curr, 
                                 struct sched_entity *se)
{
    s64 gran, vdiff = curr->vruntime - se->vruntime;

    if (vdiff <= 0)
        return -1;  // curr 更优先

    gran = SCHED_WAKEUP_GRANULARITY_NS;
    if (vdiff > gran)
        return 1;   // se 更优先，应该抢占

    return 0;
}
```

---

## 5. 运行队列

### 5.1 运行队列结构

```c
struct rq {
    spinlock_t lock;                // 队列锁
    unsigned int nr_running;        // 运行中的任务数
    u64 nr_switches;                // 上下文切换次数
    u64 clock;                      // 运行队列时钟
    u64 clock_task;                 // 任务时钟

    struct task_struct *curr;       // 当前任务
    struct task_struct *idle;       // 空闲任务

    struct list_head cfs_tasks;     // CFS 任务列表
    struct cfs_rq cfs;              // CFS 运行队列

    struct rt_rq rt;                // 实时运行队列
    struct dl_rq dl;                // 截止时间运行队列

    int cpu;                        // 所属 CPU
    int online;                     // CPU 是否在线

    /* 负载均衡相关 */
    u64 next_balance;               // 下次负载均衡时间
    u64 avg_idle;                   // 平均空闲时间
    
    /* 统计信息 */
    u64 nr_load_updates;            // 负载更新次数
    struct rq_sched_info rq_sched_info;
};
```

### 5.2 Per-CPU 运行队列

每个 CPU 有独立的运行队列，减少锁竞争：

```c
static struct rq runqueues[NR_CPUS];

#define cpu_rq(cpu)     (&runqueues[(cpu)])
#define this_rq()       cpu_rq(smp_processor_id())
```

### 5.3 运行队列操作

**入队**：

```c
void activate_task(struct rq *rq, struct task_struct *p, int flags)
{
    if (task_contributes_to_load(p))
        rq->nr_uninterruptible--;

    enqueue_task(rq, p, flags);
}

static void enqueue_task(struct rq *rq, struct task_struct *p, int flags)
{
    update_rq_clock(rq);

    if (!(flags & ENQUEUE_RESTORE))
        sched_info_queued(rq, p);

    p->sched_class->enqueue_task(rq, p, flags);
}
```

**出队**：

```c
void deactivate_task(struct rq *rq, struct task_struct *p, int flags)
{
    if (task_contributes_to_load(p))
        rq->nr_uninterruptible++;

    dequeue_task(rq, p, flags);
}
```

---

## 6. 任务结构体

### 6.1 task_struct 定义

```c
struct task_struct {
    /* 调度器状态 */
    volatile long state;            // 任务状态
    void *stack;                    // 内核栈
    u32 flags;                      // 任务标志
    u32 ptrace;                     // ptrace 标志

    /* 进程 ID */
    pid_t pid;                      // 进程 ID
    pid_t tgid;                     // 线程组 ID
    pid_t ppid;                     // 父进程 ID
    pid_t pgrp;                     // 进程组 ID
    pid_t session;                  // 会话 ID

    /* 用户/组 ID */
    uid_t uid, euid, suid, fsuid;
    gid_t gid, egid, sgid, fsgid;

    /* 调度参数 */
    int prio;                       // 动态优先级
    int static_prio;                // 静态优先级
    int normal_prio;                // 普通优先级
    unsigned int rt_priority;       // 实时优先级
    unsigned int policy;            // 调度策略
    
    struct sched_entity se;         // CFS 调度实体
    struct sched_rt_entity rt;      // RT 调度实体

    u64 utime;                      // 用户态执行时间
    u64 stime;                      // 内核态执行时间
    u64 start_time;                 // 启动时间

    /* CPU 亲和性 */
    unsigned long cpus_allowed;     // 允许的 CPU 掩码
    int nr_cpus_allowed;            // 允许的 CPU 数
    int on_cpu;                     // 是否在 CPU 上
    int recent_used_cpu;            // 最近使用的 CPU

    /* 进程关系 */
    struct task_struct *real_parent;// 真正的父进程
    struct task_struct *parent;     // 养父进程（接收 SIGCHLD）
    struct list_head children;      // 子进程链表
    struct list_head sibling;       // 兄弟进程链表
    struct task_struct *group_leader;// 线程组领导

    /* 任务链表 */
    struct list_head tasks;         // 所有任务链表
    struct list_head run_list;      // 运行链表

    /* 内存管理 */
    struct mm_struct *mm;           // 内存描述符
    struct mm_struct *active_mm;    // 活跃内存描述符

    /* 线程信息 */
    struct thread_struct thread;    // CPU 相关状态
    struct pt_regs *regs;           // 寄存器快照

    /* 退出信息 */
    int exit_state;                 // 退出状态
    int exit_code;                  // 退出码
    int exit_signal;                // 退出信号

    /* 进程名 */
    char comm[16];                  // 命令名

    /* 统计信息 */
    unsigned long min_flt;          // 次缺页次数
    unsigned long maj_flt;          // 主缺页次数
    unsigned long nvcsw;            // 自愿上下文切换
    unsigned long nivcsw;           // 非自愿上下文切换

    /* 内核线程函数 */
    int (*thread_fn)(void *);       // 线程函数
    void *thread_data;              // 线程参数
};
```

### 6.2 任务分配与释放

```c
struct task_struct *alloc_task_struct(void)
{
    struct task_struct *task;

    /* 分配 task_struct */
    task = kmalloc(sizeof(struct task_struct), GFP_KERNEL);
    if (!task)
        return NULL;

    memset(task, 0, sizeof(struct task_struct));

    /* 分配内核栈 */
    task->stack = kmalloc(THREAD_SIZE, GFP_KERNEL);
    if (!task->stack) {
        kfree(task);
        return NULL;
    }

    /* 初始化默认值 */
    task->state = TASK_RUNNING;
    task->prio = DEFAULT_PRIO;
    task->static_prio = DEFAULT_PRIO;
    task->normal_prio = DEFAULT_PRIO;
    task->policy = SCHED_NORMAL;

    /* 初始化链表 */
    INIT_LIST_HEAD(&task->tasks);
    INIT_LIST_HEAD(&task->children);
    INIT_LIST_HEAD(&task->sibling);

    return task;
}

void free_task_struct(struct task_struct *tsk)
{
    if (!tsk)
        return;

    if (tsk->stack)
        kfree(tsk->stack);
    kfree(tsk);
}
```

---

## 7. 上下文切换

### 7.1 上下文切换过程

```c
static void context_switch(struct rq *rq, struct task_struct *prev,
                          struct task_struct *next)
{
    struct mm_struct *mm, *oldmm;

    /* 准备切换 */
    prepare_task_switch(rq, prev, next);

    mm = next->mm;
    oldmm = prev->active_mm;

    /* 处理内存描述符 */
    if (!mm) {
        /* 内核线程，借用前一个任务的 mm */
        next->active_mm = oldmm;
        atomic_inc(&oldmm->mm_count);
        enter_lazy_tlb(oldmm, next);
    } else {
        /* 用户进程，切换地址空间 */
        switch_mm(oldmm, mm, next);
    }

    if (!prev->mm) {
        prev->active_mm = NULL;
        rq->prev_mm = oldmm;
    }

    /* 执行实际的上下文切换 */
    switch_to(prev, next, prev);

    barrier();

    /* 完成切换 */
    finish_task_switch(prev);
}
```

### 7.2 汇编实现 (x86_64)

```asm
# void switch_to(struct task_struct *prev, struct task_struct *next)
# RDI = prev, RSI = next
switch_to:
    # 保存当前进程的寄存器
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushfq

    # 保存当前栈指针到 prev->thread.sp
    movq %rsp, 0x1000(%rdi)

    # 切换到新进程的栈
    movq 0x1000(%rsi), %rsp

    # 恢复新进程的寄存器
    popfq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    # 返回到新进程
    ret
```

### 7.3 保存的寄存器

| 寄存器 | 用途 | 是否保存 |
|--------|------|----------|
| RAX | 返回值/临时 | 否（调用者保存） |
| RBX | 基址 | 是（被调用者保存） |
| RCX | 计数器/参数4 | 否 |
| RDX | 数据/参数3 | 否 |
| RSI | 源索引/参数2 | 否 |
| RDI | 目的索引/参数1 | 否 |
| RBP | 帧指针 | 是 |
| RSP | 栈指针 | 是（隐式） |
| R8-R11 | 临时/参数 | 否 |
| R12-R15 | 通用 | 是 |
| RFLAGS | 标志 | 是 |

---

## 8. 调度时机

### 8.1 主动调度

任务主动放弃 CPU 的情况：

```c
/* 1. 显式调用 schedule() */
void some_function(void)
{
    /* 完成当前工作后主动让出 */
    schedule();
}

/* 2. 调用 sched_yield() */
while (!condition_met()) {
    sched_yield();  /* 让其他任务运行 */
}

/* 3. 等待事件（进入睡眠） */
void wait_for_event(void)
{
    set_current_state(TASK_INTERRUPTIBLE);
    while (!event_ready) {
        schedule();
        set_current_state(TASK_INTERRUPTIBLE);
    }
    set_current_state(TASK_RUNNING);
}
```

### 8.2 被动调度（抢占）

任务被强制切换的情况：

**用户态抢占**：
- 从系统调用返回用户空间时
- 从中断返回用户空间时
- 检查 `TIF_NEED_RESCHED` 标志

**内核态抢占**（如果启用）：
- 从中断返回内核空间时
- 持有的自旋锁全部释放后
- 显式启用抢占点

```c
/* 设置重新调度标志 */
void resched_curr(struct rq *rq)
{
    struct task_struct *curr = rq->curr;
    
    if (test_tsk_need_resched(curr))
        return;
    
    set_tsk_need_resched(curr);
}

/* 检查是否需要重新调度 */
static inline int need_resched(void)
{
    return test_tsk_need_resched(current);
}
```

### 8.3 时钟中断调度

```c
void scheduler_tick(void)
{
    int cpu = smp_processor_id();
    struct rq *rq = cpu_rq(cpu);
    struct task_struct *curr = rq->curr;
    
    /* 更新运行队列时钟 */
    update_rq_clock(rq);
    
    /* 调用调度类的 tick 处理 */
    curr->sched_class->task_tick(rq, curr, 0);
    
    /* 更新负载统计 */
    update_cpu_load(rq);
    
    /* 触发负载均衡（多核） */
    trigger_load_balance(rq);
}
```

### 8.4 调度流程总结

```
                        ┌─────────────────┐
                        │   任务运行中     │
                        └────────┬────────┘
                                 │
         ┌───────────────────────┼───────────────────────┐
         │                       │                       │
         ▼                       ▼                       ▼
  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐
  │  时钟中断    │      │  系统调用    │      │  主动让出    │
  │ tick 更新    │      │  完成返回    │      │ yield/sleep │
  └──────┬───────┘      └──────┬───────┘      └──────┬───────┘
         │                     │                     │
         ▼                     ▼                     ▼
  ┌──────────────────────────────────────────────────────────┐
  │              检查 TIF_NEED_RESCHED 标志                   │
  └─────────────────────────┬────────────────────────────────┘
                            │
              ┌─────────────┴─────────────┐
              │ 需要调度?                  │
              │                           │
         是   ▼                      否   ▼
  ┌──────────────────┐         ┌──────────────────┐
  │   schedule()     │         │   继续执行当前   │
  │   选择下一个任务  │         │   任务           │
  │   上下文切换     │         │                  │
  └──────────────────┘         └──────────────────┘
```

---

## 9. API 参考

### 9.1 调度器初始化

```c
/**
 * sched_init - 初始化调度器
 * 
 * 在内核启动时调用，初始化所有 CPU 的运行队列和调度数据结构
 */
void sched_init(void);
```

### 9.2 任务管理

```c
/**
 * alloc_task_struct - 分配任务结构
 * 
 * 返回: 成功返回 task_struct 指针，失败返回 NULL
 */
struct task_struct *alloc_task_struct(void);

/**
 * free_task_struct - 释放任务结构
 * @tsk: 要释放的任务
 */
void free_task_struct(struct task_struct *tsk);

/**
 * dup_task_struct - 复制任务结构
 * @orig: 原始任务
 * 
 * 返回: 成功返回新任务指针，失败返回 NULL
 */
struct task_struct *dup_task_struct(struct task_struct *orig);

/**
 * get_task_struct - 增加任务引用计数
 * @tsk: 目标任务
 */
void get_task_struct(struct task_struct *tsk);

/**
 * put_task_struct - 减少任务引用计数
 * @tsk: 目标任务
 * 
 * 引用计数归零时自动释放任务
 */
void put_task_struct(struct task_struct *tsk);
```

### 9.3 调度控制

```c
/**
 * schedule - 执行调度
 * 
 * 选择下一个任务并进行上下文切换
 */
void schedule(void);

/**
 * yield - 主动让出 CPU
 * 
 * 将当前任务放到运行队列尾部，让其他任务运行
 */
void yield(void);

/**
 * sched_yield - yield 的系统调用版本
 * 
 * 返回: 总是返回 0
 */
long sched_yield(void);

/**
 * scheduler_tick - 时钟中断处理
 * 
 * 由时钟中断调用，更新调度统计和检查抢占
 */
void scheduler_tick(void);
```

### 9.4 任务状态

```c
/**
 * wake_up_process - 唤醒任务
 * @p: 要唤醒的任务
 * 
 * 返回: 如果任务被成功唤醒返回 1，否则返回 0
 */
int wake_up_process(struct task_struct *p);

/**
 * wake_up_new_task - 唤醒新创建的任务
 * @p: 新任务
 */
void wake_up_new_task(struct task_struct *p);

/**
 * set_task_state - 设置任务状态
 * @task: 目标任务
 * @state: 新状态
 */
void set_task_state(struct task_struct *task, long state);

/* 状态设置宏 */
#define set_current_state(state) \
    do { current->state = (state); mb(); } while (0)

#define __set_current_state(state) \
    do { current->state = (state); } while (0)
```

### 9.5 Fork 相关

```c
/**
 * sched_fork - 为新任务初始化调度数据
 * @p: 新创建的任务
 */
void sched_fork(struct task_struct *p);

/**
 * do_fork - 创建新进程
 * @clone_flags: 克隆标志
 * @stack_start: 用户栈起始地址
 * @stack_size: 栈大小
 * @parent_tidptr: 父进程 TID 存储位置
 * @child_tidptr: 子进程 TID 存储位置
 * 
 * 返回: 成功返回子进程 PID，失败返回负错误码
 */
long do_fork(unsigned long clone_flags, unsigned long stack_start,
             unsigned long stack_size, int __user *parent_tidptr,
             int __user *child_tidptr);
```

### 9.6 运行队列操作

```c
/**
 * activate_task - 将任务加入运行队列
 * @rq: 运行队列
 * @p: 任务
 * @flags: 入队标志
 */
void activate_task(struct rq *rq, struct task_struct *p, int flags);

/**
 * deactivate_task - 将任务从运行队列移除
 * @rq: 运行队列
 * @p: 任务
 * @flags: 出队标志
 */
void deactivate_task(struct rq *rq, struct task_struct *p, int flags);

/* 运行队列访问宏 */
#define cpu_rq(cpu)     (&runqueues[(cpu)])
#define this_rq()       cpu_rq(smp_processor_id())
#define task_rq(p)      cpu_rq(task_cpu(p))
```

### 9.7 当前任务

```c
/* 获取当前任务 */
extern struct task_struct *current_task;
#define current         (current_task)
#define get_current()   (current_task)

/* 设置当前任务（仅限上下文切换使用） */
#define set_current(task) do { current_task = (task); } while (0)
```

---

## 10. 调试与性能分析

### 10.1 调度统计

```c
struct sched_info {
    unsigned long pcount;       /* 调度次数 */
    unsigned long long run_delay; /* 等待运行的时间 */
    unsigned long long last_arrival; /* 上次到达运行队列的时间 */
    unsigned long long last_queued;  /* 上次入队的时间 */
};

/* 获取任务的调度统计 */
void sched_show_task(struct task_struct *p);
```

### 10.2 常见问题排查

| 问题 | 可能原因 | 排查方法 |
|------|----------|----------|
| 任务饥饿 | 高优先级任务占用 CPU | 检查 nice 值和调度策略 |
| 响应慢 | 时间片过长 | 调整 sched_latency |
| 上下文切换频繁 | 时间片过短 | 调整 sched_min_granularity |
| 死锁 | 锁顺序问题 | 检查自旋锁使用 |
| 优先级反转 | 低优先级持锁 | 使用优先级继承 |

### 10.3 调度相关内核参数

```c
/* CFS 调度参数（纳秒） */
unsigned int sysctl_sched_latency = 6000000ULL;           /* 调度周期 */
unsigned int sysctl_sched_min_granularity = 750000ULL;    /* 最小时间片 */
unsigned int sysctl_sched_wakeup_granularity = 1000000ULL; /* 唤醒粒度 */

/* 调整方法（实际系统中） */
/* /proc/sys/kernel/sched_latency_ns */
/* /proc/sys/kernel/sched_min_granularity_ns */
```

---

## 参考资料

- [Linux Kernel CFS Scheduler](https://www.kernel.org/doc/html/latest/scheduler/sched-design-CFS.html)
- [Understanding the Linux Scheduler](https://developer.ibm.com/tutorials/l-completely-fair-scheduler/)
- [OSDev Wiki - Scheduling](https://wiki.osdev.org/Scheduling_Algorithms)
- [Intel® 64 and IA-32 Architectures Software Developer's Manual](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html) - 上下文切换相关
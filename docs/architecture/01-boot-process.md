# 内核启动流程详解

## 目录

1. [概述](#1-概述)
2. [Multiboot 引导协议](#2-multiboot-引导协议)
3. [32位保护模式阶段](#3-32位保护模式阶段)
4. [长模式切换](#4-长模式切换)
5. [64位长模式阶段](#5-64位长模式阶段)
6. [内核初始化](#6-内核初始化)
7. [链接脚本详解](#7-链接脚本详解)
8. [启动调试](#8-启动调试)

---

## 1. 概述

MiniKernel 的启动过程从 BIOS/UEFI 开始，经过引导加载器（如 GRUB 或 QEMU 的 `-kernel` 选项），最终将控制权交给内核。整个启动过程可以分为以下几个阶段：

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  BIOS/UEFI  │ ──► │  Bootloader │ ──► │   boot.S    │ ──► │   main.c    │
│             │     │(GRUB/QEMU)  │     │ (32→64 bit) │     │ kernel_main │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
```

**关键文件**：
- `arch/x86_64/boot/boot.S` - 汇编引导代码
- `arch/x86_64/boot/kernel.ld` - 链接脚本
- `src/kernel/main.c` - 内核主函数

---

## 2. Multiboot 引导协议

### 2.1 Multiboot 协议简介

Multiboot 是一个开放的引导协议标准，允许引导加载器以统一的方式加载操作系统内核。MiniKernel 同时支持 Multiboot1 和 Multiboot2 协议。

### 2.2 Multiboot1 头部

```asm
.section .multiboot
.align 4

# Multiboot1 常量
.set MULTIBOOT_MAGIC,       0x1BADB002
.set MULTIBOOT_ALIGN,       1 << 0              # 页对齐
.set MULTIBOOT_MEMINFO,     1 << 1              # 请求内存信息
.set MULTIBOOT_FLAGS,       MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO
.set MULTIBOOT_CHECKSUM,    -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

# Multiboot1 头部结构
multiboot_header:
    .long MULTIBOOT_MAGIC       # 魔数：标识这是 Multiboot 兼容内核
    .long MULTIBOOT_FLAGS       # 标志：请求的功能
    .long MULTIBOOT_CHECKSUM    # 校验和：确保头部有效
```

**字段说明**：

| 字段 | 偏移 | 描述 |
|------|------|------|
| magic | 0 | 固定值 `0x1BADB002`，用于识别 |
| flags | 4 | 请求的功能位掩码 |
| checksum | 8 | 使 magic + flags + checksum = 0 |

**标志位**：
- `bit 0` (ALIGN)：要求模块按页边界对齐
- `bit 1` (MEMINFO)：请求内存映射信息
- `bit 2` (VIDEO)：请求视频模式信息

### 2.3 Multiboot2 头部

```asm
.section .multiboot2
.align 8

multiboot2_header:
    .long 0xe85250d6                    # 魔数
    .long 0                             # 架构 (0 = i386/x86)
    .long multiboot2_header_end - multiboot2_header    # 头部长度
    .long -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header))  # 校验和

    # 结束标记
    .short 0    # 类型
    .short 0    # 标志
    .long 8     # 大小
multiboot2_header_end:
```

**Multiboot2 的改进**：
- 支持更多的标签类型
- 可以请求更详细的系统信息
- 支持 UEFI 启动
- 更灵活的模块加载

### 2.4 头部位置要求

Multiboot 头部必须位于内核映像的前 8KB（Multiboot1）或 32KB（Multiboot2）内。链接脚本通过将 `.multiboot` 段放在最前面来确保这一点：

```ld
.boot ALIGN(4) :
{
    KEEP(*(.multiboot))     /* Multiboot1 头部优先 */
    . = ALIGN(8);
    KEEP(*(.multiboot2))    /* Multiboot2 头部 */
}
```

---

## 3. 32位保护模式阶段

当引导加载器将控制权交给内核时，CPU 处于 32 位保护模式，具有以下状态：

### 3.1 初始 CPU 状态

| 寄存器 | 值 | 描述 |
|--------|-----|------|
| EAX | 0x2BADB002 或 0x36d76289 | Multiboot 魔数 |
| EBX | 物理地址 | Multiboot 信息结构指针 |
| CS | 代码段选择子 | 32位代码段 |
| DS/ES/FS/GS/SS | 数据段选择子 | 32位数据段 |
| CR0.PE | 1 | 保护模式已启用 |
| CR0.PG | 0 | 分页未启用 |
| EFLAGS.IF | 0 | 中断禁用 |

### 3.2 入口点代码

```asm
.section .text
.code32
.global _start

_start:
    # 禁用中断
    cli

    # 保存 multiboot 信息
    movl %ebx, %edi    # multiboot 信息结构指针
    movl %eax, %esi    # multiboot 魔数

    # 设置临时栈指针
    movl $stack_top, %esp

    # 执行 CPU 功能检查
    call check_cpuid
    call check_long_mode

    # 设置分页并切换到长模式
    call setup_page_tables
    call enable_paging

    # 加载 GDT
    lgdt gdt64_desc

    # 跳转到 64 位代码段
    ljmp $0x08, $long_mode_start
```

### 3.3 CPUID 检查

在切换到长模式之前，必须确认 CPU 支持 CPUID 指令和长模式。

**检查 CPUID 支持**：

```asm
check_cpuid:
    # 尝试翻转 EFLAGS 中的 ID 位 (bit 21)
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $0x200000, %eax    # 翻转 ID 位
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    cmpl %ecx, %eax         # 如果能翻转，则支持 CPUID
    je no_cpuid
    ret
```

**原理**：EFLAGS 寄存器的 bit 21（ID 位）只有在 CPU 支持 CPUID 时才能被修改。

**检查长模式支持**：

```asm
check_long_mode:
    # 检查扩展 CPUID 功能
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode

    # 检查长模式位
    movl $0x80000001, %eax
    cpuid
    testl $0x20000000, %edx  # bit 29 = 长模式支持
    jz no_long_mode
    ret
```

**原理**：CPUID 扩展功能 `0x80000001` 返回的 EDX 寄存器的 bit 29 指示是否支持长模式（64位模式）。

---

## 4. 长模式切换

### 4.1 切换前的准备工作

切换到长模式需要以下准备：

1. **设置 4 级页表**
2. **启用 PAE（物理地址扩展）**
3. **设置 EFER.LME（长模式使能位）**
4. **启用分页**
5. **加载 64 位 GDT**
6. **远跳转到 64 位代码段**

### 4.2 页表结构

x86_64 使用 4 级页表结构：

```
虚拟地址（48位）
┌────────┬────────┬────────┬────────┬────────────┐
│ PML4   │ PDPT   │  PDT   │  PT    │   Offset   │
│(9 bits)│(9 bits)│(9 bits)│(9 bits)│ (12 bits)  │
└───┬────┴───┬────┴───┬────┴───┬────┴─────┬──────┘
    │        │        │        │          │
    ▼        ▼        ▼        ▼          ▼
┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
│ PML4   │→│ PDPT   │→│  PDT   │→│  PT    │→│ 物理页 │
│ Table  │ │ Table  │ │ Table  │ │ Table  │ │        │
└────────┘ └────────┘ └────────┘ └────────┘ └────────┘
```

**页表项格式**（64位）：

| 位 | 名称 | 描述 |
|-----|------|------|
| 0 | Present | 页面存在 |
| 1 | Read/Write | 0=只读, 1=可写 |
| 2 | User/Supervisor | 0=内核, 1=用户 |
| 3 | Page Write-Through | 写穿透缓存 |
| 4 | Page Cache Disable | 禁用缓存 |
| 5 | Accessed | 已访问 |
| 6 | Dirty | 已修改（仅 PT） |
| 7 | Page Size | 1=大页（2MB/1GB） |
| 12-51 | 物理地址 | 下一级表/页的物理地址 |
| 63 | Execute Disable | 禁止执行（需 EFER.NXE） |

### 4.3 页表设置代码

```asm
setup_page_tables:
    # 清空页表区域
    movl $boot_pml4, %edi
    xorl %eax, %eax
    movl $4096, %ecx
    rep stosb

    movl $boot_pdpt, %edi
    xorl %eax, %eax
    movl $4096, %ecx
    rep stosb

    movl $boot_pdt, %edi
    xorl %eax, %eax
    movl $4096, %ecx
    rep stosb

    # 设置 PML4[0] -> PDPT（恒等映射）
    movl $boot_pml4, %edi
    movl $boot_pdpt, %eax
    orl $0x003, %eax    # Present + Writable
    movl %eax, (%edi)

    # 设置 PML4[256] -> PDPT（高地址映射 0xFFFF800000000000）
    movl $boot_pml4, %edi
    addl $(256 * 8), %edi
    movl $boot_pdpt, %eax
    orl $0x003, %eax
    movl %eax, (%edi)

    # 设置 PDPT[0] -> PDT
    movl $boot_pdpt, %edi
    movl $boot_pdt, %eax
    orl $0x003, %eax
    movl %eax, (%edi)

    # 设置 PDT（使用 2MB 大页映射前 1GB）
    movl $boot_pdt, %edi
    movl $0x00000083, %eax    # Present + Writable + 2MB Page
    movl $512, %ecx

1:
    movl %eax, (%edi)
    addl $0x200000, %eax    # 下一个 2MB
    addl $8, %edi
    decl %ecx
    jnz 1b

    ret
```

**映射策略**：

| 虚拟地址范围 | 物理地址范围 | 用途 |
|-------------|-------------|------|
| 0x0 - 0x40000000 | 0x0 - 0x40000000 | 恒等映射（启动用） |
| 0xFFFF800000000000 - 0xFFFF800040000000 | 0x0 - 0x40000000 | 内核高地址映射 |

### 4.4 启用分页和长模式

```asm
enable_paging:
    # 1. 设置 CR3（页目录基址）
    movl $boot_pml4, %eax
    movl %eax, %cr3

    # 2. 启用 PAE (CR4.PAE = 1)
    movl %cr4, %eax
    orl $0x20, %eax      # bit 5 = PAE
    movl %eax, %cr4

    # 3. 设置 EFER.LME = 1（长模式使能）
    movl $0xC0000080, %ecx    # EFER MSR
    rdmsr
    orl $0x100, %eax          # bit 8 = LME
    wrmsr

    # 4. 启用分页 (CR0.PG = 1)
    movl %cr0, %eax
    orl $0x80000001, %eax     # bit 31 = PG, bit 0 = PE
    movl %eax, %cr0

    ret
```

**控制寄存器说明**：

| 寄存器 | 位 | 名称 | 作用 |
|--------|-----|------|------|
| CR0 | 0 | PE | 保护模式使能 |
| CR0 | 31 | PG | 分页使能 |
| CR3 | - | PDBR | 页目录基址寄存器 |
| CR4 | 5 | PAE | 物理地址扩展 |
| EFER | 8 | LME | 长模式使能 |
| EFER | 10 | LMA | 长模式激活（只读） |

### 4.5 64位 GDT

```asm
.section .data
.align 8

gdt64:
    .quad 0x0000000000000000    # 空描述符
    .quad 0x00209A0000000000    # 64位代码段
    .quad 0x0000920000000000    # 64位数据段
gdt64_end:

gdt64_desc:
    .word gdt64_end - gdt64 - 1    # GDT 限长
    .quad gdt64                     # GDT 基址
```

**64位代码段描述符** (0x00209A0000000000)：

| 字段 | 值 | 含义 |
|------|-----|------|
| Limit 0-15 | 0x0000 | 不使用 |
| Base 0-23 | 0x000000 | 不使用 |
| Type | 0xA | 代码段，可执行，可读 |
| S | 1 | 代码/数据段 |
| DPL | 0 | 特权级 0 |
| P | 1 | 存在 |
| L | 1 | 64位代码段 |
| D | 0 | 64位模式下必须为 0 |

---

## 5. 64位长模式阶段

### 5.1 进入长模式

```asm
.code64
long_mode_start:
    # 设置段寄存器
    movw $0x10, %ax    # 数据段选择子
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # 设置 64 位栈
    movabsq $stack_top, %rsp

    # 准备参数给 kernel_main
    # rdi = multiboot 魔数
    # rsi = multiboot 信息指针
    movl %esi, %edi
    movq $0, %rsi

    # 调用内核主函数
    call kernel_main

    # 如果 kernel_main 返回，停机
    cli
1:  hlt
    jmp 1b
```

### 5.2 栈设置

```asm
.section .bss
.align 16

stack_bottom:
    .space 16384    # 16KB 栈空间
stack_top:
```

**栈布局**：
```
高地址  stack_top    ◄── RSP 初始值
        │          │
        │   栈帧   │
        │    ↓     │
        │          │
低地址  stack_bottom
```

---

## 6. 内核初始化

### 6.1 kernel_main 函数

```c
void kernel_main(void)
{
    /* 初始化内核 */
    kernel_init();

    /* 启动交互式 shell */
    printk("Starting shell...\n");
    shell_run();

    /* 永不应该到达这里 */
    panic("Shell exited unexpectedly!");
}
```

### 6.2 kernel_init 初始化序列

```c
static void kernel_init(void)
{
    printk("Initializing %s %s\n", KERNEL_NAME, KERNEL_VERSION);

    /* 1. 内存管理初始化 */
    printk("  Initializing memory management...\n");
    mm_init();
    buddy_init();

    /* 2. 调度器初始化 */
    printk("  Initializing scheduler...\n");
    sched_init();

    /* 3. IPC 初始化 */
    printk("  Initializing IPC...\n");
    ipc_init();

    /* 4. VFS 初始化 */
    printk("  Initializing VFS...\n");
    vfs_init();

    /* 5. 网络初始化 */
    printk("  Initializing network...\n");
    net_init();

    /* 6. 驱动初始化 */
    printk("  Initializing drivers...\n");
    driver_init();

    /* 7. 创建 init 进程 */
    printk("  Creating init process...\n");
    init_task = create_init_process();

    /* 设置为当前任务 */
    set_current(init_task);

    /* 唤醒 init */
    wake_up_new_task(init_task);

    kernel_initialized = true;
    
    printk("Kernel initialization complete.\n");
}
```

### 6.3 初始化顺序依赖图

```
                    kernel_main
                         │
                         ▼
                    kernel_init
                         │
         ┌───────────────┼───────────────┐
         ▼               ▼               ▼
     mm_init       sched_init       ipc_init
         │               │               │
         ▼               ▼               ▼
    buddy_init      运行队列         消息队列
         │           初始化           初始化
         │               │
         └───────┬───────┘
                 ▼
            vfs_init ◄────┐
                 │        │
                 ▼        │ 依赖
            net_init      │
                 │        │
                 ▼        │
           driver_init ───┘
                 │
                 ▼
        create_init_process
                 │
                 ▼
            shell_run
```

### 6.4 创建 init 进程

```c
static struct task_struct *create_init_process(void)
{
    struct task_struct *task;
    struct mm_struct *mm;

    /* 分配任务结构 */
    task = alloc_task_struct();
    if (!task) {
        panic("Cannot allocate init task");
    }

    /* 分配内存描述符 */
    mm = mm_alloc();
    if (!mm) {
        free_task_struct(task);
        panic("Cannot allocate init mm");
    }

    /* 设置进程 ID */
    task->pid = 1;
    task->tgid = 1;
    task->ppid = 0;
    task->pgrp = 1;
    task->session = 1;

    /* 设置用户/组 ID (root) */
    task->uid = 0;
    task->gid = 0;

    /* 设置进程名 */
    strcpy(task->comm, "init");

    /* 关联内存描述符 */
    task->mm = mm;
    task->active_mm = mm;

    /* 设置调度参数 */
    task->state = TASK_RUNNING;
    task->prio = DEFAULT_PRIO;
    task->policy = SCHED_NORMAL;

    /* init 是自己的父进程 */
    task->real_parent = task;
    task->parent = task;
    task->group_leader = task;

    /* 初始化调度器数据 */
    sched_fork(task);

    return task;
}
```

---

## 7. 链接脚本详解

### 7.1 链接脚本结构

```ld
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* 内核物理加载地址 - 1MB */
KERNEL_PHYS_BASE = 0x100000;

SECTIONS
{
    . = KERNEL_PHYS_BASE;

    /* Multiboot 头部 - 必须在前 8KB/32KB 内 */
    .boot ALIGN(4) :
    {
        KEEP(*(.multiboot))
        . = ALIGN(8);
        KEEP(*(.multiboot2))
    }

    /* 代码段 */
    .text ALIGN(16) :
    {
        __text_start = .;
        *(.text.boot)
        *(.text._start)
        *(.text)
        *(.text.*)
        __text_end = .;
    }

    /* 只读数据段 */
    .rodata ALIGN(4K) :
    {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        __rodata_end = .;
    }

    /* 已初始化数据段 */
    .data ALIGN(4K) :
    {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    }

    /* 未初始化数据段 (BSS) */
    .bss ALIGN(4K) (NOLOAD) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    }

    /* 内核边界标记 */
    . = ALIGN(4K);
    __kernel_start = KERNEL_PHYS_BASE;
    __kernel_end = .;
    __kernel_size = __kernel_end - __kernel_start;

    /* 丢弃不需要的段 */
    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.eh_frame)
    }
}
```

### 7.2 内存布局

```
物理地址
0x00100000 ┌────────────────────┐ ◄── KERNEL_PHYS_BASE / __kernel_start
           │  .boot (Multiboot) │
           ├────────────────────┤
           │      .text         │ ◄── __text_start
           │   (代码段)         │
           │                    │ ◄── __text_end
           ├────────────────────┤ (4KB 对齐)
           │     .rodata        │ ◄── __rodata_start
           │  (只读数据)        │
           │                    │ ◄── __rodata_end
           ├────────────────────┤ (4KB 对齐)
           │      .data         │ ◄── __data_start
           │ (已初始化数据)     │
           │                    │ ◄── __data_end
           ├────────────────────┤ (4KB 对齐)
           │      .bss          │ ◄── __bss_start
           │ (未初始化数据)     │
           │                    │ ◄── __bss_end
           └────────────────────┘ ◄── __kernel_end (4KB 对齐)
```

### 7.3 关键符号

| 符号 | 用途 |
|------|------|
| `_start` | 内核入口点 |
| `__kernel_start` | 内核起始地址 |
| `__kernel_end` | 内核结束地址 |
| `__text_start/end` | 代码段边界 |
| `__bss_start/end` | BSS 段边界（需要清零） |

---

## 8. 启动调试

### 8.1 使用 QEMU 调试

```bash
# 启动 QEMU 并等待 GDB 连接
qemu-system-x86_64 -kernel build/kernel.elf -m 512M \
    -serial stdio -s -S

# 在另一个终端连接 GDB
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) break _start
(gdb) continue
```

### 8.2 常用断点

```gdb
# 32位入口点
break *0x100000

# 长模式切换后
break long_mode_start

# 内核主函数
break kernel_main

# 查看寄存器
info registers

# 查看 CR 寄存器
print/x $cr0
print/x $cr3
print/x $cr4
```

### 8.3 串口调试输出

内核使用串口 (COM1, 0x3F8) 输出调试信息：

```c
#define SERIAL_PORT 0x3F8

void serial_putc(char c)
{
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0)
        ;  // 等待发送缓冲区空
    outb(SERIAL_PORT, c);
}
```

### 8.4 常见启动问题

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| QEMU 无输出 | Multiboot 头部位置错误 | 检查链接脚本，确保头部在前 8KB |
| 立即重启 | 页表设置错误 | 使用 GDB 单步调试页表设置 |
| Triple Fault | GDT/IDT 配置错误 | 检查段描述符格式 |
| 无法进入长模式 | CPUID 检查失败 | 确认 CPU/QEMU 支持 64 位 |

---

## 参考资料

- [Intel® 64 and IA-32 Architectures Software Developer's Manual](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html)
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
- [OSDev Wiki - Setting Up Long Mode](https://wiki.osdev.org/Setting_Up_Long_Mode)
- [OSDev Wiki - Paging](https://wiki.osdev.org/Paging)
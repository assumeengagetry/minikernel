# 中断处理子系统详解

## 目录

1. [概述](#1-概述)
2. [x86_64 中断机制](#2-x86_64-中断机制)
3. [中断描述符表 (IDT)](#3-中断描述符表-idt)
4. [异常处理](#4-异常处理)
5. [硬件中断 (IRQ)](#5-硬件中断-irq)
6. [系统调用](#6-系统调用)
7. [中断处理流程](#7-中断处理流程)
8. [API 参考](#8-api-参考)

---

## 1. 概述

中断是 CPU 响应外部事件或内部异常的机制。MiniKernel 的中断子系统负责管理所有类型的中断，包括：

- **异常 (Exceptions)**：CPU 内部产生的同步中断
- **硬件中断 (IRQs)**：外部设备产生的异步中断
- **软件中断 (System Calls)**：用户程序主动触发的陷阱

### 1.1 中断分类

| 类型 | 向量号 | 来源 | 示例 |
|------|--------|------|------|
| 异常 | 0-31 | CPU 内部 | 除零、页错误、通用保护 |
| IRQ | 32-47 | 外部设备 | 时钟、键盘、硬盘 |
| 系统调用 | 128 | 软件 | syscall 指令 |

### 1.2 相关文件

| 文件 | 描述 |
|------|------|
| `kernel/interrupt/interrupt.S` | 中断处理汇编代码 |
| `kernel/include/types.h` | 类型定义 |
| `arch/x86_64/boot/boot.S` | GDT 定义 |

---

## 2. x86_64 中断机制

### 2.1 中断触发方式

```
                    ┌─────────────────────────────────────┐
                    │              CPU                    │
                    │                                     │
                    │  ┌─────────┐      ┌─────────────┐  │
     异常触发 ───────┼─►│ 异常逻辑 │      │   LAPIC     │◄─┼─── IPI
                    │  └────┬────┘      └──────┬──────┘  │
                    │       │                  │         │
                    │       └────────┬─────────┘         │
                    │                │                   │
                    │                ▼                   │
                    │         ┌─────────────┐            │
                    │         │    IDT      │            │
                    │         │  查找处理   │            │
                    │         └──────┬──────┘            │
                    │                │                   │
                    │                ▼                   │
                    │         ┌─────────────┐            │
                    │         │  中断处理   │            │
                    │         └─────────────┘            │
                    └─────────────────────────────────────┘
                                     ▲
                                     │
                    ┌────────────────┴────────────────┐
                    │            I/O APIC              │
                    │         (外部中断路由)            │
                    └────────────────┬────────────────┘
                                     │
          ┌──────────┬───────────┬───┴───┬──────────┬──────────┐
          │          │           │       │          │          │
       ┌──┴──┐   ┌──┴──┐    ┌──┴──┐  ┌──┴──┐   ┌──┴──┐   ┌──┴──┐
       │Timer│   │ KBD │    │Mouse│  │ IDE │   │ NIC │   │ ... │
       └─────┘   └─────┘    └─────┘  └─────┘   └─────┘   └─────┘
```

### 2.2 中断响应过程

当中断发生时，CPU 自动执行以下操作：

1. **保存当前状态**
   - 将 SS, RSP, RFLAGS, CS, RIP 压栈
   - 如果有错误码，压入错误码

2. **查找 IDT 表项**
   - 根据中断向量号索引 IDT
   - 获取中断处理程序地址

3. **切换特权级（如需要）**
   - 从 TSS 获取内核栈指针
   - 切换到内核栈

4. **跳转到处理程序**
   - 清除 RFLAGS.IF（禁用中断）
   - 跳转到处理程序入口

### 2.3 栈帧结构

**有错误码时**（如页错误）：

```
高地址
        ┌────────────────┐
        │       SS       │  +40
        ├────────────────┤
        │      RSP       │  +32
        ├────────────────┤
        │    RFLAGS      │  +24
        ├────────────────┤
        │       CS       │  +16
        ├────────────────┤
        │      RIP       │  +8
        ├────────────────┤
        │   Error Code   │  ◄── RSP (进入处理程序时)
        └────────────────┘
低地址
```

**无错误码时**：

```
高地址
        ┌────────────────┐
        │       SS       │  +32
        ├────────────────┤
        │      RSP       │  +24
        ├────────────────┤
        │    RFLAGS      │  +16
        ├────────────────┤
        │       CS       │  +8
        ├────────────────┤
        │      RIP       │  ◄── RSP
        └────────────────┘
低地址
```

---

## 3. 中断描述符表 (IDT)

### 3.1 IDT 结构

IDT 包含 256 个描述符，每个描述符 16 字节：

```
IDT 描述符格式（64位模式）
┌───────────────────────────────────────────────────────────────┐
│                      Offset [63:32]                          │ +12
├───────────────────────────────────────────────────────────────┤
│ Reserved │                      Offset [31:16]               │ +8
├────┬────┬────┬────┬────┬────────────────────────────────────┤
│ P  │DPL │ 0  │Type│ 0  │  IST  │        Reserved             │ +4
├────┴────┴────┴────┴────┴───────┴────────────────────────────┤
│        Segment Selector        │        Offset [15:0]        │ +0
└────────────────────────────────┴─────────────────────────────┘
```

**字段说明**：

| 字段 | 位 | 描述 |
|------|-----|------|
| Offset | 0-15, 48-63 | 处理程序地址的低 16 位和高 32 位 |
| Selector | 16-31 | 代码段选择子 |
| IST | 32-34 | 中断栈表索引（0 = 不使用） |
| Type | 40-43 | 门类型（0xE = 64位中断门，0xF = 64位陷阱门） |
| DPL | 45-46 | 描述符特权级 |
| P | 47 | 存在位 |

### 3.2 门类型

| 类型值 | 名称 | 描述 |
|--------|------|------|
| 0xE | 64位中断门 | 自动清除 IF 标志 |
| 0xF | 64位陷阱门 | 不改变 IF 标志 |

### 3.3 IDT 加载

```asm
# IDT 描述符
idt_desc:
    .word idt_end - idt - 1    # IDT 限长
    .quad idt                   # IDT 基址

# 加载 IDT
idt_flush:
    lidt (%rdi)
    ret
```

---

## 4. 异常处理

### 4.1 CPU 异常列表

| 向量 | 助记符 | 名称 | 类型 | 错误码 |
|------|--------|------|------|--------|
| 0 | #DE | 除法错误 | Fault | 无 |
| 1 | #DB | 调试异常 | Fault/Trap | 无 |
| 2 | - | NMI 中断 | Interrupt | 无 |
| 3 | #BP | 断点 | Trap | 无 |
| 4 | #OF | 溢出 | Trap | 无 |
| 5 | #BR | 边界检查 | Fault | 无 |
| 6 | #UD | 无效操作码 | Fault | 无 |
| 7 | #NM | 设备不可用 | Fault | 无 |
| 8 | #DF | 双重错误 | Abort | 有 (0) |
| 9 | - | 协处理器段越界 | Fault | 无 |
| 10 | #TS | 无效 TSS | Fault | 有 |
| 11 | #NP | 段不存在 | Fault | 有 |
| 12 | #SS | 栈段错误 | Fault | 有 |
| 13 | #GP | 通用保护错误 | Fault | 有 |
| 14 | #PF | 页错误 | Fault | 有 |
| 15 | - | 保留 | - | - |
| 16 | #MF | x87 浮点异常 | Fault | 无 |
| 17 | #AC | 对齐检查 | Fault | 有 (0) |
| 18 | #MC | 机器检查 | Abort | 无 |
| 19 | #XM | SIMD 浮点异常 | Fault | 无 |
| 20 | #VE | 虚拟化异常 | Fault | 无 |
| 21-31 | - | 保留 | - | - |

### 4.2 异常处理宏

```asm
# 无错误码的异常
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    cli
    pushq $0        # 压入虚拟错误码（保持栈对齐）
    pushq $\num     # 压入中断号
    jmp isr_common_stub
.endm

# 有错误码的异常
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    cli
    pushq $\num     # 压入中断号（错误码已由 CPU 压入）
    jmp isr_common_stub
.endm

# 定义所有异常处理程序
ISR_NOERRCODE 0     # 除法错误
ISR_NOERRCODE 1     # 调试异常
ISR_NOERRCODE 2     # NMI
ISR_NOERRCODE 3     # 断点
ISR_NOERRCODE 4     # 溢出
ISR_NOERRCODE 5     # 边界检查
ISR_NOERRCODE 6     # 无效操作码
ISR_NOERRCODE 7     # 设备不可用
ISR_ERRCODE   8     # 双重错误
ISR_NOERRCODE 9     # 协处理器段越界
ISR_ERRCODE   10    # 无效 TSS
ISR_ERRCODE   11    # 段不存在
ISR_ERRCODE   12    # 栈段错误
ISR_ERRCODE   13    # 通用保护错误
ISR_ERRCODE   14    # 页错误
# ... 继续定义其他异常
```

### 4.3 异常公共处理存根

```asm
isr_common_stub:
    # 保存所有通用寄存器
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # 保存段寄存器
    movq %ds, %rax
    pushq %rax
    movq %es, %rax
    pushq %rax
    movq %fs, %rax
    pushq %rax
    movq %gs, %rax
    pushq %rax

    # 切换到内核数据段
    movq $0x10, %rax
    movq %rax, %ds
    movq %rax, %es
    movq %rax, %fs
    movq %rax, %gs

    # 调用 C 语言异常处理程序
    movq %rsp, %rdi     # 传递寄存器结构指针
    call isr_handler

    # 恢复段寄存器
    popq %rax
    movq %rax, %gs
    popq %rax
    movq %rax, %fs
    popq %rax
    movq %rax, %es
    popq %rax
    movq %rax, %ds

    # 恢复通用寄存器
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # 清理栈上的错误码和中断号
    addq $16, %rsp

    # 中断返回
    iretq
```

### 4.4 页错误处理

页错误是最常见的异常，其错误码包含额外信息：

```
页错误错误码格式
┌───────────────────────────────────────────────┬───┬───┬───┬───┬───┐
│                    保留                        │I/D│RSV│U/S│W/R│ P │
└───────────────────────────────────────────────┴───┴───┴───┴───┴───┘
                                                  4   3   2   1   0
```

| 位 | 名称 | 值=0 | 值=1 |
|----|------|------|------|
| 0 | P | 页不存在 | 保护违规 |
| 1 | W/R | 读操作 | 写操作 |
| 2 | U/S | 内核模式 | 用户模式 |
| 3 | RSV | - | 保留位被置位 |
| 4 | I/D | 数据访问 | 指令获取 |

**CR2 寄存器**：存储引发页错误的线性地址。

```asm
page_fault_handler:
    # 保存寄存器...
    
    # 获取页错误地址
    movq %cr2, %rdi         # 第一个参数：错误地址
    
    # 获取错误码
    movq 120(%rsp), %rsi    # 第二个参数：错误码
    
    # 调用 C 处理程序
    call do_page_fault
    
    # 恢复寄存器并返回...
```

---

## 5. 硬件中断 (IRQ)

### 5.1 8259 PIC 中断控制器

传统 PC 使用两个级联的 8259 PIC：

```
                    ┌─────────────────┐
                    │      CPU        │
                    │    INTR 引脚    │
                    └────────┬────────┘
                             │
                    ┌────────┴────────┐
                    │   主 PIC (8259A) │
                    │   端口: 0x20/21  │
                    └─┬──┬──┬──┬──┬──┬─┘
                      │  │  │  │  │  │
    IRQ0 ─────────────┘  │  │  │  │  └── IRQ7
    (Timer)              │  │  │  │       (Parallel)
                         │  │  │  │
    IRQ1 ────────────────┘  │  │  └── IRQ6
    (Keyboard)              │  │       (Floppy)
                            │  │
    IRQ2 ───────────────────┘  └── IRQ5
    (Cascade)                      (Sound)
         │
         └──────────────────┐
                    ┌───────┴────────┐
                    │  从 PIC (8259A) │
                    │  端口: 0xA0/A1  │
                    └─┬──┬──┬──┬──┬──┬─┘
                      │  │  │  │  │  │
    IRQ8 ─────────────┘  │  │  │  │  └── IRQ15
    (RTC)                │  │  │  │       (Reserved)
                         │  │  │  │
    IRQ9 ────────────────┘  │  │  └── IRQ14
    (ACPI)                  │  │       (IDE Primary)
                            │  │
    IRQ10 ──────────────────┘  └── IRQ13
    (Available)                    (FPU)
```

### 5.2 IRQ 向量映射

默认映射（BIOS 设置）与重映射：

| IRQ | 默认向量 | 重映射向量 | 设备 |
|-----|----------|------------|------|
| 0 | 0x08 | 0x20 (32) | 可编程间隔定时器 |
| 1 | 0x09 | 0x21 (33) | 键盘 |
| 2 | 0x0A | 0x22 (34) | 级联（从 PIC） |
| 3 | 0x0B | 0x23 (35) | 串口 2 |
| 4 | 0x0C | 0x24 (36) | 串口 1 |
| 5 | 0x0D | 0x25 (37) | 并口 2 |
| 6 | 0x0E | 0x26 (38) | 软驱 |
| 7 | 0x0F | 0x27 (39) | 并口 1 |
| 8 | 0x70 | 0x28 (40) | 实时时钟 |
| 9 | 0x71 | 0x29 (41) | ACPI |
| 10 | 0x72 | 0x2A (42) | 可用 |
| 11 | 0x73 | 0x2B (43) | 可用 |
| 12 | 0x74 | 0x2C (44) | PS/2 鼠标 |
| 13 | 0x75 | 0x2D (45) | FPU |
| 14 | 0x76 | 0x2E (46) | 主 IDE |
| 15 | 0x77 | 0x2F (47) | 从 IDE |

### 5.3 IRQ 处理宏

```asm
# IRQ 处理程序宏
.macro IRQ num, isr_num
.global irq\num
irq\num:
    cli
    pushq $0            # 虚拟错误码
    pushq $\isr_num     # 中断向量号
    jmp irq_common_stub
.endm

# 定义所有 IRQ 处理程序
IRQ 0, 32    # 时钟
IRQ 1, 33    # 键盘
IRQ 2, 34    # 级联
IRQ 3, 35    # 串口 2
IRQ 4, 36    # 串口 1
IRQ 5, 37    # 并口 2
IRQ 6, 38    # 软驱
IRQ 7, 39    # 并口 1
IRQ 8, 40    # 实时时钟
IRQ 9, 41    # ACPI
IRQ 10, 42   # 可用
IRQ 11, 43   # 可用
IRQ 12, 44   # 鼠标
IRQ 13, 45   # FPU
IRQ 14, 46   # 主 IDE
IRQ 15, 47   # 从 IDE
```

### 5.4 IRQ 公共处理存根

```asm
irq_common_stub:
    # 保存寄存器（与 isr_common_stub 相同）
    pushq %rax
    pushq %rbx
    # ... 省略其他寄存器 ...

    # 切换到内核数据段
    movq $0x10, %rax
    movq %rax, %ds
    movq %rax, %es
    movq %rax, %fs
    movq %rax, %gs

    # 调用 C 语言 IRQ 处理程序
    movq %rsp, %rdi
    call irq_handler

    # 恢复寄存器
    # ... 省略 ...

    # 清理栈并返回
    addq $16, %rsp
    iretq
```

### 5.5 发送 EOI (End of Interrupt)

IRQ 处理完成后必须发送 EOI 信号：

```asm
# 时钟中断处理
timer_interrupt_handler:
    # 保存寄存器...
    
    # 调用 C 处理程序
    call timer_interrupt

    # 向主 PIC 发送 EOI
    movb $0x20, %al
    outb %al, $0x20
    
    # 如果是从 PIC 的中断（IRQ 8-15），还需要向从 PIC 发送 EOI
    # outb %al, $0xA0
    
    # 恢复寄存器并返回...
```

---

## 6. 系统调用

### 6.1 系统调用机制

x86_64 使用 `syscall` 指令进行系统调用，比传统的 `int 0x80` 更高效。

**syscall 指令行为**：
1. 将 RIP 保存到 RCX
2. 将 RFLAGS 保存到 R11
3. 从 STAR MSR 加载 CS 和 SS
4. 从 LSTAR MSR 加载 RIP
5. 使用 SFMASK MSR 屏蔽 RFLAGS

### 6.2 调用约定

| 寄存器 | 用途 |
|--------|------|
| RAX | 系统调用号 / 返回值 |
| RDI | 参数 1 |
| RSI | 参数 2 |
| RDX | 参数 3 |
| R10 | 参数 4（注意：不是 RCX） |
| R8 | 参数 5 |
| R9 | 参数 6 |
| RCX | 被 syscall 覆盖（保存 RIP） |
| R11 | 被 syscall 覆盖（保存 RFLAGS） |

### 6.3 系统调用入口

```asm
.global syscall_entry
syscall_entry:
    # 交换用户栈和内核栈
    swapgs

    # 保存用户寄存器
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # 保存系统调用参数
    pushq %rdi      # arg0
    pushq %rsi      # arg1
    pushq %rdx      # arg2
    pushq %r10      # arg3
    pushq %r8       # arg4
    pushq %r9       # arg5
    pushq %rax      # 系统调用号

    # 检查系统调用号是否有效
    cmpq $NR_syscalls, %rax
    jae syscall_invalid

    # 调用 do_syscall(syscall_nr, arg0, arg1, arg2, arg3, arg4, arg5)
    movq %rax, %rdi     # syscall_nr
    movq 48(%rsp), %rsi # arg0
    movq 40(%rsp), %rdx # arg1
    movq 32(%rsp), %rcx # arg2
    movq 24(%rsp), %r8  # arg3
    movq 16(%rsp), %r9  # arg4
    pushq 8(%rsp)       # arg5

    call do_syscall

    # 清理栈
    addq $8, %rsp
    addq $56, %rsp

    # 恢复用户寄存器
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    # 恢复用户 GS
    swapgs

    # 返回用户空间
    sysretq

syscall_invalid:
    # 无效系统调用，返回 -ENOSYS
    movq $-38, %rax
    # ... 恢复并返回 ...
```

### 6.4 系统调用分发

```c
long do_syscall(unsigned long nr, unsigned long arg0, unsigned long arg1,
                unsigned long arg2, unsigned long arg3, unsigned long arg4,
                unsigned long arg5)
{
    switch (nr) {
        case __NR_read:
            return sys_read(arg0, (char *)arg1, arg2);
        case __NR_write:
            return sys_write(arg0, (const char *)arg1, arg2);
        case __NR_open:
            return sys_open((const char *)arg0, arg1, arg2);
        case __NR_close:
            return sys_close(arg0);
        case __NR_getpid:
            return sys_getpid();
        case __NR_fork:
            return sys_fork();
        case __NR_exit:
            sys_exit(arg0);
            return 0;  /* 不会到达 */
        case __NR_sched_yield:
            return sys_sched_yield();
        /* ... 更多系统调用 ... */
        default:
            return -ENOSYS;
    }
}
```

### 6.5 支持的系统调用

| 编号 | 名称 | 描述 | 参数 |
|------|------|------|------|
| 0 | read | 读取文件 | fd, buf, count |
| 1 | write | 写入文件 | fd, buf, count |
| 2 | open | 打开文件 | path, flags, mode |
| 3 | close | 关闭文件 | fd |
| 39 | getpid | 获取进程 ID | - |
| 56 | clone | 创建进程/线程 | flags, stack, ... |
| 57 | fork | 创建子进程 | - |
| 58 | vfork | 创建子进程 | - |
| 59 | execve | 执行程序 | path, argv, envp |
| 60 | exit | 退出进程 | status |
| 61 | wait4 | 等待子进程 | pid, status, options, rusage |
| 62 | kill | 发送信号 | pid, sig |
| 24 | sched_yield | 让出 CPU | - |
| 12 | brk | 调整堆大小 | addr |
| 9 | mmap | 内存映射 | addr, len, prot, flags, fd, offset |
| 11 | munmap | 解除映射 | addr, len |
| 99 | sysinfo | 获取系统信息 | info |

---

## 7. 中断处理流程

### 7.1 完整处理流程图

```
                    中断/异常发生
                          │
                          ▼
              ┌───────────────────────┐
              │   CPU 自动保存状态     │
              │  SS, RSP, RFLAGS,     │
              │     CS, RIP           │
              └───────────┬───────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │    查询 IDT 表项       │
              │  根据向量号索引 IDT    │
              └───────────┬───────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │   特权级检查/切换      │
              │  如需要，切换到内核栈   │
              └───────────┬───────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │   跳转到处理程序入口   │
              │   isr_common_stub /   │
              │   irq_common_stub     │
              └───────────┬───────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │   保存所有寄存器       │
              │   切换到内核数据段     │
              └───────────┬───────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │   调用 C 处理程序      │
              │   isr_handler /       │
              │   irq_handler         │
              └───────────┬───────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │   恢复所有寄存器       │
              │   清理栈帧            │
              └───────────┬───────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │       iretq           │
              │   返回被中断的代码     │
              └───────────────────────┘
```

### 7.2 中断处理时序

```
时间线
────────────────────────────────────────────────────────────────────►

  │         │              │                │              │
  │ 中断发生 │   保存上下文   │   执行处理程序   │   恢复上下文  │
  │         │              │                │              │
  ▼         ▼              ▼                ▼              ▼
──┼─────────┼──────────────┼────────────────┼──────────────┼──────►
  │         │              │                │              │
  │ 硬件    │   汇编存根    │    C 处理函数   │   汇编存根   │
  │ 触发    │   入口        │                │   出口       │
```

### 7.3 中断嵌套

MiniKernel 当前不支持中断嵌套（进入中断处理时 IF=0）。如需支持嵌套中断，需要：

1. 在适当时机调用 `sti` 重新启用中断
2. 使用中断栈表（IST）防止栈溢出
3. 正确处理中断优先级

---

## 8. API 参考

### 8.1 中断控制

```c
/* 启用中断 */
void enable_interrupts(void);
static inline void local_irq_enable(void) {
    __asm__ volatile("sti" ::: "memory");
}

/* 禁用中断 */
void disable_interrupts(void);
static inline void local_irq_disable(void) {
    __asm__ volatile("cli" ::: "memory");
}

/* 保存中断状态并禁用 */
static inline unsigned long local_irq_save(void) {
    unsigned long flags;
    __asm__ volatile(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

/* 恢复中断状态 */
static inline void local_irq_restore(unsigned long flags) {
    __asm__ volatile(
        "pushq %0\n\t"
        "popfq"
        :
        : "r"(flags)
        : "memory"
    );
}

/* 检查中断是否启用 */
static inline int interrupts_enabled(void) {
    unsigned long flags;
    __asm__ volatile("pushfq; popq %0" : "=r"(flags));
    return (flags & 0x200) != 0;
}
```

### 8.2 IDT 操作

```c
/* 加载 IDT */
void idt_flush(uint64_t idt_ptr);

/* 设置 IDT 表项 */
void set_idt_entry(int vector, void *handler, uint16_t selector, 
                   uint8_t type_attr, uint8_t ist);

/* 初始化 IDT */
void idt_init(void);
```

### 8.3 中断处理程序注册

```c
/* 注册异常处理程序 */
typedef void (*exception_handler_t)(struct pt_regs *regs, 
                                    unsigned long error_code);
void register_exception_handler(int vector, exception_handler_t handler);

/* 注册 IRQ 处理程序 */
typedef void (*irq_handler_t)(int irq, void *data);
int request_irq(unsigned int irq, irq_handler_t handler, 
                unsigned long flags, const char *name, void *data);
void free_irq(unsigned int irq, void *data);
```

### 8.4 PIC 控制

```c
/* 初始化 8259 PIC */
void pic_init(void);

/* 重映射 IRQ */
void pic_remap(int offset1, int offset2);

/* 发送 EOI */
void pic_send_eoi(unsigned char irq);

/* 屏蔽/解除屏蔽 IRQ */
void pic_set_mask(unsigned char irq);
void pic_clear_mask(unsigned char irq);

/* 获取 IRQ 服务寄存器 */
uint16_t pic_get_isr(void);
uint16_t pic_get_irr(void);
```

---

## 9. 调试技巧

### 9.1 常见问题排查

| 症状 | 可能原因 | 排查方法 |
|------|----------|----------|
| Triple Fault | IDT 配置错误 | 检查 IDT 表项格式、段选择子 |
| 中断不触发 | PIC 未正确初始化 | 检查 PIC 重映射、掩码设置 |
| 中断后死机 | EOI 未发送 | 确认 IRQ 处理末尾发送 EOI |
| 页错误循环 | 页错误处理程序本身触发页错误 | 使用 IST 隔离栈 |
| 系统调用返回错误值 | 寄存器保存/恢复错误 | 检查汇编存根中的寄存器顺序 |

### 9.2 使用 QEMU 调试中断

```bash
# 显示中断信息
(qemu) info irq

# 显示 IDT
(qemu) info idt

# 跟踪中断
qemu-system-x86_64 -d int -kernel kernel.elf

# 在特定中断处暂停
(gdb) catch signal SIGTRAP
```

### 9.3 中断调试输出

```c
void dump_regs(struct pt_regs *regs)
{
    printk("RAX=%016lx RBX=%016lx RCX=%016lx\n", 
           regs->rax, regs->rbx, regs->rcx);
    printk("RDX=%016lx RSI=%016lx RDI=%016lx\n",
           regs->rdx, regs->rsi, regs->rdi);
    printk("RBP=%016lx RSP=%016lx RIP=%016lx\n",
           regs->rbp, regs->rsp, regs->rip);
    printk("R8 =%016lx R9 =%016lx R10=%016lx\n",
           regs->r8, regs->r9, regs->r10);
    printk("R11=%016lx R12=%016lx R13=%016lx\n",
           regs->r11, regs->r12, regs->r13);
    printk("R14=%016lx R15=%016lx\n",
           regs->r14, regs->r15);
    printk("CS=%04lx SS=%04lx RFLAGS=%016lx\n",
           regs->cs, regs->ss, regs->eflags);
}
```

---

## 参考资料

- [Intel® 64 and IA-32 Architectures Software Developer's Manual, Volume 3](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html) - 中断和异常处理
- [OSDev Wiki - Interrupts](https://wiki.osdev.org/Interrupts)
- [OSDev Wiki - 8259 PIC](https://wiki.osdev.org/8259_PIC)
- [OSDev Wiki - APIC](https://wiki.osdev.org/APIC)
- [AMD64 Architecture Programmer's Manual](https://developer.amd.com/resources/developer-guides-manuals/) - 系统调用机制
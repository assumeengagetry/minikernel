# x86_64 上下文切换汇编代码
# 这个文件包含了进程/线程上下文切换的底层实现

# Mark stack as non-executable (fix linker warning)
.section .note.GNU-stack,"",@progbits

.text
.global switch_to
.global ret_from_fork

# 上下文切换函数
# void switch_to(struct task_struct *prev, struct task_struct *next)
# RDI = prev, RSI = next
switch_to:
    # 保存当前进程的寄存器状态
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushfq

    # 保存当前栈指针到 prev->thread.sp
    movq %rsp, 0x1000(%rdi)    # 假设 thread.sp 在 task_struct 偏移 0x1000

    # 切换到新进程的栈
    movq 0x1000(%rsi), %rsp    # 从 next->thread.sp 恢复栈指针

    # 恢复新进程的寄存器状态
    popfq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    # 返回到新进程
    ret

# 从 fork 返回的特殊处理
ret_from_fork:
    # 设置返回值为 0 (子进程)
    movq $0, %rax

    # 恢复寄存器状态
    popfq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    # 返回到用户空间
    ret

# 内核线程启动函数
# void kernel_thread_helper(void)
.global kernel_thread_helper
kernel_thread_helper:
    # 清除方向标志
    cld

    # 调用线程函数
    # 线程函数指针在 %rbx 中
    # 线程参数在 %r12 中
    movq %r12, %rdi    # 第一个参数
    callq *%rbx        # 调用线程函数

    # 如果线程函数返回，退出线程
    movq %rax, %rdi    # 退出码
    callq do_exit

    # 永远不应该到达这里
    ud2

# 用户模式返回
# void ret_from_sys_call(void)
.global ret_from_sys_call
ret_from_sys_call:
    # 检查是否需要重新调度
    testl $0x1, %gs:0x18    # 检查 need_resched 标志
    jnz need_resched

    # 检查是否有信号待处理
    testl $0x2, %gs:0x18    # 检查 sigpending 标志
    jnz signal_pending

    # 恢复用户模式寄存器
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rax
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi

    # 返回用户空间
    sysretq

need_resched:
    # 调用调度器
    callq schedule
    jmp ret_from_sys_call

signal_pending:
    # 处理信号
    callq do_signal
    jmp ret_from_sys_call

# 保存用户模式寄存器
# void save_user_regs(struct pt_regs *regs)
.global save_user_regs
save_user_regs:
    # 保存通用寄存器
    movq %rax, 0x50(%rdi)   # pt_regs->rax
    movq %rbx, 0x28(%rdi)   # pt_regs->rbx
    movq %rcx, 0x58(%rdi)   # pt_regs->rcx
    movq %rdx, 0x60(%rdi)   # pt_regs->rdx
    movq %rsi, 0x68(%rdi)   # pt_regs->rsi
    movq %rbp, 0x20(%rdi)   # pt_regs->rbp
    movq %r8, 0x48(%rdi)    # pt_regs->r8
    movq %r9, 0x40(%rdi)    # pt_regs->r9
    movq %r10, 0x38(%rdi)   # pt_regs->r10
    movq %r11, 0x30(%rdi)   # pt_regs->r11
    movq %r12, 0x18(%rdi)   # pt_regs->r12
    movq %r13, 0x10(%rdi)   # pt_regs->r13
    movq %r14, 0x08(%rdi)   # pt_regs->r14
    movq %r15, 0x00(%rdi)   # pt_regs->r15

    # 保存指令指针和栈指针
    movq 0x80(%rsp), %rax   # 从栈中获取 RIP
    movq %rax, 0x70(%rdi)   # pt_regs->rip
    movq 0x98(%rsp), %rax   # 从栈中获取 RSP
    movq %rax, 0x88(%rdi)   # pt_regs->rsp

    # 保存标志寄存器
    movq 0x90(%rsp), %rax   # 从栈中获取 EFLAGS
    movq %rax, 0x78(%rdi)   # pt_regs->eflags

    # 保存段寄存器
    movq 0x88(%rsp), %rax   # 从栈中获取 CS
    movq %rax, 0x80(%rdi)   # pt_regs->cs
    movq 0xA0(%rsp), %rax   # 从栈中获取 SS
    movq %rax, 0x90(%rdi)   # pt_regs->ss

    ret

# 恢复用户模式寄存器
# void restore_user_regs(struct pt_regs *regs)
.global restore_user_regs
restore_user_regs:
    # 恢复通用寄存器
    movq 0x50(%rdi), %rax   # pt_regs->rax
    movq 0x28(%rdi), %rbx   # pt_regs->rbx
    movq 0x58(%rdi), %rcx   # pt_regs->rcx
    movq 0x60(%rdi), %rdx   # pt_regs->rdx
    movq 0x68(%rdi), %rsi   # pt_regs->rsi
    movq 0x20(%rdi), %rbp   # pt_regs->rbp
    movq 0x48(%rdi), %r8    # pt_regs->r8
    movq 0x40(%rdi), %r9    # pt_regs->r9
    movq 0x38(%rdi), %r10   # pt_regs->r10
    movq 0x30(%rdi), %r11   # pt_regs->r11
    movq 0x18(%rdi), %r12   # pt_regs->r12
    movq 0x10(%rdi), %r13   # pt_regs->r13
    movq 0x08(%rdi), %r14   # pt_regs->r14
    movq 0x00(%rdi), %r15   # pt_regs->r15

    # 恢复指令指针和栈指针到栈中
    movq 0x70(%rdi), %rax   # pt_regs->rip
    movq %rax, 0x80(%rsp)   # 恢复到栈中
    movq 0x88(%rdi), %rax   # pt_regs->rsp
    movq %rax, 0x98(%rsp)   # 恢复到栈中

    # 恢复标志寄存器
    movq 0x78(%rdi), %rax   # pt_regs->eflags
    movq %rax, 0x90(%rsp)   # 恢复到栈中

    # 恢复段寄存器
    movq 0x80(%rdi), %rax   # pt_regs->cs
    movq %rax, 0x88(%rsp)   # 恢复到栈中
    movq 0x90(%rdi), %rax   # pt_regs->ss
    movq %rax, 0xA0(%rsp)   # 恢复到栈中

    ret

# FPU 状态保存
# void save_fpu_state(struct fpu_state *fpu)
.global save_fpu_state
save_fpu_state:
    # 检查 FPU 是否可用
    mov %cr0, %rax
    test $0x8, %rax         # 检查 TS 位
    jnz fpu_not_available

    # 保存 FPU 状态
    fxsave (%rdi)
    ret

fpu_not_available:
    # FPU 不可用，返回错误
    movq $-1, %rax
    ret

# FPU 状态恢复
# void restore_fpu_state(struct fpu_state *fpu)
.global restore_fpu_state
restore_fpu_state:
    # 检查 FPU 是否可用
    mov %cr0, %rax
    test $0x8, %rax         # 检查 TS 位
    jnz fpu_not_available

    # 恢复 FPU 状态
    fxrstor (%rdi)
    ret

# 原子操作
# int atomic_cmpxchg(int *ptr, int old, int new)
.global atomic_cmpxchg
atomic_cmpxchg:
    movl %esi, %eax         # old 值
    lock cmpxchgl %edx, (%rdi)
    ret

# 原子加法
# int atomic_add(int *ptr, int val)
.global atomic_add
atomic_add:
    movl %esi, %eax         # val
    lock xaddl %eax, (%rdi)
    addl %esi, %eax         # 返回新值
    ret

# 原子减法
# int atomic_sub(int *ptr, int val)
.global atomic_sub
atomic_sub:
    negl %esi               # -val
    movl %esi, %eax
    lock xaddl %eax, (%rdi)
    addl %esi, %eax         # 返回新值
    ret

# 内存屏障
# void memory_barrier(void)
.global memory_barrier
memory_barrier:
    mfence
    ret

# 读内存屏障
# void read_barrier(void)
.global read_barrier
read_barrier:
    lfence
    ret

# 写内存屏障
# void write_barrier(void)
.global write_barrier
write_barrier:
    sfence
    ret

# 获取时间戳计数器
# uint64_t rdtsc(void)
.global rdtsc
rdtsc:
    rdtsc
    shlq $32, %rdx
    orq %rdx, %rax
    ret

# 等待指令
# void cpu_pause(void)
.global cpu_pause
cpu_pause:
    pause
    ret

# 停机指令
# void cpu_halt(void)
.global cpu_halt
cpu_halt:
    hlt
    ret

# 无操作循环
# void cpu_relax(void)
.global cpu_relax
cpu_relax:
    pause
    ret

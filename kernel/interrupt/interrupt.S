# x86_64 中断处理汇编代码
# 这个文件包含了中断和异常处理的底层实现

.text
.code64

# 常量定义
.set ENOSYS, 38
.set NR_syscalls, 256

.global idt_flush
.global isr_common_stub
.global irq_common_stub

# 中断描述符表刷新
# void idt_flush(uint64_t idt_ptr)
idt_flush:
    lidt (%rdi)
    ret

# 异常处理宏
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    cli
    pushq $0        # 推入虚拟错误码
    pushq $\num     # 推入中断号
    jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
.global isr\num
isr\num:
    cli
    pushq $\num     # 推入中断号
    jmp isr_common_stub
.endm

# 中断请求处理宏
.macro IRQ num, isr_num
.global irq\num
irq\num:
    cli
    pushq $0        # 推入虚拟错误码
    pushq $\isr_num # 推入中断号
    jmp irq_common_stub
.endm

# 定义异常处理程序
ISR_NOERRCODE 0     # 除法错误
ISR_NOERRCODE 1     # 调试异常
ISR_NOERRCODE 2     # NMI中断
ISR_NOERRCODE 3     # 断点异常
ISR_NOERRCODE 4     # 溢出异常
ISR_NOERRCODE 5     # 边界检查异常
ISR_NOERRCODE 6     # 无效操作码异常
ISR_NOERRCODE 7     # 协处理器不可用异常
ISR_ERRCODE   8     # 双重错误异常
ISR_NOERRCODE 9     # 协处理器段溢出异常
ISR_ERRCODE   10    # 无效TSS异常
ISR_ERRCODE   11    # 段不存在异常
ISR_ERRCODE   12    # 栈段错误异常
ISR_ERRCODE   13    # 一般保护异常
ISR_ERRCODE   14    # 页错误异常
ISR_NOERRCODE 15    # 保留
ISR_NOERRCODE 16    # 浮点异常
ISR_ERRCODE   17    # 对齐检查异常
ISR_NOERRCODE 18    # 机器检查异常
ISR_NOERRCODE 19    # SIMD浮点异常
ISR_NOERRCODE 20    # 虚拟化异常
ISR_NOERRCODE 21    # 保留
ISR_NOERRCODE 22    # 保留
ISR_NOERRCODE 23    # 保留
ISR_NOERRCODE 24    # 保留
ISR_NOERRCODE 25    # 保留
ISR_NOERRCODE 26    # 保留
ISR_NOERRCODE 27    # 保留
ISR_NOERRCODE 28    # 保留
ISR_NOERRCODE 29    # 保留
ISR_NOERRCODE 30    # 保留
ISR_NOERRCODE 31    # 保留

# 定义IRQ处理程序
IRQ 0, 32    # 时钟中断
IRQ 1, 33    # 键盘中断
IRQ 2, 34    # 级联中断
IRQ 3, 35    # 串口2中断
IRQ 4, 36    # 串口1中断
IRQ 5, 37    # 并口2中断
IRQ 6, 38    # 软盘中断
IRQ 7, 39    # 并口1中断
IRQ 8, 40    # 实时时钟中断
IRQ 9, 41    # 保留
IRQ 10, 42   # 保留
IRQ 11, 43   # 保留
IRQ 12, 44   # 鼠标中断
IRQ 13, 45   # 协处理器中断
IRQ 14, 46   # 硬盘中断
IRQ 15, 47   # 保留

# 异常处理公共存根
isr_common_stub:
    # 保存所有寄存器
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

    # 调用异常处理程序
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

    # 恢复所有寄存器
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

# IRQ处理公共存根
irq_common_stub:
    # 保存所有寄存器
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

    # 调用IRQ处理程序
    movq %rsp, %rdi     # 传递寄存器结构指针
    call irq_handler

    # 恢复段寄存器
    popq %rax
    movq %rax, %gs
    popq %rax
    movq %rax, %fs
    popq %rax
    movq %rax, %es
    popq %rax
    movq %rax, %ds

    # 恢复所有寄存器
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

# 系统调用入口
.global syscall_entry
syscall_entry:
    # 交换用户栈和内核栈
    swapgs

    # 保存用户模式寄存器
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # 保存系统调用参数
    pushq %rdi  # arg0
    pushq %rsi  # arg1
    pushq %rdx  # arg2
    pushq %r10  # arg3 (注意：系统调用使用r10而不是rcx)
    pushq %r8   # arg4
    pushq %r9   # arg5
    pushq %rax  # 系统调用号

    # 检查系统调用号是否有效
    cmpq $NR_syscalls, %rax
    jae syscall_invalid

    # 准备参数调用 do_syscall
    # do_syscall(syscall_nr, arg0, arg1, arg2, arg3, arg4, arg5)
    movq %rax, %rdi     # syscall_nr
    movq 48(%rsp), %rsi # arg0 (original rdi)
    movq 40(%rsp), %rdx # arg1 (original rsi)
    movq 32(%rsp), %rcx # arg2 (original rdx)
    movq 24(%rsp), %r8  # arg3 (original r10)
    movq 16(%rsp), %r9  # arg4 (original r8)
    # arg5 需要通过栈传递
    pushq 8(%rsp)       # arg5 (original r9)

    call do_syscall

    # 清理栈上的 arg5
    addq $8, %rsp

    # 恢复栈
    addq $56, %rsp

    # 恢复用户模式寄存器
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    # 恢复用户GS
    swapgs

    # 返回用户空间
    sysretq

syscall_invalid:
    # 无效系统调用，返回 -ENOSYS
    movq $-38, %rax     # -ENOSYS = -38
    addq $56, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    swapgs
    sysretq

# 页错误处理
.global page_fault_handler
page_fault_handler:
    # 保存寄存器
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

    # 获取页错误地址
    movq %cr2, %rdi

    # 获取错误码 (在保存的寄存器之后)
    movq 120(%rsp), %rsi

    # 调用页错误处理程序
    call do_page_fault

    # 恢复寄存器
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

    # 清理栈上的错误码
    addq $8, %rsp

    # 中断返回
    iretq

# 时钟中断处理
.global timer_interrupt_handler
timer_interrupt_handler:
    # 保存寄存器
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

    # 调用时钟中断处理程序
    call timer_interrupt

    # 向PIC发送EOI
    movb $0x20, %al
    outb %al, $0x20

    # 恢复寄存器
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

    # 中断返回
    iretq

# 键盘中断处理
.global keyboard_interrupt_handler
keyboard_interrupt_handler:
    # 保存寄存器
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

    # 调用键盘中断处理程序
    call keyboard_interrupt

    # 向PIC发送EOI
    movb $0x20, %al
    outb %al, $0x20

    # 恢复寄存器
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

    # 中断返回
    iretq

# 设置中断标志
.global enable_interrupts
enable_interrupts:
    sti
    ret

# 清除中断标志
.global disable_interrupts
disable_interrupts:
    cli
    ret

# 检查中断是否启用
.global interrupts_enabled
interrupts_enabled:
    pushfq
    popq %rax
    andq $0x200, %rax
    shrq $9, %rax
    ret

# 中断返回
.global interrupt_return
interrupt_return:
    iretq

# 默认中断/异常处理程序 (如果C代码未定义)
.weak isr_handler
isr_handler:
    ret

.weak irq_handler
irq_handler:
    ret

.weak do_page_fault
do_page_fault:
    ret

.weak timer_interrupt
timer_interrupt:
    ret

.weak keyboard_interrupt
keyboard_interrupt:
    ret

.weak do_syscall
do_syscall:
    movq $-38, %rax     # -ENOSYS
    ret

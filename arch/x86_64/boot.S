# x86_64 简化版内核引导代码

.section .bss
.align 16
stack_bottom:
    .space 16384  # 16KB 栈空间
stack_top:

# 页表结构 (4KB 对齐)
.align 4096
boot_pml4:
    .space 4096
boot_pdpt:
    .space 4096
boot_pdt:
    .space 4096

.section .text
.global _start
.global gdt64
.global gdt64_desc

# 内核入口点
_start:
    # 设置栈指针
    movq $stack_top, %rsp

    # 保存 multiboot2 信息
    pushq %rbx    # multiboot2 信息结构指针
    pushq %rax    # multiboot2 魔数

    # 检查是否支持 CPUID
    call check_cpuid

    # 检查是否支持长模式
    call check_long_mode

    # 设置分页
    call setup_page_tables
    call enable_paging

    # 加载 GDT
    lgdt gdt64_desc

    # 跳转到 64 位代码段
    pushq $0x08
    pushq $long_mode_start
    lretq

# 检查 CPUID 支持
check_cpuid:
    # 尝试翻转 EFLAGS 中的 ID 位
    pushfq
    pop %rax
    mov %rax, %rbx
    xor $0x200000, %rax    # 翻转 ID 位
    push %rax
    popfq
    pushfq
    pop %rax
    cmp %rax, %rbx
    je no_cpuid
    ret

no_cpuid:
    mov $'0', %al
    jmp error

# 检查长模式支持
check_long_mode:
    # 检查扩展功能
    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb no_long_mode

    # 检查长模式位
    mov $0x80000001, %eax
    cpuid
    test $0x20000000, %edx
    jz no_long_mode
    ret

no_long_mode:
    mov $'1', %al
    jmp error

# 设置页表
setup_page_tables:
    # 清空页表
    mov $boot_pml4, %rdi
    mov $0, %rax
    mov $1024, %rcx
    rep stosq

    # 设置 PML4
    mov $boot_pml4, %rdi
    mov $boot_pdpt, %rax
    or $0x003, %rax    # 存在 + 可写
    mov %rax, (%rdi)

    # 设置 PDPT
    mov $boot_pdpt, %rdi
    mov $boot_pdt, %rax
    or $0x003, %rax    # 存在 + 可写
    mov %rax, (%rdi)

    # 设置 PDT (映射前 2MB)
    mov $boot_pdt, %rdi
    mov $0x000000, %rax
    or $0x083, %rax    # 存在 + 可写 + 2MB页
    mov %rax, (%rdi)

    # 映射内核虚拟地址 (高地址)
    mov $boot_pml4, %rdi
    add $256 * 8, %rdi    # 第256个条目 (0xFFFF800000000000)
    mov $boot_pdpt, %rax
    or $0x003, %rax
    mov %rax, (%rdi)

    ret

# 启用分页
enable_paging:
    # 设置 CR3 (页目录基址)
    mov $boot_pml4, %rax
    mov %rax, %cr3

    # 启用 PAE
    mov %cr4, %rax
    or $0x20, %rax
    mov %rax, %cr4

    # 设置长模式位
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    # 启用分页
    mov %cr0, %rax
    or $0x80000000, %eax
    mov %rax, %cr0

    ret

# 错误处理
error:
    # 显示错误代码
    mov $0xb8000, %rbx
    mov $0x4f00, %rdx
    or %rax, %rdx
    mov %rdx, (%rbx)

    # 停机
    hlt
    jmp error

# 64位模式代码
.code64
long_mode_start:
    # 设置段寄存器
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # 设置高地址栈
    mov $stack_top, %rsp
    movabs $0xFFFF800000000000, %rax
    add %rax, %rsp

    # 恢复 multiboot2 参数
    popq %rax
    popq %rbx

    # 调用内核主函数
    call kernel_main

    # 如果 kernel_main 返回，停机
    cli
    hlt
    jmp .-1

# 64位全局描述符表
.align 8
gdt64:
    .quad 0x0000000000000000    # 空描述符
    .quad 0x00209A0000000000    # 64位代码段
    .quad 0x0000920000000000    # 64位数据段

gdt64_desc:
    .word gdt64_desc - gdt64 - 1
    .quad gdt64

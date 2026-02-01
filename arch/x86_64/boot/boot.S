# x86_64 内核引导代码
# 这个文件包含了内核的引导入口点和初始化代码

.section .multiboot2
.align 8

# Multiboot2 头部
multiboot2_header:
    .long 0xe85250d6                    # 魔数
    .long 0                             # 架构 (0 = i386)
    .long multiboot2_header_end - multiboot2_header    # 头部长度
    .long -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header))  # 校验和

    # 结束标记
    .short 0    # 类型
    .short 0    # 标志
    .long 8     # 大小
multiboot2_header_end:

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

.section .data
.align 8
# 64位全局描述符表
gdt64:
    .quad 0x0000000000000000    # 空描述符
    .quad 0x00209A0000000000    # 64位代码段
    .quad 0x0000920000000000    # 64位数据段
gdt64_end:

.global gdt64_desc
gdt64_desc:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start

# 内核入口点 (从 32 位保护模式进入)
_start:
    # 禁用中断
    cli

    # 保存 multiboot2 信息
    movl %ebx, %edi    # multiboot2 信息结构指针
    movl %eax, %esi    # multiboot2 魔数

    # 设置临时栈指针
    movl $stack_top, %esp

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
    ljmp $0x08, $long_mode_start

# 检查 CPUID 支持
check_cpuid:
    # 尝试翻转 EFLAGS 中的 ID 位
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
    cmpl %ecx, %eax
    je no_cpuid
    ret

no_cpuid:
    movb $'0', %al
    jmp error

# 检查长模式支持
check_long_mode:
    # 检查扩展功能
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode

    # 检查长模式位
    movl $0x80000001, %eax
    cpuid
    testl $0x20000000, %edx
    jz no_long_mode
    ret

no_long_mode:
    movb $'1', %al
    jmp error

# 设置页表
setup_page_tables:
    # 清空页表
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

    # 设置 PML4[0] -> PDPT
    movl $boot_pml4, %edi
    movl $boot_pdpt, %eax
    orl $0x003, %eax    # 存在 + 可写
    movl %eax, (%edi)

    # 设置 PML4[256] -> PDPT (高地址映射 0xFFFF800000000000)
    movl $boot_pml4, %edi
    addl $(256 * 8), %edi
    movl $boot_pdpt, %eax
    orl $0x003, %eax
    movl %eax, (%edi)

    # 设置 PDPT[0] -> PDT
    movl $boot_pdpt, %edi
    movl $boot_pdt, %eax
    orl $0x003, %eax    # 存在 + 可写
    movl %eax, (%edi)

    # 设置 PDT (映射前 1GB 使用 2MB 页)
    movl $boot_pdt, %edi
    movl $0x00000083, %eax    # 存在 + 可写 + 2MB页
    movl $512, %ecx

1:
    movl %eax, (%edi)
    addl $0x200000, %eax    # 下一个 2MB
    addl $8, %edi
    decl %ecx
    jnz 1b

    ret

# 启用分页
enable_paging:
    # 设置 CR3 (页目录基址)
    movl $boot_pml4, %eax
    movl %eax, %cr3

    # 启用 PAE
    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4

    # 设置长模式位 (EFER.LME)
    movl $0xC0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr

    # 启用分页 (CR0.PG)
    movl %cr0, %eax
    orl $0x80000001, %eax    # PG + PE
    movl %eax, %cr0

    ret

# 错误处理
error:
    # 显示错误代码到 VGA
    movl $0xb8000, %ebx
    movw $0x4f00, %dx
    orb %al, %dl
    movw %dx, (%ebx)

    # 停机
1:  cli
    hlt
    jmp 1b

# 64位模式代码
.code64
long_mode_start:
    # 设置段寄存器
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # 设置 64 位栈
    movabsq $stack_top, %rsp

    # 清除 BSS 段 (可选，如果有)
    # 准备参数给 kernel_main
    # rdi = multiboot2 魔数, rsi = multiboot2 信息指针
    movl %esi, %edi    # multiboot2 魔数
    movq $0, %rsi      # 暂时不传递 multiboot 信息

    # 调用内核主函数
    call kernel_main

    # 如果 kernel_main 返回，停机
    cli
1:  hlt
    jmp 1b

# 辅助函数：串口输出字符 (调试用)
.global serial_putc_asm
serial_putc_asm:
    # 参数在 %rdi 中
    movw $0x3F8, %dx
    addw $5, %dx
1:  inb %dx, %al
    testb $0x20, %al
    jz 1b
    movw $0x3F8, %dx
    movb %dil, %al
    outb %al, %dx
    ret

# 停机函数
.global halt_cpu
halt_cpu:
    cli
    hlt
    jmp halt_cpu

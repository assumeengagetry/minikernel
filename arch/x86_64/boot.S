/* Multiboot2 x86_64 bootstrap. */

.section .multiboot2,"a"
.align 8
multiboot2_header:
    .long 0xE85250D6
    .long 0
    .long multiboot2_header_end - multiboot2_header
    .long -(0xE85250D6 + (multiboot2_header_end - multiboot2_header))
    .word 0
    .word 0
    .long 8
multiboot2_header_end:

.section .bss
.align 16
stack_bottom:
    .space 16384
stack_top:

.align 4096
boot_pml4:
    .space 4096
boot_pdpt:
    .space 4096
boot_pdt:
    .space 4096
boot_pt:
    .space 4096

.align 4
multiboot_magic:
    .long 0
multiboot_info:
    .long 0

.section .data
.align 8
.global gdt64
gdt64:
    .quad 0x0000000000000000
    .quad 0x00209A0000000000
    .quad 0x0000920000000000
gdt64_end:

gdt64_desc:
    .word 23
    .long gdt64

.section .text
.code32
.global _start
.type _start, @function

_start:
    cli
    cld
    movl $stack_top, %esp
    xorl %ebp, %ebp

    cmpl $0x36D76289, %eax
    jne invalid_multiboot
    movl %eax, multiboot_magic
    movl %ebx, multiboot_info

    call check_cpuid
    call check_long_mode
    call setup_page_tables
    lgdt gdt64_desc
    jmp enable_paging

check_cpuid:
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $0x200000, %eax
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
    movb $'C', %al
    jmp boot_error

check_long_mode:
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode

    movl $0x80000001, %eax
    cpuid
    testl $0x20000000, %edx
    jz no_long_mode
    ret

no_long_mode:
    movb $'L', %al
    jmp boot_error

setup_page_tables:
    movl $boot_pml4, %edi
    xorl %eax, %eax
    movl $4096, %ecx
    rep stosl

    movl $boot_pml4, %edi
    movl $boot_pdpt, %eax
    orl $0x003, %eax
    movl %eax, (%edi)

    movl $boot_pdpt, %edi
    movl $boot_pdt, %eax
    orl $0x003, %eax
    movl %eax, (%edi)

    movl $boot_pdt, %edi
    movl $boot_pt, %eax
    orl $0x003, %eax
    movl %eax, (%edi)

    movl $boot_pt, %edi
    xorl %ebx, %ebx
    movl $512, %ecx
.Lpte:
    movl %ebx, %eax
    orl $0x003, %eax
    movl %eax, (%edi)
    addl $0x1000, %ebx
    addl $8, %edi
    loop .Lpte
    ret

enable_paging:
    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4

    movl $boot_pml4, %eax
    movl %eax, %cr3

    movl $0xC0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr

    movl %cr0, %eax
    orl $0x80000001, %eax
    movl %eax, %cr0
    ljmp $0x08, $long_mode_start

invalid_multiboot:
    movb $'M', %al
    jmp boot_error

boot_error:
    movl $0xB8000, %edi
    movb $'!', (%edi)
    movb $0x4F, 1(%edi)
    movb %al, 2(%edi)
    movb $0x4F, 3(%edi)
    cli
.Lerror_halt:
    hlt
    jmp .Lerror_halt

.code64
long_mode_start:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    movabs $stack_top, %rsp
    movl multiboot_magic(%rip), %edi
    movl multiboot_info(%rip), %esi
    call kernel_main

    cli
.Lreturned:
    hlt
    jmp .Lreturned

.size _start, . - _start
.section .note.GNU-stack,"",@progbits

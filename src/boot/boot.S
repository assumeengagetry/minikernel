.section .multiboot2
.align 8
header_start:
    .long 0xe85250d6
    .long 0
    .long header_end - header_start
    .long -(0xe85250d6 + 0 + (header_end - header_start))
    .word 0
    .word 0
    .long 8
header_end:

.section .text
.code32
.global _start
.extern kernel_main

_start:
    cli
    movl $stack_top, %esp

    movl %eax, mb_magic
    movl %ebx, mb_info

    call setup_paging
    call setup_long_mode

    hlt

setup_paging:
    movl $pdpt_table + 0x03, pml4_table
    movl $0, pml4_table + 4

    movl $pd_table + 0x03, pdpt_table
    movl $0, pdpt_table + 4

    movl $0x00000083, pd_table
    movl $0, pd_table + 4

    ret

setup_long_mode:
    lgdt gdt64_ptr

    movl %cr4, %eax
    orl $0x00000020, %eax      # CR4.PAE
    movl %eax, %cr4

    movl $0xC0000080, %ecx     # IA32_EFER
    rdmsr
    orl $0x00000100, %eax      # EFER.LME
    wrmsr

    movl $pml4_table, %eax
    movl %eax, %cr3

    movl %cr0, %eax
    orl $0x80000001, %eax      # CR0.PG | CR0.PE
    movl %eax, %cr0

    ljmp $0x08, $long_mode_start

.code64
long_mode_start:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    lea stack_top(%rip), %rsp

    movl mb_magic(%rip), %edi
    movl mb_info(%rip), %esi
    call kernel_main

.hang:
    hlt
    jmp .hang

.section .rodata
.align 8
gdt64:
    .quad 0x0000000000000000
    .quad 0x00af9a000000ffff
    .quad 0x00af92000000ffff
gdt64_end:
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .bss
.align 16
mb_magic:
    .long 0

mb_info:
    .long 0

.align 4096
pml4_table:
    .skip 4096

.align 4096
pdpt_table:
    .skip 4096

.align 4096
pd_table:
    .skip 4096

.align 16
stack_bottom:
    .skip 16384
stack_top:

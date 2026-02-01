/* x86_64 内核链接脚本 */
/* 确保 Multiboot 头部在内核最前面且被正确加载 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* 内核物理加载地址 - 1MB (传统内核加载地址) */
KERNEL_PHYS_BASE = 0x100000;

SECTIONS
{
    /* 从 1MB 开始 */
    . = KERNEL_PHYS_BASE;

    /*
     * Multiboot 头部段
     * 必须在最前面，在前 8KB 内 (Multiboot1) / 32KB 内 (Multiboot2)
     * 使用 ALIGN(4096) 确保整个头部和代码在同一个 LOAD 段中
     */
    .boot ALIGN(4) :
    {
        /* Multiboot1 头部 (12 bytes) - 用于 QEMU -kernel */
        KEEP(*(.multiboot))

        /* Multiboot2 头部 - 用于 GRUB2 */
        . = ALIGN(8);
        KEEP(*(.multiboot2))
    }

    /* 代码段 - 紧跟在 multiboot 头部之后 */
    .text ALIGN(16) :
    {
        __text_start = .;
        *(.text.boot)       /* 启动代码优先 */
        *(.text._start)     /* 入口点 */
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

    /* 未初始化数据段 */
    .bss ALIGN(4K) (NOLOAD) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    }

    /* 内核结束标记 */
    . = ALIGN(4K);
    __kernel_start = KERNEL_PHYS_BASE;
    __kernel_end = .;
    __kernel_size = __kernel_end - __kernel_start;

    /* 丢弃不需要的段 */
    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.gnu.hash)
    }
}

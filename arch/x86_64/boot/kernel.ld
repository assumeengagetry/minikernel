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
     * 必须在最前面，并位于 Multiboot1 要求的前 8KB 内
     * 使用 ALIGN(4096) 确保整个头部和代码在同一个 LOAD 段中
     */
    .boot ALIGN(4) :
    {
        /* Multiboot1 头部 (12 bytes) */
        KEEP(*(.multiboot))
    }

    /* 代码段 - 紧跟在 multiboot 头部之后 */
    .text ALIGN(16) :
    {
        *(.text.boot)       /* 启动代码优先 */
        *(.text._start)     /* 入口点 */
        *(.text)
        *(.text.*)
    }

    /* 只读数据段 */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* 已初始化数据段 */
    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    /* 未初始化数据段 */
    .bss ALIGN(4K) (NOLOAD) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

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

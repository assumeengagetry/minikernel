OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .multiboot2 ALIGN(8) :
    {
        KEEP(*(.multiboot2))
    }

    .text ALIGN(4096) :
    {
        *(.text.boot)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4096) (NOLOAD) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    . = ALIGN(4096);
    __kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}

ASSERT(SIZEOF(.multiboot2) > 0, "missing Multiboot2 header")
ASSERT(__kernel_end < 0x100000000, "kernel must fit below 4 GiB")

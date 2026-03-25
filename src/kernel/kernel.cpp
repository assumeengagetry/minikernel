#include <stdint.h>
#include "vga.hpp"

extern "C" __attribute__((noreturn)) void kernel_main(uint32_t magic, uint32_t mbi) {
    // vga::clear();
    vga::write_string("minikernel MVP booted", 0, 0, 0x0F);

    if (magic == 0x36d76289u) {
        vga::write_string("Multiboot2 OK", 1, 0, 0x0A);
    } else {
        vga::write_string("Multiboot2 BAD", 1, 0, 0x4C);
    }

    vga::write_string("QEMU + GRUB + ELF + long mode", 2, 0, 0x07);

    (void)mbi;

    for (;;) {
        __asm__ volatile("hlt");
    }
}

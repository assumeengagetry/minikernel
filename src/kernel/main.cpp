/* MicroKernel boot entry and serial console. */

#include "../../kernel/include/shell.h"

extern "C" {

#define SERIAL_PORT 0x3F8

static inline unsigned char inb(unsigned short port)
{
    unsigned char value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void serial_init(void)
{
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x80);
    outb(SERIAL_PORT + 0, 0x01);
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x03);
    outb(SERIAL_PORT + 2, 0xC7);
    outb(SERIAL_PORT + 4, 0x03);
}

void serial_putc(char c)
{
    for (unsigned int i = 0; i < 1000000; i++) {
        if (inb(SERIAL_PORT + 5) & 0x20) {
            outb(SERIAL_PORT, (unsigned char)c);
            return;
        }
    }
}

static void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n')
            serial_putc('\r');
        serial_putc(*text++);
    }
}

static inline void halt(void)
{
    __asm__ __volatile__("hlt");
}

void kernel_main(void)
{
    serial_init();
    serial_write("Initializing MicroKernel 0.1.0\n");
    serial_write("Kernel initialization complete.\n");
    serial_write("Starting shell...\n");
    shell_run();

    for (;;)
        halt();
}

}

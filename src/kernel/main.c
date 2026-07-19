/* Minimal serial console and kernel entry point. */

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

static void serial_putc(char value)
{
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0)
        __asm__ __volatile__("pause");

    outb(SERIAL_PORT, value);
}

static void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n')
            serial_putc('\r');
        serial_putc(*text++);
    }
}

__attribute__((noreturn)) void kernel_main(unsigned int magic,
                                            unsigned int info)
{
    (void)info;

    serial_init();
    serial_write("MicroKernel 0.1.0\n");
    if (magic != 0x36D76289)
        serial_write("Invalid Multiboot2 magic\n");
    serial_write("Kernel initialization complete.\n");

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

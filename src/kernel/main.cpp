/* MicroKernel boot entry and serial console. */

#include "../../kernel/include/mm.h"
#include "../../kernel/include/shell.h"
#include "../../kernel/include/types.h"

extern "C" {

#define KERNEL_NAME "MicroKernel"
#define KERNEL_VERSION "0.1.0"
#define SERIAL_PORT 0x3F8

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)

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

void console_write(const char *buffer, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (buffer[i] == '\n')
            serial_putc('\r');
        serial_putc(buffer[i]);
    }
}

static int format_unsigned(char *buffer, unsigned long value, unsigned int base,
                           bool uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char reversed[32];
    int length = 0;

    do {
        reversed[length++] = digits[value % base];
        value /= base;
    } while (value != 0);

    for (int i = 0; i < length; i++)
        buffer[i] = reversed[length - i - 1];

    return length;
}

static int format_signed(char *buffer, long value)
{
    int offset = 0;
    unsigned long magnitude;

    if (value < 0) {
        buffer[offset++] = '-';
        magnitude = (unsigned long)(-(value + 1)) + 1;
    } else {
        magnitude = (unsigned long)value;
    }

    return offset + format_unsigned(buffer + offset, magnitude, 10, false);
}

static void append_char(char **output, char *end, char value)
{
    if (*output < end)
        *(*output)++ = value;
}

static void append_string(char **output, char *end, const char *value)
{
    while (*value != '\0' && *output < end)
        *(*output)++ = *value++;
}

static int kernel_vsnprintf(char *buffer, size_t size, const char *format,
                            va_list args)
{
    char *output = buffer;
    char *end;

    if (size == 0)
        return 0;

    end = buffer + size - 1;

    while (*format != '\0' && output < end) {
        if (*format != '%') {
            append_char(&output, end, *format++);
            continue;
        }

        format++;
        bool is_long = false;
        if (*format == 'l') {
            is_long = true;
            format++;
        }

        char number[32];
        int length = 0;

        switch (*format) {
        case 's': {
            const char *value = va_arg(args, const char *);
            append_string(&output, end, value ? value : "(null)");
            break;
        }
        case 'd':
        case 'i':
            length = format_signed(number,
                                   is_long ? va_arg(args, long)
                                           : (long)va_arg(args, int));
            for (int i = 0; i < length; i++)
                append_char(&output, end, number[i]);
            break;
        case 'u':
            length = format_unsigned(number,
                                     is_long ? va_arg(args, unsigned long)
                                             : (unsigned long)va_arg(args, unsigned int),
                                     10, false);
            for (int i = 0; i < length; i++)
                append_char(&output, end, number[i]);
            break;
        case 'x':
        case 'X':
            length = format_unsigned(number,
                                     is_long ? va_arg(args, unsigned long)
                                             : (unsigned long)va_arg(args, unsigned int),
                                     16, *format == 'X');
            for (int i = 0; i < length; i++)
                append_char(&output, end, number[i]);
            break;
        case 'p':
            append_string(&output, end, "0x");
            length = format_unsigned(number,
                                     (unsigned long)va_arg(args, void *), 16,
                                     false);
            for (int i = 0; i < length; i++)
                append_char(&output, end, number[i]);
            break;
        case 'c':
            append_char(&output, end, (char)va_arg(args, int));
            break;
        case '%':
            append_char(&output, end, '%');
            break;
        default:
            append_char(&output, end, '%');
            if (is_long)
                append_char(&output, end, 'l');
            if (*format != '\0')
                append_char(&output, end, *format);
            break;
        }

        if (*format != '\0')
            format++;
    }

    *output = '\0';
    return (int)(output - buffer);
}

int printk(const char *format, ...)
{
    char buffer[1024];
    va_list args;

    va_start(args, format);
    int length = kernel_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    console_write(buffer, (size_t)length);
    return length;
}

static inline void halt(void)
{
    __asm__ __volatile__("hlt");
}

void panic(const char *format, ...)
{
    char buffer[1024];
    va_list args;

    va_start(args, format);
    kernel_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    printk("KERNEL PANIC: %s\n", buffer);
    local_irq_disable();

    for (;;)
        halt();
}

unsigned long local_irq_save(void)
{
    unsigned long flags;
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

void local_irq_restore(unsigned long flags)
{
    __asm__ __volatile__("pushq %0; popfq" :: "r"(flags) : "memory");
}

void local_irq_disable(void)
{
    __asm__ __volatile__("cli" ::: "memory");
}

void local_irq_enable(void)
{
    __asm__ __volatile__("sti" ::: "memory");
}

void local_bh_disable(void) { }
void local_bh_enable(void) { }

void kernel_main(void)
{
    serial_init();
    printk("Initializing %s %s\n", KERNEL_NAME, KERNEL_VERSION);
    mm_init();
    printk("Kernel initialization complete.\n");
    printk("Starting shell...\n");
    shell_run();
    panic("Shell exited unexpectedly");
}

}

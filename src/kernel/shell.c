/*
 * MicroKernel Shell
 * 
 * A simple kernel-level shell for debugging and interaction
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/mm.h"

/* ===========================================================================
 * Constants
 * ===========================================================================*/

#define SHELL_BUFFER_SIZE   256
#define SHELL_HISTORY_SIZE  10
#define SHELL_MAX_ARGS      16
#define SHELL_PROMPT        "microkernel> "

/* Serial port */
#define SERIAL_PORT         0x3F8
#define SERIAL_DATA         (SERIAL_PORT + 0)
#define SERIAL_IER          (SERIAL_PORT + 1)
#define SERIAL_FIFO         (SERIAL_PORT + 2)
#define SERIAL_LCR          (SERIAL_PORT + 3)
#define SERIAL_MCR          (SERIAL_PORT + 4)
#define SERIAL_LSR          (SERIAL_PORT + 5)

/* Keyboard port */
#define KBD_DATA_PORT       0x60
#define KBD_STATUS_PORT     0x64

/* VGA text mode */
#define VGA_BUFFER          0xB8000
#define VGA_WIDTH           80
#define VGA_HEIGHT          25
#define VGA_COLOR_WHITE     0x0F
#define VGA_COLOR_GREEN     0x0A
#define VGA_COLOR_CYAN      0x0B

/* Special keys */
#define KEY_BACKSPACE       0x08
#define KEY_TAB             0x09
#define KEY_ENTER           0x0D
#define KEY_ESCAPE          0x1B
#define KEY_UP              0x80
#define KEY_DOWN            0x81
#define KEY_LEFT            0x82
#define KEY_RIGHT           0x83

/* ===========================================================================
 * External declarations
 * ===========================================================================*/

extern int printk(const char *fmt, ...);
extern void console_write(const char *buffer, size_t len);
extern void serial_putc(char c);
extern unsigned long nr_free_pages(void);

/* ===========================================================================
 * Port I/O
 * ===========================================================================*/

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

static inline void io_wait(void)
{
    outb(0x80, 0);
}

/* ===========================================================================
 * String utilities
 * ===========================================================================*/

static size_t shell_strlen(const char *s)
{
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static int shell_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *shell_strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static char *shell_strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

static int shell_isspace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int shell_isdigit(char c)
{
    return c >= '0' && c <= '9';
}

static int shell_atoi(const char *s)
{
    int result = 0;
    int sign = 1;
    
    while (shell_isspace(*s)) s++;
    
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    while (shell_isdigit(*s)) {
        result = result * 10 + (*s - '0');
        s++;
    }
    
    return sign * result;
}

/* ===========================================================================
 * Shell state
 * ===========================================================================*/

static char shell_buffer[SHELL_BUFFER_SIZE];
static int shell_buffer_pos = 0;
static char shell_history[SHELL_HISTORY_SIZE][SHELL_BUFFER_SIZE];
static int shell_history_count = 0;
static int shell_history_index = 0;
static volatile bool shell_running = true;
static u64 shell_start_time = 0;
static u64 jiffies = 0;  /* Simple tick counter */

/* ===========================================================================
 * Output functions
 * ===========================================================================*/

static void shell_putchar(char c)
{
    serial_putc(c);
}

static void shell_puts(const char *s)
{
    while (*s) {
        shell_putchar(*s++);
    }
}

static void shell_print_int(long value)
{
    char buf[32];
    int i = 0;
    int negative = 0;
    unsigned long uval;
    
    if (value < 0) {
        negative = 1;
        uval = -value;
    } else {
        uval = value;
    }
    
    if (uval == 0) {
        buf[i++] = '0';
    } else {
        while (uval > 0) {
            buf[i++] = '0' + (uval % 10);
            uval /= 10;
        }
    }
    
    if (negative) {
        shell_putchar('-');
    }
    
    while (i > 0) {
        shell_putchar(buf[--i]);
    }
}

static void shell_print_hex(unsigned long value)
{
    static const char hex[] = "0123456789abcdef";
    char buf[17];
    int i = 0;
    
    if (value == 0) {
        shell_puts("0x0");
        return;
    }
    
    while (value > 0) {
        buf[i++] = hex[value & 0xF];
        value >>= 4;
    }
    
    shell_puts("0x");
    while (i > 0) {
        shell_putchar(buf[--i]);
    }
}

static void shell_newline(void)
{
    shell_puts("\r\n");
}

static void shell_print_prompt(void)
{
    shell_puts("\033[32m");  /* Green color */
    shell_puts(SHELL_PROMPT);
    shell_puts("\033[0m");   /* Reset color */
}

/* ===========================================================================
 * Input functions
 * ===========================================================================*/

static void serial_init(void)
{
    /* Disable interrupts */
    outb(SERIAL_IER, 0x00);
    
    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_LCR, 0x80);
    
    /* Set divisor to 1 (115200 baud) */
    outb(SERIAL_DATA, 0x01);
    outb(SERIAL_IER, 0x00);
    
    /* 8 bits, no parity, one stop bit */
    outb(SERIAL_LCR, 0x03);
    
    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(SERIAL_FIFO, 0xC7);
    
    /* Enable IRQs, RTS/DSR set */
    outb(SERIAL_MCR, 0x0B);
    
    /* Enable receive interrupt */
    outb(SERIAL_IER, 0x01);
}

static int serial_received(void)
{
    return inb(SERIAL_LSR) & 0x01;
}

static int serial_try_getchar(void)
{
    if (serial_received()) {
        return inb(SERIAL_DATA);
    }
    return -1;
}

/* ===========================================================================
 * History management
 * ===========================================================================*/

static void shell_history_add(const char *cmd)
{
    if (shell_strlen(cmd) == 0) return;
    
    /* Don't add if same as last command */
    if (shell_history_count > 0) {
        int last = (shell_history_count - 1) % SHELL_HISTORY_SIZE;
        if (shell_strcmp(shell_history[last], cmd) == 0) {
            shell_history_index = shell_history_count;
            return;
        }
    }
    
    int index = shell_history_count % SHELL_HISTORY_SIZE;
    shell_strncpy(shell_history[index], cmd, SHELL_BUFFER_SIZE - 1);
    shell_history[index][SHELL_BUFFER_SIZE - 1] = '\0';
    shell_history_count++;
    shell_history_index = shell_history_count;
}

static const char *shell_history_get(int offset)
{
    int index = shell_history_index + offset;
    if (index < 0 || index >= shell_history_count) {
        return NULL;
    }
    
    int start = 0;
    if (shell_history_count > SHELL_HISTORY_SIZE) {
        start = shell_history_count - SHELL_HISTORY_SIZE;
    }
    
    if (index < start) {
        return NULL;
    }
    
    shell_history_index = index;
    return shell_history[index % SHELL_HISTORY_SIZE];
}

/* ===========================================================================
 * Command parsing
 * ===========================================================================*/

static int shell_parse_args(char *line, char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;
    
    while (*p && argc < max_args) {
        /* Skip whitespace */
        while (*p && shell_isspace(*p)) {
            *p++ = '\0';
        }
        
        if (*p == '\0') break;
        
        /* Handle quoted strings */
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            argv[argc++] = p;
            while (*p && *p != quote) p++;
            if (*p) *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && !shell_isspace(*p)) p++;
        }
    }
    
    return argc;
}

/* ===========================================================================
 * Built-in commands
 * ===========================================================================*/

static void cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_puts("\r\n");
    shell_puts("╔══════════════════════════════════════════════════════════════╗\r\n");
    shell_puts("║              MicroKernel Shell - Available Commands          ║\r\n");
    shell_puts("╠══════════════════════════════════════════════════════════════╣\r\n");
    shell_puts("║  help              - Show this help message                  ║\r\n");
    shell_puts("║  version           - Display kernel version                  ║\r\n");
    shell_puts("║  clear             - Clear the screen                        ║\r\n");
    shell_puts("║  echo <text>       - Print text to console                   ║\r\n");
    shell_puts("║  mem               - Show memory statistics                  ║\r\n");
    shell_puts("║  uptime            - Show system uptime                      ║\r\n");
    shell_puts("║  cpuinfo           - Display CPU information                 ║\r\n");
    shell_puts("║  history           - Show command history                    ║\r\n");
    shell_puts("║  date              - Show current date/time (placeholder)    ║\r\n");
    shell_puts("║  hexdump <addr> <n>- Dump n bytes at address                 ║\r\n");
    shell_puts("║  poke <addr> <val> - Write byte to address                   ║\r\n");
    shell_puts("║  reboot            - Reboot the system                       ║\r\n");
    shell_puts("║  shutdown          - Shutdown the system                     ║\r\n");
    shell_puts("║  panic             - Trigger kernel panic (testing)          ║\r\n");
    shell_puts("╚══════════════════════════════════════════════════════════════╝\r\n");
}

static void cmd_version(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_puts("\r\n");
    shell_puts("MicroKernel v0.1.0\r\n");
    shell_puts("  Architecture: x86_64\r\n");
    shell_puts("  Build type:   Debug\r\n");
    shell_puts("  License:      MIT\r\n");
}

static void cmd_clear(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    /* ANSI escape sequence to clear screen */
    shell_puts("\033[2J\033[H");
}

static void cmd_echo(int argc, char *argv[])
{
    shell_puts("\r\n");
    for (int i = 1; i < argc; i++) {
        shell_puts(argv[i]);
        if (i < argc - 1) {
            shell_putchar(' ');
        }
    }
    shell_newline();
}

static void cmd_mem(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    unsigned long free_pages = 0;
    
    /* Try to get actual free pages if the function exists */
    /* For now, use placeholder values */
    free_pages = 8192;  /* Placeholder: 32MB */
    
    shell_puts("\r\n");
    shell_puts("╔═══════════════════════════════════════╗\r\n");
    shell_puts("║         Memory Statistics             ║\r\n");
    shell_puts("╠═══════════════════════════════════════╣\r\n");
    shell_puts("║  Free pages:    ");
    shell_print_int(free_pages);
    shell_puts("\r\n");
    shell_puts("║  Free memory:   ");
    shell_print_int((free_pages * 4096) / 1024 / 1024);
    shell_puts(" MB\r\n");
    shell_puts("║  Page size:     4096 bytes\r\n");
    shell_puts("╚═══════════════════════════════════════╝\r\n");
}

static void cmd_uptime(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    /* Placeholder uptime calculation */
    unsigned long seconds = jiffies / 100;  /* Assuming 100 Hz tick */
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    shell_puts("\r\n");
    shell_puts("System uptime: ");
    
    if (days > 0) {
        shell_print_int(days);
        shell_puts(" day(s), ");
    }
    
    shell_print_int(hours % 24);
    shell_puts(":");
    if ((minutes % 60) < 10) shell_putchar('0');
    shell_print_int(minutes % 60);
    shell_puts(":");
    if ((seconds % 60) < 10) shell_putchar('0');
    shell_print_int(seconds % 60);
    shell_newline();
}

static void cmd_cpuinfo(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    char vendor[13] = {0};
    unsigned int eax, ebx, ecx, edx;
    
    /* Get vendor string */
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );
    
    *(unsigned int *)&vendor[0] = ebx;
    *(unsigned int *)&vendor[4] = edx;
    *(unsigned int *)&vendor[8] = ecx;
    vendor[12] = '\0';
    
    /* Get processor info */
    unsigned int family, model, stepping;
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    stepping = eax & 0xF;
    model = (eax >> 4) & 0xF;
    family = (eax >> 8) & 0xF;
    
    if (family == 0xF) {
        family += (eax >> 20) & 0xFF;
    }
    if (family == 0x6 || family == 0xF) {
        model += ((eax >> 16) & 0xF) << 4;
    }
    
    shell_puts("\r\n");
    shell_puts("╔═══════════════════════════════════════╗\r\n");
    shell_puts("║           CPU Information             ║\r\n");
    shell_puts("╠═══════════════════════════════════════╣\r\n");
    shell_puts("║  Vendor:   ");
    shell_puts(vendor);
    shell_puts("\r\n");
    shell_puts("║  Family:   ");
    shell_print_int(family);
    shell_puts("\r\n");
    shell_puts("║  Model:    ");
    shell_print_int(model);
    shell_puts("\r\n");
    shell_puts("║  Stepping: ");
    shell_print_int(stepping);
    shell_puts("\r\n");
    shell_puts("║  Features: ");
    if (edx & (1 << 25)) shell_puts("SSE ");
    if (edx & (1 << 26)) shell_puts("SSE2 ");
    if (ecx & (1 << 0))  shell_puts("SSE3 ");
    if (ecx & (1 << 28)) shell_puts("AVX ");
    shell_puts("\r\n");
    shell_puts("╚═══════════════════════════════════════╝\r\n");
}

static void cmd_history(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_puts("\r\n");
    shell_puts("Command history:\r\n");
    
    int start = 0;
    if (shell_history_count > SHELL_HISTORY_SIZE) {
        start = shell_history_count - SHELL_HISTORY_SIZE;
    }
    
    for (int i = start; i < shell_history_count; i++) {
        shell_puts("  ");
        shell_print_int(i + 1);
        shell_puts("  ");
        shell_puts(shell_history[i % SHELL_HISTORY_SIZE]);
        shell_newline();
    }
    
    if (shell_history_count == 0) {
        shell_puts("  (empty)\r\n");
    }
}

static void cmd_date(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    /* Read RTC - placeholder for now */
    shell_puts("\r\n");
    shell_puts("Date/Time: (RTC not implemented)\r\n");
    shell_puts("System ticks: ");
    shell_print_int(jiffies);
    shell_newline();
}

static unsigned long parse_hex(const char *s)
{
    unsigned long result = 0;
    
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    
    while (*s) {
        char c = *s++;
        int digit;
        
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            break;
        }
        
        result = (result << 4) | digit;
    }
    
    return result;
}

static void cmd_hexdump(int argc, char *argv[])
{
    if (argc < 3) {
        shell_puts("\r\nUsage: hexdump <address> <count>\r\n");
        shell_puts("  Example: hexdump 0x100000 64\r\n");
        return;
    }
    
    unsigned long addr = parse_hex(argv[1]);
    int count = shell_atoi(argv[2]);
    
    if (count <= 0 || count > 256) {
        shell_puts("\r\nError: count must be 1-256\r\n");
        return;
    }
    
    shell_puts("\r\n");
    
    for (int i = 0; i < count; i += 16) {
        shell_print_hex(addr + i);
        shell_puts(": ");
        
        /* Hex bytes */
        for (int j = 0; j < 16 && (i + j) < count; j++) {
            unsigned char byte = *(unsigned char *)(addr + i + j);
            shell_putchar("0123456789abcdef"[byte >> 4]);
            shell_putchar("0123456789abcdef"[byte & 0xF]);
            shell_putchar(' ');
            if (j == 7) shell_putchar(' ');
        }
        
        /* Padding if needed */
        for (int j = count - i; j < 16; j++) {
            shell_puts("   ");
            if (j == 7) shell_putchar(' ');
        }
        
        shell_puts(" |");
        
        /* ASCII representation */
        for (int j = 0; j < 16 && (i + j) < count; j++) {
            unsigned char byte = *(unsigned char *)(addr + i + j);
            if (byte >= 0x20 && byte < 0x7F) {
                shell_putchar(byte);
            } else {
                shell_putchar('.');
            }
        }
        
        shell_puts("|\r\n");
    }
}

static void cmd_poke(int argc, char *argv[])
{
    if (argc < 3) {
        shell_puts("\r\nUsage: poke <address> <value>\r\n");
        shell_puts("  Example: poke 0xB8000 0x41\r\n");
        return;
    }
    
    unsigned long addr = parse_hex(argv[1]);
    unsigned char value = (unsigned char)parse_hex(argv[2]);
    
    *(unsigned char *)addr = value;
    
    shell_puts("\r\nWrote ");
    shell_print_hex(value);
    shell_puts(" to ");
    shell_print_hex(addr);
    shell_newline();
}

static void cmd_reboot(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_puts("\r\nRebooting...\r\n");
    
    /* Try keyboard controller reset */
    outb(0x64, 0xFE);
    
    /* If that didn't work, try triple fault */
    __asm__ __volatile__("lidt 0");
    __asm__ __volatile__("int $0");
    
    /* If we get here, just halt */
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void cmd_shutdown(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_puts("\r\nShutting down...\r\n");
    
    /* QEMU shutdown via port 0x604 */
    outb(0x604, 0x2000 & 0xFF);
    
    /* Bochs/older QEMU shutdown */
    outb(0xB004, 0x2000 & 0xFF);
    
    /* ACPI shutdown - S5 state */
    outb(0x604, 0x00);
    
    shell_puts("Shutdown failed. Please power off manually.\r\n");
    
    /* Halt */
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

static void cmd_panic(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    shell_puts("\r\nTriggering kernel panic...\r\n");
    
    /* This will hopefully trigger the panic handler */
    printk("KERNEL PANIC: User-triggered panic from shell\n");
    
    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

/* ===========================================================================
 * Command table
 * ===========================================================================*/

typedef void (*shell_cmd_func)(int argc, char *argv[]);

struct shell_command {
    const char *name;
    shell_cmd_func func;
    const char *description;
};

static struct shell_command shell_commands[] = {
    { "help",     cmd_help,     "Show help message" },
    { "?",        cmd_help,     "Show help message" },
    { "version",  cmd_version,  "Display kernel version" },
    { "ver",      cmd_version,  "Display kernel version" },
    { "clear",    cmd_clear,    "Clear screen" },
    { "cls",      cmd_clear,    "Clear screen" },
    { "echo",     cmd_echo,     "Print text" },
    { "mem",      cmd_mem,      "Show memory statistics" },
    { "memory",   cmd_mem,      "Show memory statistics" },
    { "uptime",   cmd_uptime,   "Show system uptime" },
    { "cpuinfo",  cmd_cpuinfo,  "Display CPU information" },
    { "cpu",      cmd_cpuinfo,  "Display CPU information" },
    { "history",  cmd_history,  "Show command history" },
    { "date",     cmd_date,     "Show date/time" },
    { "time",     cmd_date,     "Show date/time" },
    { "hexdump",  cmd_hexdump,  "Dump memory" },
    { "x",        cmd_hexdump,  "Dump memory" },
    { "poke",     cmd_poke,     "Write to memory" },
    { "reboot",   cmd_reboot,   "Reboot system" },
    { "shutdown", cmd_shutdown, "Shutdown system" },
    { "halt",     cmd_shutdown, "Shutdown system" },
    { "panic",    cmd_panic,    "Trigger kernel panic" },
    { NULL,       NULL,         NULL }
};

/* ===========================================================================
 * Command execution
 * ===========================================================================*/

static void shell_execute(char *line)
{
    char *argv[SHELL_MAX_ARGS];
    int argc;
    
    /* Skip leading whitespace */
    while (*line && shell_isspace(*line)) line++;
    
    /* Empty command */
    if (*line == '\0') return;
    
    /* Add to history */
    shell_history_add(line);
    
    /* Parse arguments */
    argc = shell_parse_args(line, argv, SHELL_MAX_ARGS);
    if (argc == 0) return;
    
    /* Find and execute command */
    for (struct shell_command *cmd = shell_commands; cmd->name != NULL; cmd++) {
        if (shell_strcmp(argv[0], cmd->name) == 0) {
            cmd->func(argc, argv);
            return;
        }
    }
    
    /* Unknown command */
    shell_puts("\r\nUnknown command: ");
    shell_puts(argv[0]);
    shell_puts("\r\nType 'help' for available commands.\r\n");
}

/* ===========================================================================
 * Line editing
 * ===========================================================================*/

static void shell_clear_line(void)
{
    /* Move cursor to start of line and clear */
    shell_puts("\r\033[K");
    shell_print_prompt();
}

static void shell_refresh_line(void)
{
    shell_clear_line();
    shell_buffer[shell_buffer_pos] = '\0';
    shell_puts(shell_buffer);
}

static void shell_handle_char(char c)
{
    /* Handle escape sequences */
    static int escape_state = 0;
    static char escape_buf[8] __attribute__((unused));
    static int escape_pos = 0;
    
    if (escape_state > 0) {
        escape_buf[escape_pos++] = c;
        
        if (escape_state == 1 && c == '[') {
            escape_state = 2;
            return;
        }
        
        if (escape_state == 2) {
            escape_state = 0;
            escape_pos = 0;
            
            switch (c) {
                case 'A':  /* Up arrow */
                    {
                        const char *hist = shell_history_get(-1);
                        if (hist) {
                            shell_strcpy(shell_buffer, hist);
                            shell_buffer_pos = shell_strlen(shell_buffer);
                            shell_refresh_line();
                        }
                    }
                    break;
                    
                case 'B':  /* Down arrow */
                    {
                        const char *hist = shell_history_get(1);
                        if (hist) {
                            shell_strcpy(shell_buffer, hist);
                            shell_buffer_pos = shell_strlen(shell_buffer);
                            shell_refresh_line();
                        } else {
                            shell_buffer[0] = '\0';
                            shell_buffer_pos = 0;
                            shell_refresh_line();
                        }
                    }
                    break;
                    
                case 'C':  /* Right arrow - ignore for now */
                    break;
                    
                case 'D':  /* Left arrow - ignore for now */
                    break;
                    
                case '3':  /* Delete key (followed by ~) */
                    break;
            }
            return;
        }
        
        escape_state = 0;
        escape_pos = 0;
        return;
    }
    
    switch (c) {
        case '\033':  /* Escape */
            escape_state = 1;
            escape_pos = 0;
            break;
            
        case '\r':    /* Enter */
        case '\n':
            shell_newline();
            shell_buffer[shell_buffer_pos] = '\0';
            shell_execute(shell_buffer);
            shell_buffer_pos = 0;
            shell_buffer[0] = '\0';
            shell_history_index = shell_history_count;
            shell_print_prompt();
            break;
            
        case 0x7F:    /* Backspace (DEL) */
        case '\b':    /* Backspace (BS) */
            if (shell_buffer_pos > 0) {
                shell_buffer_pos--;
                shell_buffer[shell_buffer_pos] = '\0';
                shell_puts("\b \b");  /* Erase character */
            }
            break;
            
        case '\t':    /* Tab - simple completion */
            /* TODO: implement tab completion */
            break;
            
        case 0x03:    /* Ctrl+C */
            shell_puts("^C\r\n");
            shell_buffer_pos = 0;
            shell_buffer[0] = '\0';
            shell_print_prompt();
            break;
            
        case 0x04:    /* Ctrl+D */
            if (shell_buffer_pos == 0) {
                shell_puts("\r\nLogout\r\n");
                /* Could exit shell here if desired */
            }
            break;
            
        case 0x0C:    /* Ctrl+L - clear screen */
            cmd_clear(0, NULL);
            shell_print_prompt();
            shell_puts(shell_buffer);
            break;
            
        default:
            /* Printable character */
            if (c >= 0x20 && c < 0x7F && shell_buffer_pos < SHELL_BUFFER_SIZE - 1) {
                shell_buffer[shell_buffer_pos++] = c;
                shell_buffer[shell_buffer_pos] = '\0';
                shell_putchar(c);
            }
            break;
    }
}

/* ===========================================================================
 * Shell main loop
 * ===========================================================================*/

static void shell_banner(void)
{
    shell_puts("\033[2J\033[H");  /* Clear screen */
    shell_puts("\r\n");
    shell_puts("\033[36m");  /* Cyan color */
    shell_puts("  __  __ _               _  __                    _ \r\n");
    shell_puts(" |  \\/  (_) ___ _ __ ___| |/ /___ _ __ _ __   ___| |\r\n");
    shell_puts(" | |\\/| | |/ __| '__/ _ \\ ' // _ \\ '__| '_ \\ / _ \\ |\r\n");
    shell_puts(" | |  | | | (__| | | (_) | < |  __/ |  | | | |  __/ |\r\n");
    shell_puts(" |_|  |_|_|\\___|_|  \\___/|_|\\_\\___|_|  |_| |_|\\___|_|\r\n");
    shell_puts("\033[0m");  /* Reset color */
    shell_puts("\r\n");
    shell_puts("  MicroKernel v0.1.0 - A minimal x86_64 microkernel\r\n");
    shell_puts("  Type 'help' for available commands.\r\n");
    shell_puts("\r\n");
}

/*
 * Initialize and run the shell
 * This function does not return
 */
void shell_init(void)
{
    /* Initialize serial port for input */
    serial_init();
    
    /* Reset state */
    shell_buffer_pos = 0;
    shell_buffer[0] = '\0';
    shell_history_count = 0;
    shell_history_index = 0;
    shell_running = true;
    
    /* Print banner */
    shell_banner();
    
    /* Print initial prompt */
    shell_print_prompt();
    
    /* Main shell loop */
    while (shell_running) {
        /* Read character from serial port */
        int c = serial_try_getchar();
        
        if (c >= 0) {
            shell_handle_char((char)c);
        } else {
            /* No input, update tick counter and yield CPU */
            jiffies++;
            __asm__ __volatile__("pause");
        }
    }
}

/*
 * Run shell (alias for shell_init)
 */
void shell_run(void)
{
    shell_init();
}

/*
 * Process a single character (for interrupt-driven input)
 */
void shell_input_char(char c)
{
    shell_handle_char(c);
}

/* A small serial command shell for kernel debugging. */

#include "shell.h"

extern "C" {

#define SHELL_BUFFER_SIZE 256
#define SHELL_HISTORY_SIZE 10
#define SHELL_MAX_ARGS 16
#define SHELL_PROMPT "microkernel> "

extern void serial_putc(char c);
extern int  serial_try_getchar(void);

static int shell_strcmp(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }

    return *(const unsigned char *)left - *(const unsigned char *)right;
}

static int shell_copy(char *destination, const char *source)
{
    int length = 0;

    while ((destination[length] = source[length]) != '\0')
        length++;

    return length;
}

static int shell_isspace(char value)
{
    return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

static void shell_putchar(char value)
{
    serial_putc(value);
}

static void shell_puts(const char *text)
{
    while (*text != '\0')
        shell_putchar(*text++);
}

static void shell_newline(void)
{
    shell_puts("\r\n");
}

static void shell_print_int(long value)
{
    char          buffer[32];
    unsigned long magnitude;
    int           length = 0;

    if (value < 0) {
        shell_putchar('-');
        magnitude = (unsigned long)(-(value + 1)) + 1;
    } else {
        magnitude = (unsigned long)value;
    }

    do {
        buffer[length++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);

    while (length > 0)
        shell_putchar(buffer[--length]);
}

static char shell_buffer[SHELL_BUFFER_SIZE];
static int  shell_buffer_pos;
static char shell_history[SHELL_HISTORY_SIZE][SHELL_BUFFER_SIZE];
static int  shell_history_count;
static int  shell_history_index;

static void shell_history_add(const char *command)
{
    int index;
    int length;

    if (*command == '\0')
        return;

    if (shell_history_count > 0) {
        index = (shell_history_count - 1) % SHELL_HISTORY_SIZE;
        if (shell_strcmp(shell_history[index], command) == 0) {
            shell_history_index = shell_history_count;
            return;
        }
    }

    index = shell_history_count % SHELL_HISTORY_SIZE;
    for (length = 0; length < SHELL_BUFFER_SIZE - 1 && command[length] != '\0';
         length++)
        shell_history[index][length] = command[length];
    shell_history[index][length] = '\0';
    shell_history_count++;
    shell_history_index = shell_history_count;
}

static const char *shell_history_get(int offset)
{
    int first = shell_history_count > SHELL_HISTORY_SIZE
                    ? shell_history_count - SHELL_HISTORY_SIZE
                    : 0;
    int index = shell_history_index + offset;

    if (index < first || index >= shell_history_count)
        return nullptr;

    shell_history_index = index;
    return shell_history[index % SHELL_HISTORY_SIZE];
}

static int shell_parse_args(char *line, char *argv[], int max_args)
{
    int   argc   = 0;
    char *cursor = line;

    while (*cursor != '\0' && argc < max_args) {
        while (*cursor != '\0' && shell_isspace(*cursor))
            *cursor++ = '\0';

        if (*cursor == '\0')
            break;

        if (*cursor == '"' || *cursor == '\'') {
            char quote   = *cursor++;
            argv[argc++] = cursor;
            while (*cursor != '\0' && *cursor != quote)
                cursor++;
            if (*cursor != '\0')
                *cursor++ = '\0';
        } else {
            argv[argc++] = cursor;
            while (*cursor != '\0' && !shell_isspace(*cursor))
                cursor++;
        }
    }

    return argc;
}

static void cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    shell_newline();
    shell_puts("Commands:\r\n");
    shell_puts("  help       Show this help\r\n");
    shell_puts("  version    Show kernel version\r\n");
    shell_puts("  clear      Clear the terminal\r\n");
    shell_puts("  echo       Print text\r\n");
    shell_puts("  history    Show command history\r\n");
}

static void cmd_version(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    shell_puts("\r\nMicroKernel v" MICROKERNEL_VERSION "\r\n");
    shell_puts("Architecture: x86_64\r\n");
}

static void cmd_clear(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    shell_puts("\033[2J\033[H");
}

static void cmd_echo(int argc, char *argv[])
{
    shell_newline();

    for (int index = 1; index < argc; index++) {
        if (index > 1)
            shell_putchar(' ');
        shell_puts(argv[index]);
    }

    shell_newline();
}

static void cmd_history(int argc, char *argv[])
{
    int first = shell_history_count > SHELL_HISTORY_SIZE
                    ? shell_history_count - SHELL_HISTORY_SIZE
                    : 0;

    (void)argc;
    (void)argv;

    shell_puts("\r\nCommand history:\r\n");
    for (int index = first; index < shell_history_count; index++) {
        shell_puts("  ");
        shell_print_int(index + 1);
        shell_puts("  ");
        shell_puts(shell_history[index % SHELL_HISTORY_SIZE]);
        shell_newline();
    }

    if (shell_history_count == 0)
        shell_puts("  (empty)\r\n");
}

typedef void (*shell_command_function)(int argc, char *argv[]);

struct shell_command {
    const char            *name;
    shell_command_function function;
};

static const struct shell_command shell_commands[] = {
    {"help", cmd_help}, {"version", cmd_version}, {"clear", cmd_clear},
    {"echo", cmd_echo}, {"history", cmd_history}, {nullptr, nullptr},
};

static void shell_execute(char *line)
{
    char *argv[SHELL_MAX_ARGS];
    int   argc;

    while (*line != '\0' && shell_isspace(*line))
        line++;

    if (*line == '\0')
        return;

    shell_history_add(line);
    argc = shell_parse_args(line, argv, SHELL_MAX_ARGS);
    if (argc == 0)
        return;

    for (const struct shell_command *command = shell_commands;
         command->name != nullptr; command++) {
        if (shell_strcmp(argv[0], command->name) == 0) {
            command->function(argc, argv);
            return;
        }
    }

    shell_puts("\r\nUnknown command: ");
    shell_puts(argv[0]);
    shell_puts("\r\nType 'help' for available commands.\r\n");
}

static void shell_clear_line(void)
{
    shell_puts("\r\033[K");
    shell_puts(SHELL_PROMPT);
}

static void shell_refresh_line(void)
{
    shell_clear_line();
    shell_buffer[shell_buffer_pos] = '\0';
    shell_puts(shell_buffer);
}

static void shell_handle_escape(char value, int *escape_state)
{
    *escape_state = 0;

    if (value == 'A' || value == 'B') {
        const char *command = shell_history_get(value == 'A' ? -1 : 1);

        if (command != nullptr) {
            shell_buffer_pos = shell_copy(shell_buffer, command);
        } else {
            shell_buffer[0]  = '\0';
            shell_buffer_pos = 0;
        }
        shell_refresh_line();
    }
}

static void shell_handle_char(char value)
{
    static int escape_state;

    if (escape_state == 1) {
        escape_state = value == '[' ? 2 : 0;
        return;
    }

    if (escape_state == 2) {
        shell_handle_escape(value, &escape_state);
        return;
    }

    switch (value) {
    case '\033':
        escape_state = 1;
        break;
    case '\r':
    case '\n':
        shell_newline();
        shell_buffer[shell_buffer_pos] = '\0';
        shell_execute(shell_buffer);
        shell_buffer_pos    = 0;
        shell_buffer[0]     = '\0';
        shell_history_index = shell_history_count;
        shell_puts(SHELL_PROMPT);
        break;
    case '\b':
    case 0x7F:
        if (shell_buffer_pos > 0) {
            shell_buffer[--shell_buffer_pos] = '\0';
            shell_puts("\b \b");
        }
        break;
    case 0x03:
        shell_puts("^C\r\n");
        shell_buffer_pos = 0;
        shell_buffer[0]  = '\0';
        shell_puts(SHELL_PROMPT);
        break;
    case 0x0C:
        cmd_clear(0, nullptr);
        shell_puts(SHELL_PROMPT);
        shell_puts(shell_buffer);
        break;
    default:
        if (value >= 0x20 && value < 0x7F &&
            shell_buffer_pos < SHELL_BUFFER_SIZE - 1) {
            shell_buffer[shell_buffer_pos++] = value;
            shell_buffer[shell_buffer_pos]   = '\0';
            shell_putchar(value);
        }
        break;
    }
}

void shell_run(void)
{
    shell_buffer_pos    = 0;
    shell_buffer[0]     = '\0';
    shell_history_count = 0;
    shell_history_index = 0;

    shell_puts("MicroKernel v" MICROKERNEL_VERSION "\r\n");
    shell_puts("Type 'help' for commands.\r\n\r\n");
    shell_puts(SHELL_PROMPT);

    for (;;) {
        int value = serial_try_getchar();

        if (value >= 0)
            shell_handle_char((char)value);
        else
            __asm__ __volatile__("pause");
    }
}
}

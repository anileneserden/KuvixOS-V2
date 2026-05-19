#include <lib/commands.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

// Linker Script sembolleri
extern command_t _cmd_start[];
extern command_t _cmd_end[];

// ------------------------------------------------------------
// Output routing
// ------------------------------------------------------------
static commands_out_fn_t g_out = NULL;
static void* g_out_user = NULL;

static commands_clear_fn_t g_clear = NULL;
static void* g_clear_user = NULL;

static const char* g_cwd = "/";

void commands_set_cwd(const char* abs_cwd) {
    g_cwd = (abs_cwd && abs_cwd[0]) ? abs_cwd : "/";
}

const char* commands_get_cwd(void) {
    return g_cwd ? g_cwd : "/";
}

void commands_set_output(commands_out_fn_t fn, void* user) {
    g_out = fn;
    g_out_user = user;
}

void commands_puts(const char* s) {
    if (!s) return;

    if (g_out) {
        g_out(g_out_user, s);
        // 🚫 Eski GUI yenileme kodları (desktop_request_redraw, wm_draw, fb_present) temizlendi.
        // Artık saf TTY console veya seri port üzerinden akış doğrudan sağlanıyor.
    } else {
        printk("%s", s);
    }
}

void commands_putc(char c) {
    char tmp[2] = { c, 0 };
    commands_puts(tmp);
}

static void cmd_print_uint(uint32_t v, int base) {
    char buf[32];
    const char* dig = "0123456789ABCDEF";
    int i = 0;

    if (v == 0) { commands_putc('0'); return; }

    while (v > 0 && i < (int)sizeof(buf)) {
        buf[i++] = dig[v % (uint32_t)base];
        v /= (uint32_t)base;
    }
    while (i--) commands_putc(buf[i]);
}

static void cmd_print_int(int v) {
    if (v < 0) {
        commands_putc('-');
        cmd_print_uint((uint32_t)(-v), 10);
    } else {
        cmd_print_uint((uint32_t)v, 10);
    }
}

// Basit printf: %s %c %d %u %x %p %%
void commands_printf(const char* fmt, ...) {
    if (!fmt) return;

    va_list ap;
    va_start(ap, fmt);

    for (const char* p = fmt; *p; p++) {
        if (*p != '%') { commands_putc(*p); continue; }
        p++;
        if (!*p) break;

        switch (*p) {
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                commands_puts(s);
            } break;

            case 'c': {
                int c = va_arg(ap, int);
                commands_putc((char)c);
            } break;

            case 'd': {
                int v = va_arg(ap, int);
                cmd_print_int(v);
            } break;

            case 'u': {
                uint32_t v = va_arg(ap, uint32_t);
                cmd_print_uint(v, 10);
            } break;

            case 'x': {
                uint32_t v = va_arg(ap, uint32_t);
                commands_puts("0x");
                cmd_print_uint(v, 16);
            } break;

            case 'p': {
                uintptr_t v = (uintptr_t)va_arg(ap, void*);
                commands_puts("0x");
                cmd_print_uint((uint32_t)v, 16);
            } break;

            case '%':
                commands_putc('%');
                break;

            default:
                commands_putc('%');
                commands_putc(*p);
                break;
        }
    }

    va_end(ap);
}

// ------------------------------------------------------------
// Clear routing
// ------------------------------------------------------------

void commands_set_clear(commands_clear_fn_t fn, void* user) {
    g_clear = fn;
    g_clear_user = user;
}

void commands_clear(void) {
    if (g_clear) g_clear(g_clear_user);
}

// ------------------------------------------------------------
// Parser + Execute
// ------------------------------------------------------------
static int k_streq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static int split_line(char* line, char** argv, int max_args) {
    int argc = 0;
    char* p = line;

    while (*p && argc < max_args) {
        while (*p == ' ') p++;
        if (!*p) break;

        argv[argc++] = p;

        while (*p && *p != ' ') p++;

        if (*p == ' ') {
            *p = '\0';
            p++;
        }
    }
    return argc;
}

void commands_execute(char* line) {
    if (!line) return;

    char* argv[16];
    int argc = split_line(line, argv, 16);
    if (argc == 0) return;

    for (command_t* cmd = _cmd_start; cmd < _cmd_end; cmd++) {
        if (cmd->name && k_streq(argv[0], cmd->name)) {
            cmd->fn(argc, argv);
            return;
        }
    }

    commands_puts("Bilinmeyen komut: '");
    commands_puts(argv[0]);
    commands_puts("'. Yardim icin 'help' yazin.\n");
}
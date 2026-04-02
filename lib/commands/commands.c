#include <lib/commands.h>
#include <app/app_manager.h>
#include <kernel/printk.h>
#include <kernel/fs/vfs.h>
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
    } else {
        // fallback: seri/VGA
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

static int ends_with(const char* s, const char* suffix) {
    size_t s_len;
    size_t suffix_len;

    if (!s || !suffix) return 0;

    s_len = strlen(s);
    suffix_len = strlen(suffix);

    if (suffix_len > s_len) return 0;

    return strncmp(s + s_len - suffix_len, suffix, suffix_len) == 0;
}

static int looks_like_openable_path(const char* s) {
    if (!s || !s[0]) return 0;

    if (s[0] == '/' || (s[0] == '.' && s[1] == '/')) return 1;
    if (strchr(s, '/')) return 1;

    return ends_with(s, ".kef") ||
           ends_with(s, ".ksf") ||
           ends_with(s, ".txt") ||
           ends_with(s, ".kth") ||
           ends_with(s, ".html") ||
           ends_with(s, ".htm");
}

static void build_open_path(const char* input, char* out, size_t out_sz) {
    const char* cwd;

    if (!out || out_sz == 0) return;

    out[0] = '\0';

    if (!input || !input[0]) return;

    if (input[0] == '/') {
        strncpy(out, input, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    cwd = commands_get_cwd();
    if (!cwd || !cwd[0]) cwd = "/";

    strncpy(out, cwd, out_sz - 1);
    out[out_sz - 1] = '\0';

    if (strcmp(out, "/") != 0 && out[strlen(out) - 1] != '/') {
        strncat(out, "/", out_sz - strlen(out) - 1);
    }

    if (input[0] == '.' && input[1] == '/') {
        strncat(out, input + 2, out_sz - strlen(out) - 1);
        return;
    }

    strncat(out, input, out_sz - strlen(out) - 1);
}

static int try_open_path(const char* token) {
    char resolved[256];
    vfs_stat_t st;

    if (!looks_like_openable_path(token)) return 0;

    build_open_path(token, resolved, sizeof(resolved));

    if (vfs_stat(resolved, &st) == 1) {
        appmgr_open_path(resolved);
        return 1;
    }

    if (strcmp(resolved, token) != 0 && vfs_stat(token, &st) == 1) {
        appmgr_open_path(token);
        return 1;
    }

    return 0;
}

// Satırı boşluklara göre argv'ye böler
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

    if (try_open_path(argv[0])) {
        return;
    }

    commands_puts("Bilinmeyen komut: '");
    commands_puts(argv[0]);
    commands_puts("'. Yardim icin 'help' yazin.\n");
}
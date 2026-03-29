#include <lib/shell.h>

#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>
#include <lib/string.h>
#include <stdint.h>

#include <kernel/drivers/input/keyboard.h>

// ------------------------------------------------------------
// Identity + cwd
// ------------------------------------------------------------
static char g_username[32] = "root";
static char g_hostname[32] = "kuvix";
static char g_cwd[128]     = "/home";
static int g_dirty = 0;

// ------------------------------------------------------------
// Line editor state
// ------------------------------------------------------------
static char g_line[128];
static int  g_len = 0;
static int  g_inited = 0;

// ------------------------------------------------------------
// Commands routing
// ------------------------------------------------------------
static void shell_cmd_out(void* u, const char* s) {
    (void)u;
    if (!s) return;
    printk("%s", s);
    g_dirty = 1;
}

static void shell_cmd_clear(void* u) {
    (void)u;
    fb_console_clear();
    g_dirty = 1;
}

// ------------------------------------------------------------
// Echo helpers
// ------------------------------------------------------------
static inline void echo_char(uint8_t c) {
    printk("%c", (unsigned char)c);
    g_dirty = 1;
}

static inline void echo_newline(void) {
    printk("\n");
    g_dirty = 1;
}

static inline void echo_backspace(void) {
    printk("\b \b");
    g_dirty = 1;
}

// ------------------------------------------------------------
// Prompt
// ------------------------------------------------------------
static void shell_print_prompt(void) {
    fb_console_set_color(0x0000FF00, 0x00000000);
    printk("%s@%s", g_username, g_hostname);

    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk(":");

    fb_console_set_color(0x0000FF00, 0x00000000);
    if (strcmp(g_cwd, "/home/root") == 0) {
        printk("~");
    } else {
        printk("%s", g_cwd);
    }

    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk("$ ");

    g_dirty = 1;
}

// ------------------------------------------------------------
// Public setters
// ------------------------------------------------------------
void shell_set_username(const char* u) {
    if (!u) return;
    strncpy(g_username, u, sizeof(g_username) - 1);
    g_username[sizeof(g_username) - 1] = 0;
}

void shell_set_hostname(const char* h) {
    if (!h) return;
    strncpy(g_hostname, h, sizeof(g_hostname) - 1);
    g_hostname[sizeof(g_hostname) - 1] = 0;
}

void shell_set_cwd(const char* p) {
    if (!p) return;
    strncpy(g_cwd, p, sizeof(g_cwd) - 1);
    g_cwd[sizeof(g_cwd) - 1] = 0;
}

const char* shell_get_cwd(void) {
    return g_cwd;
}

// ------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------
void shell_init(void) {
    if (!g_inited) {
        commands_set_output(shell_cmd_out, NULL);
        commands_set_clear(shell_cmd_clear, NULL);

        strncpy(g_username, "root", sizeof(g_username) - 1);
        g_username[sizeof(g_username) - 1] = 0;

        strncpy(g_hostname, "kuvix", sizeof(g_hostname) - 1);
        g_hostname[sizeof(g_hostname) - 1] = 0;

        strncpy(g_cwd, "/home", sizeof(g_cwd) - 1);
        g_cwd[sizeof(g_cwd) - 1] = 0;

        g_inited = 1;
    }

    fb_console_clear();

    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n\n");

    g_len = 0;
    g_line[0] = 0;

    shell_print_prompt();
    fb_console_flush();
    g_dirty = 0;
}

// ------------------------------------------------------------
// Tick
// ------------------------------------------------------------
void shell_tick(void) {
    if (g_dirty) {
        fb_console_flush();
        g_dirty = 0;
    }
}

// ------------------------------------------------------------
// Input
// ------------------------------------------------------------
void shell_handle_scancode(uint16_t ev) {
    uint8_t sc = (uint8_t)(ev & 0xFF);
    uint8_t is_e0 = ((ev & 0xFF00) == 0xE000);

    if (is_e0) return;
    if (sc & 0x80) return;

    char c = kbd_scancode_to_ascii(sc);
    if (!c) return;

    if (c == '\n' || c == '\r') {
        g_line[g_len] = 0;

        echo_newline();

        if (g_len > 0) {
            commands_execute(g_line);
        }

        g_len = 0;
        g_line[0] = 0;

        shell_print_prompt();
        return;
    }

    if (c == '\b' || (uint8_t)c == 8 || (uint8_t)c == 127) {
        if (g_len > 0) {
            g_len--;
            g_line[g_len] = 0;
            echo_backspace();
        }
        return;
    }

    uint8_t uc = (uint8_t)c;
    if (uc >= 32) {
        if (g_len < (int)sizeof(g_line) - 1) {
            g_line[g_len++] = (char)uc;
            g_line[g_len] = 0;
            echo_char(uc);
        }
    }
}
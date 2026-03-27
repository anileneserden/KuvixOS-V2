// lib/shell/shell.c

#include <lib/shell/shell.h>
#include <lib/shell/shell_history.h>

#include <kernel/printk.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/input/keyboard.h>

#include <lib/string.h>
#include <stdint.h>

// ------------------------------------------------------------
// Debug (Makefile: -DKBD_SERIAL_DEBUG)
// ------------------------------------------------------------
#ifdef KBD_SERIAL_DEBUG
#define SHELL_KEY_DEBUG 1
#else
#define SHELL_KEY_DEBUG 0
#endif

static void shell_debug_key(uint16_t key) {
#if SHELL_KEY_DEBUG
    printk("\n[KEY raw=0x%x", (unsigned int)key);

    if ((key & 0xFF00) == 0xFF00) {
        uint8_t code = (uint8_t)(key & 0xFF);
        printk(" SPECIAL=0x%x", (unsigned int)code);

        if (code == KBD_UP)    printk(" UP");
        if (code == KBD_DOWN)  printk(" DOWN");
        if (code == KBD_LEFT)  printk(" LEFT");
        if (code == KBD_RIGHT) printk(" RIGHT");
    } else {
        uint8_t c = (uint8_t)key;

        if (c == '\n' || c == '\r') {
            printk(" ENTER");
        } else if (c == '\b' || c == 8 || c == 127) {
            printk(" BACKSPACE");
        } else if (c >= 32 && c <= 126) {
            printk(" CHAR='%c'", (char)c);
        } else {
            printk(" CHAR=0x%x", (unsigned int)c);
        }
    }

    printk("]\n");
    fb_console_flush();
#else
    (void)key;
#endif
}

// ------------------------------------------------------------
// Identity
// ------------------------------------------------------------
static char g_username[32] = "root";
static char g_hostname[32] = "kuvix";
static char g_cwd[128]     = "/home";

// ------------------------------------------------------------
// Line state
// ------------------------------------------------------------
static char g_line[128];
static int  g_len = 0;
static int  g_prompt_visible = 0;

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

// ------------------------------------------------------------
// Commands output routing
// ------------------------------------------------------------
static void shell_cmd_out(void* u, const char* s) {
    (void)u;
    if (!s) return;
    printk("%s", s);
    fb_console_flush();
}

static void shell_cmd_clear(void* u) {
    (void)u;
    fb_console_clear();
    fb_console_flush();
}

// ------------------------------------------------------------
// Prompt / echo helpers
// ------------------------------------------------------------
static void shell_print_prompt(void) {
    fb_console_set_color(0x0000FF00, 0x00000000);
    printk("%s@%s:%s", g_username, g_hostname, g_cwd);

    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk("$ ");
    fb_console_flush();

    g_prompt_visible = 1;
}

static inline void shell_echo_char(uint8_t c) {
    printk("%c", (unsigned char)c);
    fb_console_flush();
}

static inline void shell_echo_newline(void) {
    printk("\n");
    fb_console_flush();
}

static inline void shell_echo_backspace(void) {
    printk("\b \b");
    fb_console_flush();
}

// ------------------------------------------------------------
// Replace line (history navigation)
// ------------------------------------------------------------
static void shell_replace_line(const char* text) {
    if (!text) text = "";

    while (g_len > 0) {
        shell_echo_backspace();
        g_len--;
    }

    int len = (int)strlen(text);
    if (len > (int)sizeof(g_line) - 1) {
        len = (int)sizeof(g_line) - 1;
    }

    for (int i = 0; i < len; i++) {
        g_line[i] = text[i];
        shell_echo_char((uint8_t)text[i]);
    }

    g_len = len;
    g_line[g_len] = '\0';
}

// ------------------------------------------------------------
// Execute current line
// ------------------------------------------------------------
static void shell_submit_line(void) {
    g_line[g_len] = '\0';
    shell_echo_newline();

    if (g_len > 0) {
        shell_history_add(g_line);

        commands_set_output(shell_cmd_out, NULL);
        commands_set_clear(shell_cmd_clear, NULL);
        commands_execute(g_line);
        fb_console_flush();
    }

    g_len = 0;
    g_line[0] = '\0';
    g_prompt_visible = 0;
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void shell_init(void) {
    shell_history_init();

    commands_set_output(shell_cmd_out, NULL);
    commands_set_clear(shell_cmd_clear, NULL);

    g_len = 0;
    g_line[0] = '\0';
    g_prompt_visible = 0;

    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n");
    printk("Key debug: %s\n\n", SHELL_KEY_DEBUG ? "ON" : "OFF");
    fb_console_flush();
}

void shell_tick(void) {
    if (!g_prompt_visible) {
        shell_print_prompt();
    }
}

void shell_handle_key(uint16_t key) {
    if (!g_prompt_visible) {
        shell_print_prompt();
    }

    shell_debug_key(key);

#if SHELL_KEY_DEBUG
    // debug satırı prompt'u bozduğu için yeniden prompt bas
    g_prompt_visible = 0;
    shell_print_prompt();

    if (g_len > 0) {
        for (int i = 0; i < g_len; i++) {
            shell_echo_char((uint8_t)g_line[i]);
        }
    }
#endif

    // --------------------------------------------------------
    // Special keys
    // --------------------------------------------------------
    if ((key & 0xFF00) == 0xFF00) {
        uint8_t code = (uint8_t)(key & 0xFF);

        if (code == KBD_UP) {
            shell_replace_line(shell_history_prev());
        } else if (code == KBD_DOWN) {
            shell_replace_line(shell_history_next());
        }

        return;
    }

    // --------------------------------------------------------
    // Normal character
    // --------------------------------------------------------
    char c = (char)key;

    // ENTER
    if (c == '\n' || c == '\r') {
        shell_submit_line();
        return;
    }

    // BACKSPACE
    if (c == '\b' || (uint8_t)c == 8 || (uint8_t)c == 127) {
        if (g_len > 0) {
            g_len--;
            g_line[g_len] = '\0';
            shell_echo_backspace();
        }
        return;
    }

    // Printable char
    if ((uint8_t)c >= 32) {
        if (g_len < (int)sizeof(g_line) - 1) {
            g_line[g_len++] = c;
            g_line[g_len] = '\0';
            shell_echo_char((uint8_t)c);
        }
    }
}

// ------------------------------------------------------------
// Backward compatibility
// Eski kod hala shell_handle_scancode() çağırıyorsa kırılmasın
// ------------------------------------------------------------
void shell_handle_scancode(uint16_t sc) {
    shell_handle_key(sc);
}
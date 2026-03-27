// lib/shell/shell.c

#include <lib/shell/shell.h>
#include <lib/shell/shell_history.h>

#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/input/keyboard.h>

#include <stdint.h>
#include <lib/string.h>

// ------------------------------------------------------------
// Identity
// ------------------------------------------------------------
static char g_username[32] = "root";
static char g_hostname[32] = "kuvix";
static char g_cwd[128]     = "/home";

// ------------------------------------------------------------
// Prompt
// ------------------------------------------------------------
static void shell_print_prompt(void) {
    fb_console_set_color(0x0000FF00, 0x00000000);
    printk("%s@%s:%s", g_username, g_hostname, g_cwd);

    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk("$ ");

    fb_console_flush();
}

// ------------------------------------------------------------
// Echo
// ------------------------------------------------------------
static inline void echo_char(uint8_t c) {
    printk("%c", (unsigned char)c);
    fb_console_flush();
}

static inline void echo_newline(void) {
    printk("\n");
    fb_console_flush();
}

static inline void echo_backspace(void) {
    printk("\b \b");
    fb_console_flush();
}

// ------------------------------------------------------------
// Replace line (history)
// ------------------------------------------------------------
static void replace_line(char* buffer, int* i, int max_len, const char* cmd) {
    while (*i > 0) {
        echo_backspace();
        (*i)--;
    }

    int len = strlen(cmd);
    if (len > max_len - 1) len = max_len - 1;

    for (int j = 0; j < len; j++) {
        buffer[j] = cmd[j];
        echo_char(cmd[j]);
    }

    *i = len;
    buffer[*i] = '\0';
}

// ------------------------------------------------------------
// Readline
// ------------------------------------------------------------
void shell_readline(char* buffer, int max_len) {
    int i = 0;
    buffer[0] = '\0';

    while (i < max_len - 1) {
        kbd_poll();

        int key = kbd_get_char();
        if (key == 0) continue;

        // 🔥 SPECIAL KEYS
        if ((key & 0xFF00) == 0xFF00) {
            int code = key & 0xFF;

            if (code == KBD_UP) {
                replace_line(buffer, &i, max_len, shell_history_prev());
            }
            else if (code == KBD_DOWN) {
                replace_line(buffer, &i, max_len, shell_history_next());
            }

            continue;
        }

        char c = (char)key;

        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            echo_newline();
            return;
        }

        if (c == '\b' || (uint8_t)c == 127) {
            if (i > 0) {
                i--;
                buffer[i] = '\0';
                echo_backspace();
            }
            continue;
        }

        if ((uint8_t)c >= 32) {
            buffer[i++] = c;
            buffer[i] = '\0';
            echo_char(c);
        }
    }

    buffer[i] = '\0';
    echo_newline();
}

// ------------------------------------------------------------
// Shell main
// ------------------------------------------------------------
void shell_init(void) {
    kbd_init();
    shell_history_init();

    printk("KuvixOS Shell V2 Hazir!\n");
    printk("help yazabilirsin.\n\n");

    char line[128];

    while (1) {
        shell_print_prompt();

        shell_readline(line, sizeof(line));

        if (line[0]) {
            shell_history_add(line);
            commands_execute(line);
        }
    }
}
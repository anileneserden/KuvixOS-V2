// lib/shell.c
#include <lib/shell.h>
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>

// polling kullanıyorsan lazım
#include <kernel/drivers/input/keyboard.h>  // kbd_poll()

#include <stdint.h>
#include <lib/string.h>

// ------------------------------------------------------------
// Basit "identity" + cwd (ileride FS/VFS ile güncellenecek)
// ------------------------------------------------------------
static char g_username[32] = "root";
static char g_hostname[32] = "kuvix";
static char g_cwd[128]     = "/home";

// İstersen dışarıdan değiştirmek için (opsiyonel)
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

static void shell_print_prompt(void) {
    // user: yeşil
    fb_console_set_color(0x0000FF00, 0x00000000);
    printk("%s", g_username);
    printk("@%s", g_hostname);
    printk(":%s", g_cwd);

    // $: beyaz
    fb_console_set_color(0x00FFFFFF, 0x00000000);
    printk("$ ");

    fb_console_flush();
}

// ------------------------------------------------------------
// Echo helpers
// ------------------------------------------------------------
static inline void echo_char(uint8_t c) {
    // 127+ karakterlerde signed char bozulmasın
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
// Readline
// ------------------------------------------------------------
void shell_readline(char* buffer, int max_len) {
    int i = 0;
    if (max_len <= 0) return;
    buffer[0] = '\0';

    while (i < max_len - 1) {
        // IRQ yoksa buffer'ı besle (polling)
        kbd_poll();

        char c = 0;

        if (kbd_has_character()) {
            c = kbd_get_char();
        }

        if (c == 0) {
            asm volatile("pause");
            continue;
        }

        // ENTER
        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            echo_newline();
            return;
        }

        // BACKSPACE
        if (c == '\b' || (uint8_t)c == 8 || (uint8_t)c == 127) {
            if (i > 0) {
                i--;
                echo_backspace();
            }
            continue;
        }

        // Latin-1/CP1252 tarzı 0..255 kabul (kontrol karakterleri hariç)
        uint8_t uc = (uint8_t)c;
        if (uc >= 32) {
            buffer[i++] = (char)uc;
            echo_char(uc);
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

    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n");
    printk("FONT TEST: \xFD \xF0 \xFC \xFE \xF6 \xE7\n\n");
    fb_console_flush();

    char line[128];

    while (1) {
        shell_print_prompt();

        shell_readline(line, (int)sizeof(line));

        if (line[0] != '\0') {
            commands_execute(line);
            fb_console_flush();
        }
    }
}

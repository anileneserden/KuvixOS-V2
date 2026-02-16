// lib/shell.c
#include <lib/shell.h>
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>

// polling kullanıyorsan lazım
#include <kernel/drivers/input/keyboard.h>  // kbd_poll()

static inline void echo_char(uint8_t c) {
    // ✅ 127+ karakterlerde signed char bozulmasın
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

void shell_readline(char* buffer, int max_len) {
    int i = 0;
    buffer[0] = '\0';

    while (i < max_len - 1) {
        // ✅ IRQ yoksa buffer'ı besle (polling)
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

        // ✅ Latin-1/CP1252 tarzı 0..255 kabul (kontrol karakterleri hariç)
        uint8_t uc = (uint8_t)c;
        if (uc >= 32) {
            buffer[i++] = (char)uc;
            echo_char(uc);
        }
    }

    buffer[i] = '\0';
    echo_newline();
}

void shell_init(void) {
    kbd_init();

    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n");

    printk("FONT TEST: \xFD \xF0 \xFC \xFE \xF6 \xE7\n");

    printk("\n");
    fb_console_flush();

    char line[128];

    while (1) {
        printk("KuvixOS> ");
        fb_console_flush();

        shell_readline(line, (int)sizeof(line));

        if (line[0] != '\0') {
            commands_execute(line);
            fb_console_flush();
        }
    }
}

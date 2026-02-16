#include <lib/shell.h>
#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>

// ✅ BUNU EKLE
#include <kernel/drivers/input/keyboard.h>  // kbd_poll()

static inline void echo_char(char c) {
    printk("%c", c);
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

        if (c == '\n' || c == '\r') {
            buffer[i] = '\0';
            echo_newline();
            return;
        }

        if (c == '\b' || c == 8 || c == 127) {
            if (i > 0) {
                i--;
                echo_backspace();
            }
            continue;
        }

        if ((uint8_t)c >= 32 && (uint8_t)c <= 255) {
            buffer[i++] = c;
            echo_char(c);
        }
    }

    buffer[i] = '\0';
    echo_newline();
}

void shell_init(void) {
    kbd_init();

    printk("KuvixOS Shell V2 Hazir!\n");
    printk("Komutlar icin 'help' yazabilirsiniz.\n\n");
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

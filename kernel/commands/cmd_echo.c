#include <kernel/printk.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb_console.h>

// color.c içindeki global renkleri kullan
extern unsigned int color_get_fg(void);
extern unsigned int color_get_bg(void);

void cmd_echo(int argc, char** argv) {
    fb_console_set_color(color_get_fg(), color_get_bg()); // aktif renk

    for (int i = 1; i < argc; i++) {
        commands_printf(argv[i]);
        if (i < argc - 1) commands_puts(" ");
    }
    commands_puts("\n");

    // İstersen varsayılan renge dönme, çünkü color komutu zaten globali kontrol ediyor.
    // fb_console_set_color(0x00FFFFFF, 0x00000000);
}

REGISTER_COMMAND(echo, cmd_echo, "Metni ekrana yazdırır (renk destekli)");

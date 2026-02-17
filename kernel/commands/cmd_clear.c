#include <kernel/drivers/video/fb_console.h>
#include <lib/commands.h>

void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    fb_console_clear();
}

REGISTER_COMMAND(clear, cmd_clear, "Ekranı temizler");

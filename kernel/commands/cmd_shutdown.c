#include <ui/power_screen.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/power.h>

void cmd_shutdown(int argc, char** argv)
{
    if (argc >= 2) {
        if (!strcmp(argv[1], "--now") || !strcmp(argv[1], "now")) {
            power_shutdown();
            return;
        }

        commands_puts("Kullanim: shutdown [--now|now]\n");
        return;
    }

    ui_power_screen_shutdown(3);
}

REGISTER_COMMAND(shutdown, cmd_shutdown, "Sistemi guvenli bir sekilde kapatir");
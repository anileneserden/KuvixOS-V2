#include <ui/power_screen.h>
#include <lib/commands.h>

void cmd_shutdown(int argc, char** argv)
{
    (void)argc; (void)argv;
    ui_power_screen_shutdown(3);
}

REGISTER_COMMAND(shutdown, cmd_shutdown, "Sistemi guvenli bir sekilde kapatir");
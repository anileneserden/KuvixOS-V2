#include <lib/commands.h>

void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    commands_clear();
}

REGISTER_COMMAND(clear, cmd_clear, "Ekranı temizler");
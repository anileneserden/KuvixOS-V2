#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        commands_printf(argv[i]);
        if (i < argc - 1) commands_puts(" ");
    }
    commands_puts("\n");
}

REGISTER_COMMAND(echo, cmd_echo, "Metni ekrana yazdırır");
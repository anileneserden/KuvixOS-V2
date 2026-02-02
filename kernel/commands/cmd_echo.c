#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        printk(argv[i]);
        if (i < argc - 1) printk(" ");
    }
    printk("\n");
}

REGISTER_COMMAND(echo, cmd_echo, "Metni ekrana yazdirir");
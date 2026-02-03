#include <lib/commands.h>
#include <kernel/printk.h>

void cmd_hello_fn(int argc, char** argv) {
    (void)argc; (void)argv;
    printk("Merhaba! Linker Registry basariyla calisiyor.\n");
}

REGISTER_COMMAND(hello, cmd_hello_fn, "Selamlama komutu");
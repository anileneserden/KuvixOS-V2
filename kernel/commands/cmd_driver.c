#include <lib/commands.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <kernel/kdf.h>

void cmd_driver(int argc, char** argv) {
    if (argc < 3) {
        commands_puts("Usage: driver load <path>\n");
        return;
    }

    if (strcmp(argv[1], "load") == 0) {
        int ret = kdf_load_driver(argv[2]);
        
        if (ret == 0) {
            printk("Driver loaded successfully: %s\n", argv[2]);
        } else {
            printk("Failed to load driver (Error code: %d)\n", ret);
        }
    } else {
        commands_puts("Unknown driver subcommand. Use 'load'.\n");
    }
}

// Komut sistemine kaydet
REGISTER_COMMAND(driver, cmd_driver, "Driver management: driver load <path>");
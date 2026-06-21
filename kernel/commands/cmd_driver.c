#include <lib/commands.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <kernel/kdf.h>

void cmd_driver(int argc, char** argv) {
    // Argüman kontrolünü en az 2 yapıyoruz (driver list için 2 yeterli)
    if (argc < 2) {
        commands_puts("Usage:\n");
        commands_puts("  driver load <path>   -> Loads specified .kdf driver\n");
        commands_puts("  driver list          -> Lists all loaded dynamic drivers\n");
        return;
    }

    if (strcmp(argv[1], "load") == 0) {
        if (argc < 3) {
            commands_puts("Error: Please specify the driver path!\n");
            return;
        }
        
        int ret = kdf_load_driver(argv[2]);
        if (ret == 0) {
            printk("Driver loaded successfully: %s\n", argv[2]);
        } else {
            printk("Failed to load driver (Error code: %d)\n", ret);
        }
    } 
    // SENİN KODA EKLEDİĞİMİZ LİSTELEME ÖZELLİĞİ:
    else if (strcmp(argv[1], "list") == 0) {
        kdf_list_drivers(); 
    } 
    else {
        commands_puts("Unknown driver subcommand. Use 'load' or 'list'.\n");
    }
}

// Komut açıklamasına list özelliğini de ekledik
REGISTER_COMMAND(driver, cmd_driver, "Driver management: driver load <path> or driver list");
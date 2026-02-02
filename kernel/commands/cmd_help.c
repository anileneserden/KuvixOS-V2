#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern command_t _cmd_start;
    extern command_t _cmd_end;

    printk("KuvixOS V2 Yardim Menusu:\n");
    printk("--------------------------\n");

    for (command_t* cmd = &_cmd_start; cmd < &_cmd_end; cmd++) {
        // %-10s yerine basit bir boşluk veya tab kullanalım
        printk("  ");
        printk(cmd->name);
        printk(" - ");
        printk(cmd->help);
        printk("\n");
    }
}

// OTOMATİK KAYIT: Hiçbir yere include etmene gerek yok!
REGISTER_COMMAND(help, cmd_help, "Tum komutlari ve aciklamalarini listeler");
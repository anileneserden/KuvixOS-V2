#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern command_t _cmd_start;
    extern command_t _cmd_end;

    commands_puts("KuvixOS V2 Yardım Menüsü:\n");
    commands_puts("--------------------------\n");

    for (command_t* cmd = &_cmd_start; cmd < &_cmd_end; cmd++) {
        // %-10s yerine basit bir boşluk veya tab kullanalım
        commands_puts("  ");
        commands_printf(cmd->name);
        commands_puts(" - ");
        commands_printf(cmd->help);
        commands_puts("\n");
    }
}

// OTOMATİK KAYIT: Hiçbir yere include etmene gerek yok!
REGISTER_COMMAND(help, cmd_help, "Tüm komutları ve açıklamalarını listeler");
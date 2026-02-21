#include <kernel/printk.h>
#include <kernel/drivers/ata_pio.h>
#include <lib/commands.h>

void cmd_disks(int argc, char** argv) {
    (void)argc; (void)argv;

    commands_puts("KuvixOS Disk Listesi:\n");
    commands_puts("------------------------------------\n");
    commands_puts("No  Tip    Bağlantı         Boyut\n");
    
    // ATA sürücüsünden bilgileri çek
    ata_pio_print_info();
    
    // İleride buraya USB, RamFS vb. eklenebilir
    commands_puts("------------------------------------\n");
}

REGISTER_COMMAND(disks, cmd_disks, "Sistemdeki diskleri listeler");
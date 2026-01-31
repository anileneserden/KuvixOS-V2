#include <kernel/printk.h>
#include <kernel/drivers/ata_pio.h>
#include <lib/commands.h>

void cmd_disks(int argc, char** argv) {
    (void)argc; (void)argv;

    printk("KuvixOS Disk Listesi:\n");
    printk("------------------------------------\n");
    printk("No  Tip    Baglanti         Boyut\n");
    
    // ATA sürücüsünden bilgileri çek
    ata_pio_print_info();
    
    // İleride buraya USB, RamFS vb. eklenebilir
    printk("------------------------------------\n");
}

REGISTER_COMMAND(disks, cmd_disks, "Sistemdeki diskleri listeler");
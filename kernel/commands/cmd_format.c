#include <kernel/printk.h>
#include <kernel/fs/kvxfs.h>
#include <lib/commands.h>

void cmd_format(int argc, char** argv) {
    (void)argc; (void)argv;
    printk("KuvixOS: Manuel format baslatiliyor...\n");
    
    if (kvxfs_force_format()) {
        printk("Disk basariyla formatlandi ve baglandi.\n");
    } else {
        printk("HATA: Format islemi hala basarisiz. ATA surucusunu kontrol edin.\n");
    }
}

REGISTER_COMMAND(format, cmd_format, "Kalici diski (KVXFS) manuel olarak formatlar");
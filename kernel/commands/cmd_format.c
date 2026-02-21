#include <kernel/printk.h>
#include <kernel/fs/kvxfs.h>
#include <lib/commands.h>

void cmd_format(int argc, char** argv) {
    (void)argc; (void)argv;
    commands_puts("KuvixOS: Manuel format başlatılıyor...\n");
    
    if (kvxfs_force_format()) {
        commands_puts("Disk başarıyla formatlandı ve baglandı.\n");
    } else {
        commands_puts("HATA: Format işlemi hala başarısız. ATA sürücüsünü kontrol edin.\n");
    }
}

REGISTER_COMMAND(format, cmd_format, "Kalıcı diski (KVXFS) manuel olarak formatlar");
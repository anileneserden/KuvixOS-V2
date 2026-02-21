#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_install(int argc, char** argv) {
    (void)argc; (void)argv;
    commands_puts("KuvixOS Kurulumu Başlatılıyor...\n");

    // 1. Gerekli klasörleri oluştur
    commands_puts("Sistem dizinleri oluşturuluyor...\n");
    kvxfs_mkdir("/persist/system");
    kvxfs_mkdir("/persist/boot");

    // 2. Kernel'ı kopyala (Şimdilik sembolik, ileride vfs_read ile kernel.elf kopyalanacak)
    // Test amaçlı bir sistem dosyası yazalım
    const char* config_data = "KuvixOS V2 - Installed\nBootMode=ATA_PIO\n";
    if (kvxfs_write_all("/persist/system/os.conf", (uint8_t*)config_data, 42)) {
        commands_puts("Kurulum Tamamlandı! /persist/system/os.conf oluşturuldu.\n");
    } else {
        commands_puts("HATA: Dosya yazılamadı.\n");
    }
}

REGISTER_COMMAND(install, cmd_install, "Sistemi diske kurar");
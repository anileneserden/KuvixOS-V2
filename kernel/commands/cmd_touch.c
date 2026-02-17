#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: touch /persist/dosya_adı\n");
        return;
    }

    const char* path = argv[1];

    // Sadece /persist/ dizinine izin veriyoruz
    if (strncmp(path, "/persist/", 9) != 0) {
        printk("Hata: Sadece /persist/ altında dosya oluşturulabilir.\n");
        return;
    }

    // Boş bir içerik oluşturuyoruz (0 byte)
    uint8_t empty_data = 0;
    
    // kvxfs_write_all kullanarak diske yazıyoruz
    if (kvxfs_write_all(path, &empty_data, 0)) {
        printk("Dosya oluşturuldu: %s\n", path);
    } else {
        printk("Hata: Dosya oluşturulamadı!\n");
    }
}

REGISTER_COMMAND(touch, cmd_touch, "Yeni bir boş dosya oluşturur");
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: touch <dosya_yolu>\n");
        return;
    }

    const char* path = argv[1];

    // Sabit /persist/ kontrolünü kaldırdık, doğrudan yazıyoruz!
    uint8_t empty_data = 0;
    if (kvxfs_write_all(path, &empty_data, 0)) {
        printk("Dosya olusturuldu: %s\n", path);
    } else {
        printk("Hata: Dosya olusturulamadı!\n");
    }
}

REGISTER_COMMAND(touch, cmd_touch, "Yeni bir boş dosya oluşturur");
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <lib/shell.h>

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: touch <dosya_adi>\n");
        return;
    }

    char full_path[128];
    const char* target = argv[1];
    const char* cwd = shell_get_cwd();

    // Yol birleştirme
    if (target[0] == '/') {
        strncpy(full_path, target, sizeof(full_path) - 1);
    } else {
        strcpy(full_path, cwd);
        if (full_path[strlen(full_path)-1] != '/') strcat(full_path, "/");
        strcat(full_path, target);
    }

    // DISK KONTROLÜ (Hatanın kaynağını burada yakalayabiliriz)
    // kvxfs_is_mounted() gibi bir fonksiyonun varsa burada kontrol et
    
    uint8_t dummy = 0;
    if (kvxfs_write_all(full_path, &dummy, 0)) {
        printk("Dosya olusturuldu: %s\n", full_path);
    } else {
        // Hata zaten KVXFS içindeki printk ile basılıyor
        printk("Hata: %s olusturulamadi!\n", full_path);
    }
}

REGISTER_COMMAND(touch, cmd_touch, "Yeni bir boş dosya oluşturur");
#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: touch <dosya_adi_veya_yolu>\n");
        printk("Ornek: touch notlar.txt VEYA touch /home/anil/test.txt\n");
        return;
    }

    const char* path = argv[1];
    char target_path[VFS_PATH_MAX] = {0};

    if (path[0] == '/') {
        strncpy(target_path, path, VFS_PATH_MAX - 1);
    } else {
        const char* current_cwd = vfs_get_cwd();
        if (!current_cwd) {
            current_cwd = "/";
        }
        
        strncpy(target_path, current_cwd, VFS_PATH_MAX - 1);
        
        size_t len = strlen(target_path);
        if (len > 0 && target_path[len - 1] != '/') {
            strcat(target_path, "/");
        }
        strcat(target_path, path);
    }

    uint8_t empty_data = 0;
    if (kvxfs_write_all(target_path, &empty_data, 0)) {
        printk("Dosya olusturuldu: %s\n", target_path);
    } else {
        printk("Hata: Dosya olusturulamadı: %s\n", target_path);
    }
}

REGISTER_COMMAND(touch, cmd_touch, "Yeni bir boş dosya oluşturur");
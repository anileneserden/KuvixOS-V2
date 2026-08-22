#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/fs/kvxfs.h>

// Octal string'i (örn: "600" veya "755") uint16_t değerine çeviren yardımcı fonksiyon
static uint16_t parse_octal(const char* str) {
    uint16_t val = 0;
    while (*str >= '0' && *str <= '7') {
        val = val * 8 + (*str - '0');
        str++;
    }
    return val;
}

void cmd_chmod(int argc, char** argv) {
    if (argc < 3) {
        printk("Kullanım: chmod <izin> <dosya_adi_veya_yolu>\n");
        printk("Örnek: chmod 600 notlar.txt VEYA chmod 755 /home/anil/script.sh\n");
        return;
    }

    uint16_t perms = parse_octal(argv[1]);
    const char* path = argv[2];
    char target_path[VFS_PATH_MAX] = {0};

    // 1. Yol kombinasyonunu hazırla (Göreli yol mu, Tam yol mu?)
    if (path[0] == '/') {
        // Kullanıcı tam yol girdiyse doğrudan kopyala
        strncpy(target_path, path, VFS_PATH_MAX - 1);
    } else {
        // Kullanıcı sadece dosya adı girdiyse mevcut CWD ile harmanla
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

    // 2. Hazırlanan target_path üzerinden kvxfs_chmod fonksiyonunu çağır
    if (kvxfs_chmod(target_path, perms)) {
        printk("Başarılı: %s izinleri güncellendi (0%o).\n", target_path, perms);
    } else {
        printk("Hata: Dosya bulunamadı veya izinler güncellenemedi: %s\n", target_path);
    }
}

REGISTER_COMMAND(chmod, cmd_chmod, "Changes file permissions");
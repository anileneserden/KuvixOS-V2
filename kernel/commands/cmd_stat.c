#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_stat(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: stat <dosya_adi_veya_yolu>\n");
        printk("Örnek: stat test.txt\n");
        return;
    }

    const char* path = argv[1];
    char target_path[VFS_PATH_MAX] = {0};

    // cat ve chmod ile aynı akıllı yol çözümleme mantığı
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

    vfs_stat_t st;
    if (vfs_stat(target_path, &st)) {
        printk("--- Dosya Bilgileri: %s ---\n", target_path);
        printk("Tür        : %s\n", (st.type == VFS_T_DIR) ? "Dizin (Klasör)" : "Dosya");
        printk("Boyut      : %d bytes\n", st.size);
        // %o yerine desimal ve hex (örneğin 0644 için 420 veya 0x1a4) olarak basalım
        printk("İzinler (Dec): %d (Hex: %x)\n", st.permissions, st.permissions);
        printk("Sahip UID  : %d\n", st.owner_uid);
        printk("Backend    : %d\n", st.backend);
    } else {
        printk("Hata: Dosya bulunamadı veya stat alınamadı: %s\n", target_path);
    }
}

REGISTER_COMMAND(stat, cmd_stat, "Dosya veya dizin detaylı meta verilerini gosterir");
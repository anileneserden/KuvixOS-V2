#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

// Yolu CWD ile harmanlayan yardımcı fonksiyonumuz
static void resolve_mv_path(const char* src, char* dest) {
    if (src[0] == '/') {
        strncpy(dest, src, VFS_PATH_MAX - 1);
    } else {
        const char* current_cwd = vfs_get_cwd();
        if (!current_cwd) {
            current_cwd = "/";
        }
        
        strncpy(dest, current_cwd, VFS_PATH_MAX - 1);
        size_t len = strlen(dest);
        if (len > 0 && dest[len - 1] != '/') {
            strcat(dest, "/");
        }
        strcat(dest, src);
    }
}

void cmd_mv(int argc, char** argv) {
    if (argc < 3) {
        printk("Kullanim: mv <kaynak_yolu> <hedef_yolu>\n");
        printk("Ornek: mv dosya.txt yeni_ad.txt\n");
        printk("Ornek: mv dosya.txt /home/anil/deneme/\n");
        return;
    }

    const char* src_param = argv[1];
    const char* dest_param = argv[2];

    char full_src_path[VFS_PATH_MAX] = {0};
    char full_dest_path[VFS_PATH_MAX] = {0};

    // 1. Kaynak ve Hedef yollarını CWD'ye göre çöz
    resolve_mv_path(src_param, full_src_path);
    resolve_mv_path(dest_param, full_dest_path);

    // 2. Kritik Güvenlik Kontrolü
    if (strcmp(full_src_path, "/") == 0 || strcmp(full_src_path, "/home") == 0 || strcmp(full_src_path, "/sys") == 0) {
        printk("Hata: Kritik sistem dizinlerinin yeri degistirilemez!\n");
        return;
    }

    // 3. VFS üzerinden rename/move tetiklemesi (vfs_rename kullanıyoruz)
    if (vfs_rename(full_src_path, full_dest_path)) {
        printk("Tasindi/Yeniden adlandirildi: %s -> %s\n", full_src_path, full_dest_path);
    } else {
        printk("Hata: Tasima veya adlandirma islemi basarisiz!\n");
    }
}

REGISTER_COMMAND(mv, cmd_mv, "Dosya veya dizinleri tasir / yeniden adlandirir");
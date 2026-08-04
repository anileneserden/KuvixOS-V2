#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: rm <dosya_veya_klasor>\n");
        printk("Ornek: rm test.txt VEYA rm /home/anil/test.txt\n");
        return;
    }

    const char* path = argv[1];
    char target_path[VFS_PATH_MAX] = {0};

    // 1. Kritik Güvenlik Kontrolü: Sistem dosyalarının yanlışlıkla uçurulmasını önleyelim
    if (strcmp(path, "/") == 0 || strcmp(path, "/home") == 0 || strcmp(path, "/home/anil") == 0 || strcmp(path, "/sys") == 0) {
        printk("Hata: Kritik sistem dizinleri silinemez!\n");
        return;
    }

    // 2. Yol kombinasyonunu hazırla (Göreli yol mu, Tam yol mu?)
    if (path[0] == '/') {
        // Kullanıcı "/home/anil/selam.txt" gibi tam yol girdiyse doğrudan kopyala
        strncpy(target_path, path, VFS_PATH_MAX - 1);
    } else {
        // Kullanıcı sadece "selam.txt" girdiyse, mevcut aktif çalışma dizinini (CWD) al ve birleştir
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

    // 3. Doğrudan VFS katmanının kendi güvenli silme fonksiyonunu çağırıyoruz.
    // vfs_remove arka planda zaten yolu normalize edip doğrudan kvxfs_remove kancasını tetikler.
    if (vfs_remove(target_path)) {
        printk("Silindi: %s\n", target_path);
    } else {
        printk("Hata: Dosya veya dizin silinemedi: %s\n", target_path);
    }
}

REGISTER_COMMAND(rm, cmd_rm, "Dosya veya dizini siler");
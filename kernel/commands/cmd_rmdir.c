#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_rmdir(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: rmdir <dizin_adi>\n");
        printk("Ornek: rmdir bos_klasor VEYA rmdir /home/anil/bos_klasor\n");
        return;
    }

    const char* path = argv[1];
    char target_path[VFS_PATH_MAX] = {0};

    // Kritik Güvenlik Kontrolü: Sistem klasörlerinin korunması
    if (strcmp(path, "/") == 0 || strcmp(path, "/home") == 0 || strcmp(path, "/home/anil") == 0 || strcmp(path, "/sys") == 0) {
        printk("Hata: Kritik sistem dizinleri rmdir ile silinemez!\n");
        return;
    }

    // 1. Yol kombinasyonunu hazırla (Göreli yol mu, Tam yol mu?)
    if (path[0] == '/') {
        strncpy(target_path, path, VFS_PATH_MAX - 1);
    } else {
        const char* current_cwd = vfs_get_cwd();
        if (!current_cwd) current_cwd = "/";
        
        strncpy(target_path, current_cwd, VFS_PATH_MAX - 1);
        size_t len = strlen(target_path);
        if (len > 0 && target_path[len - 1] != '/') {
            strcat(target_path, "/");
        }
        strcat(target_path, path);
    }

    // 2. Senin Harika Fikrin: Klasör boş mu dolumu kontrolü (VFS üzerinden)
    // vfs_is_dir_empty veya doğrudan vfs_remove çağrısı kontrolü:
    // Bizim alt katmandaki FUSE (KMS) zaten rmdir fonksiyonunda klasör boyutu ve doluluk kontrolü yapabiliyor.
    // Ancak çekirdek seviyesinde de vfs_remove klasörün durumuna göre başarılı/başarısız döner.
    
    // Doğrudan VFS katmanının kendi güvenli rmdir/remove mekanizmasını tetikliyoruz.
    // vfs_remove arka planda zaten yolu kendi temizler.
    if (vfs_remove(target_path)) {
        printk("Dizin basariyla silindi: %s (Kod: 0)\n", target_path);
    } else {
        // Eğer alt tarafta klasör doluysa veya yoksa hata dönecektir
        printk("Hata: Dizin silinemedi! Dizin bos olmayabilir veya mevcut degil. (Kod: 1)\n");
    }
}

REGISTER_COMMAND(rmdir, cmd_rmdir, "Sadece bos dizinleri siler");
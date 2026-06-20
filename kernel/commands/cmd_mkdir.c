#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: mkdir <dizin_adi>\n");
        printk("Ornek: mkdir yeni_klasor VEYA mkdir /home/anil/yeni_klasor\n");
        return;
    }

    const char* path = argv[1];
    char target_path[VFS_PATH_MAX] = {0};

    // 1. Yol kombinasyonunu hazırla (Göreli yol mu, Tam yol mu?)
    if (path[0] == '/') {
        // Kullanıcı "/home/anil/test" gibi tam yol girdiyse doğrudan kopyala
        strncpy(target_path, path, VFS_PATH_MAX - 1);
    } else {
        // Kullanıcı sadece "test" girdiyse, mevcut çalışma dizinini (CWD) al ve birleştir
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

    // 2. Doğrudan VFS katmanının kendi güvenli mkdir fonksiyonunu çağırıyoruz.
    // vfs_mkdir arka planda zaten yolu normalize edip kvxfs_mkdir'e paslayacaktır.
    if (vfs_mkdir(target_path)) {
        printk("Dizin olusturuldu: %s\n", target_path);
    } else {
        printk("Hata: Dizin olusturulamadi: %s\n", target_path);
    }
}

REGISTER_COMMAND(mkdir, cmd_mkdir, "Dizin olusturur");
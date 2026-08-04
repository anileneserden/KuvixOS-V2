#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_cd(int argc, char** argv) {
    // 1. Parametre girilmediyse varsayılan olarak kök dizine ("/") yönlendir
    const char* path = (argc > 1) ? argv[1] : "/";
    char target_path[VFS_PATH_MAX] = {0};

    // 2. Yol kombinasyonunu hazırla (Göreli yol mu, Tam yol mu?)
    if (path[0] == '/') {
        // Kullanıcı "cd /home" gibi tam yol girdiyse doğrudan kopyala
        strncpy(target_path, path, VFS_PATH_MAX - 1);
    } else {
        // Kullanıcı "cd deneme" gibi göreli yol girdiyse, mevcut aktif dizini al ve birleştir
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

    // 3. Doğrudan VFS katmanının kendi güvenli dizin değiştirme fonksiyonunu çağır
    // vfs_set_cwd arka planda zaten yolu normalize eder ve geçerliliğini doğrular
    if (vfs_set_cwd(target_path)) {
        return;
    }

    // Eğer başarısız olursa hata mesajı bas
    printk("Hata: Dizin degistirilemedi: %s\n", path);
}

REGISTER_COMMAND(cd, cmd_cd, "Calisma dizinini degistirir");
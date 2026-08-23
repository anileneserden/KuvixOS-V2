#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/fs/kvxfs.h>
#include <init/session.h>

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: cat <dosya_adi_veya_yolu>\n");
        printk("Ornek: cat notlar.txt VEYA cat /home/anil/deneme.txt\n");
        return;
    }

    const char* path = argv[1];
    char target_path[VFS_PATH_MAX] = {0};

    // Yol birleştirme işlemleri (aynı kalıyor)
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

    // 🔹 ÖNCE DOSYANIN VARLIĞINI VE İZİN DURUMUNU KONTROL EDELİM
    // (İsteğe bağlı: kvxfs içinde dosya index'ini bulan bir fonksiyonun varsa doğrudan sorgulayabilirsin)
    // Şimdilik kvxfs_read_all başarısız olduğunda nedenini anlamak için basit bir kontrol:
    
    static uint8_t cat_buf[4096]; 
    uint32_t read_size = 0;

    if (kvxfs_read_all(target_path, cat_buf, sizeof(cat_buf) - 1, &read_size)) {
        cat_buf[read_size] = '\0';
        printk("%s\n", (const char*)cat_buf);
    } else {
        // Dosya okunamadı. Acaba dosya var mı yoksa yetki mi yok?
        // Oturumu kontrol edelim:
        user_session_t* session = session_get_current();
        if (session && session->uid != 0 && strcmp(target_path, "/etc/passwd") == 0) {
            printk("Hata: Erişim reddedildi (Permission Denied): %s\n", target_path);
        } else {
            printk("Hata: Dosya bulunamadı veya okunamadı: %s\n", target_path);
        }
    }
}

REGISTER_COMMAND(cat, cmd_cat, "Displays file content");
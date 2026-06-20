#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/fs/kvxfs.h>

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanım: cat <dosya_adi_veya_yolu>\n");
        printk("Ornek: cat notlar.txt VEYA cat /home/anil/deneme.txt\n");
        return;
    }

    const char* path = argv[1];
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

    // Geçici bir okuma tamponu (Buffer) oluşturuyoruz (Maksimum 4KB)
    static uint8_t cat_buf[4096]; 
    uint32_t read_size = 0;

    // 2. Akıllıca oluşturulan target_path üzerinden okuma yapıyoruz
    if (kvxfs_read_all(target_path, cat_buf, sizeof(cat_buf) - 1, &read_size)) {
        cat_buf[read_size] = '\0'; // String sonlandırıcı ekle
        printk("%s\n", (const char*)cat_buf);
    } else {
        printk("Hata: Dosya okunamadı veya bulunamadı: %s\n", target_path);
    }
}

REGISTER_COMMAND(cat, cmd_cat, "Displays file content");
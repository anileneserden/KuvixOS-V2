#include <lib/commands.h>
#include <kernel/user.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_passwd(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: passwd <yeni_sifre>\n");
        return;
    }

    const char* username = user_get_current_name();
    char config_line[64];

    // Format: "anil:yeni_sifre"
    strncpy(config_line, username, 31);
    strcat(config_line, ":");
    strcat(config_line, argv[1]);

    // Senin VFS yapına göre pointer kullanmalıyız
    vfs_file_t* f = 0;
    
    // VFS_O_WRONLY | VFS_O_CREAT: Yazma modu ve dosya yoksa oluştur
    // Senin sisteminde vfs_open: (path, flags, vfs_file_t**) şeklinde çalışıyor
    if (vfs_open("/persist/system/config/users.cfg", VFS_O_WRONLY | VFS_O_CREAT, &f)) {
        uint32_t written = 0;
        
        // vfs_write(pointer, data, size, &written_out)
        int res = vfs_write(f, (const uint8_t*)config_line, strlen(config_line), &written);
        
        // vfs_close(pointer)
        vfs_close(f);
        
        if (res) {
            printk("Sifre basariyla güncellendi.\n");
        } else {
            printk("Hata: Dosyaya yazilamadi!\n");
        }
    } else {
        printk("Hata: users.cfg acilamadi!\n");
    }
}

REGISTER_COMMAND(passwd, cmd_passwd, "Mevcut kullanicinin sifresini degistirir");
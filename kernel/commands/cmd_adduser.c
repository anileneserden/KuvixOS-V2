#include <lib/commands.h>
#include <kernel/user.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_adduser(int argc, char** argv) {
    if (argc < 3) {
        printk("Kullanim: adduser <kullanici_adi> <sifre>\n");
        return;
    }

    // Yetki Kontrolü
    const char* current = user_get_current_name();
    if (strcmp(current, "root") != 0 && strcmp(current, "anil") != 0) {
        printk("Hata: Yetkiniz yok!\n");
        return;
    }

    const char* new_user = argv[1];
    const char* new_pass = argv[2];
    char config_line[128];
    char home_path[64];

    // 1. sprintf yerine strcpy/strcat ile config_line oluşturma
    // Format: "user:pass\n"
    strcpy(config_line, new_user);
    strcat(config_line, ":");
    strcat(config_line, new_pass);
    strcat(config_line, "\n");

    // 2. sprintf yerine strcpy/strcat ile home_path oluşturma
    // Format: "/home/user"
    strcpy(home_path, "/home/");
    strcat(home_path, new_user);

    // 3. Ev dizini oluştur
    if (vfs_mkdir(home_path)) {
        printk("Ev dizini olusturuldu: %s\n", home_path);
    } else {
        printk("Uyari: Ev dizini olusturulamadi.\n");
    }

    // 4. Dosyaya yaz (vfs_write_all senin vfs.c içinde vardı)
    if (vfs_write_all("/persist/system/config/users.cfg", (const uint8_t*)config_line, strlen(config_line))) {
        printk("Kullanici '%s' basariyla eklendi.\n", new_user);
    } else {
        printk("Hata: users.cfg yazilamadi!\n");
    }
}

REGISTER_COMMAND(adduser, cmd_adduser, "Sisteme yeni bir kullanici ekler");
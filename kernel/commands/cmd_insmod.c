#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <stdint.h>

// loader.c içerisinde tanımlı olan sürücü yükleme fonksiyonunun prototipi
void load_driver_module(const char* filepath);

static void cmd_insmod(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("kullanim: insmod <surucu_yolu.kdf>\n");
        return;
    }

    const char* raw_path = argv[1];
    char target_path[VFS_PATH_MAX] = {0};

    // Yol mutlak mı (örn: /sys/drivers/...) yoksa göreceli mi kontrol edelim
    if (raw_path[0] == '/') {
        strncpy(target_path, raw_path, VFS_PATH_MAX - 1);
    } else {
        const char* current_cwd = vfs_get_cwd();
        if (!current_cwd) current_cwd = "/";
        strncpy(target_path, current_cwd, VFS_PATH_MAX - 1);
        size_t len = strlen(target_path);
        if (len > 0 && target_path[len - 1] != '/') {
            strcat(target_path, "/");
        }
        strcat(target_path, raw_path);
    }

    commands_puts("[insmod] Surucu yukleniyor: ");
    commands_puts(target_path);
    commands_puts("\n");

    // Loader mekanizmamızı çağırarak sürücüyü belleğe alıp başlatıyoruz
    load_driver_module(target_path);
}

REGISTER_COMMAND(insmod, cmd_insmod, "insmod <surucu_yolu.kdf> - Bir .kdf formatindaki surucuyu yukler");
#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanım: touch <dosya_yolu>\n");
        return;
    }

    char resolved[VFS_PATH_MAX];
    const char* input = argv[1];

    if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
        commands_puts("Hata: Yol cozumlenemedi.\n");
        return;
    }

    uint8_t empty_data = 0;
    
    if (kvxfs_write_all(resolved, &empty_data, 0)) {
        commands_printf("Dosya olusturuldu: %s\n", resolved);
    } else {
        commands_puts("Hata: Dosya olusturulamadi!\n");
    }
}

REGISTER_COMMAND(touch, cmd_touch, "Yeni bir bos dosya olusturur");
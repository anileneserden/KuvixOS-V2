#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: rm <dosya/dizin>\n");
        return;
    }

    // Kullanıcıya bilgi ver
    commands_printf("Siliniyor: %s...\n", argv[1]);

    // VFS üzerinden silme işlemini başlat
    // vfs_remove başarılıysa 1, başarısızsa 0 döner (VFS yapına göre)
    if (vfs_remove(argv[1])) {
        commands_puts("Basariyla silindi.\n");
    } else {
        commands_puts("Hata: Dosya silinemedi veya bulunamadi!\n");
    }
}

REGISTER_COMMAND(rm, cmd_rm, "Dosya veya dizini siler");
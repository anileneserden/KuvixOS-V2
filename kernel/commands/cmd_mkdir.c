#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: mkdir <dizin>\n");
        commands_puts("Ornek: mkdir /persist/test\n");
        return;
    }

    char resolved[VFS_PATH_MAX];
    if (!vfs_resolve_path(argv[1], resolved, sizeof(resolved))) {
        commands_puts("Hata: yol cozumlenemedi.\n");
        return;
    }

    if (vfs_mkdir(resolved)) {
        commands_printf("Dizin oluşturuldu: %s\n", resolved);
    } else {
        commands_printf("Hata: Dizin oluşturulamadı (KVXFS Hatası): %s\n", resolved);
    }
}

REGISTER_COMMAND(mkdir, cmd_mkdir, "Dizin oluşturur");
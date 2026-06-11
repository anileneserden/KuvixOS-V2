#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: rm <dosya/dizin>\n");
        commands_puts("Ornek: rm /test2/a.txt\n");
        return;
    }

    const char* path = argv[1];

    if (!path || !path[0]) {
        commands_puts("Hata: gecersiz yol.\n");
        return;
    }

    if (strcmp(path, "/") == 0 ||
        strcmp(path, "/persist") == 0 ||
        strcmp(path, "/") == 0 ||
        strcmp(path, "/fat") == 0 ||
        strcmp(path, "/tmp") == 0) {
        commands_puts("Hata: bu yol silinemez.\n");
        return;
    }

    commands_printf("Siliniyor: %s\n", path);

    if (vfs_remove(path)) {
        commands_puts("Silindi.\n");
    } else {
        commands_puts("Hata: dosya/dizin silinemedi.\n");
    }
}

REGISTER_COMMAND(rm, cmd_rm, "Dosya veya dizini siler");
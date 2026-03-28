#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/fat.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_ls(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";

    if (strncmp(path, "/persist", 8) == 0) {
        kvxfs_list_all(path);
    } else if (strcmp(path, "/fat") == 0 || strcmp(path, "/fat/") == 0) {
        if (!fat_list_root_cmd()) {
            commands_puts("Hata: FAT root listelenemedi.\n");
        }
    } else {
        vfs_list(path, (void*)0, (void*)0);
    }
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin içeriğini listeler");
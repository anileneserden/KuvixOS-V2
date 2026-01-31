#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_ls(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";

    if (strncmp(path, "/persist", 8) == 0) {
        kvxfs_list_all(path);
    } else {
        // Standart VFS listeleme (Ramfs için)
        vfs_list(path, (void*)0, (void*)0);
    }
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");
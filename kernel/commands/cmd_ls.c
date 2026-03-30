#include <kernel/fs/vfs.h>
#include <kernel/fs/kvxfs.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_ls(int argc, char** argv) {
    const char* input = (argc > 1) ? argv[1] : "";
    char resolved[VFS_PATH_MAX];

    if (!input[0]) {
        strncpy(resolved, vfs_get_cwd(), sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = 0;
    } else {
        if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
            commands_puts("Hata: yol cozumlenemedi.\n");
            return;
        }
    }

    if (strncmp(resolved, "/persist", 8) == 0) {
        kvxfs_list_all(resolved);
    } else {
        vfs_list(resolved, (void*)0, (void*)0);
    }
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");
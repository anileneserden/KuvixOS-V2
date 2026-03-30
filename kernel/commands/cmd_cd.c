#include <kernel/fs/vfs.h>
#include <lib/commands.h>

void cmd_cd(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";

    if (vfs_set_cwd(path)) {
        return;
    }

    commands_printf("Hata: dizin degistirilemedi: %s\n", path);
}

REGISTER_COMMAND(cd, cmd_cd, "Calisma dizinini degistirir");
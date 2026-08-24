#include <lib/commands.h>
#include <stddef.h>

extern void load_user_module(const char* path);

void cmd_kde(int argc, char** argv) {
    const char* filepath = "/sys/de/desktopIconsLoad.kde";
    if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
        filepath = argv[1];
    }
    load_user_module(filepath);
}

REGISTER_COMMAND(kde, cmd_kde, "Starts KuvixOS Desktop Environment");
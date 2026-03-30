#include <kernel/fs/kvxfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>

void cmd_tree(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/persist";

    if (strncmp(path, "/persist", 8) == 0) {
        if (!kvxfs_tree(path)) {
            printk("Hata: tree basarili olmadi.\n");
        }
        return;
    }

    printk("Su an sadece /persist icin tree destekleniyor.\n");
}

REGISTER_COMMAND(tree, cmd_tree, "Dizin agacini gosterir");
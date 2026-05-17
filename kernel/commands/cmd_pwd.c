#include <kernel/printk.h>
#include <kernel/fs/vfs.h>
#include <lib/commands.h>

void cmd_pwd(int argc, char** argv) {
    (void)argc; (void)argv;
    // Mevcut çalışma dizinini al ve yazdır
    printk("%s\n", vfs_get_cwd());
}

REGISTER_COMMAND(pwd, cmd_pwd, "Mevcut çalışma dizinini gösterir");

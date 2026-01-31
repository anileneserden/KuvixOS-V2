#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_cd(int argc, char** argv) {
    (void)argc; (void)argv; // İkisini de sustur
    printk("CD komutu yakinda VFS'ye uyarlanacak...\n");
}

REGISTER_COMMAND(cd, cmd_cd, "Calisma dizinini degistirir");
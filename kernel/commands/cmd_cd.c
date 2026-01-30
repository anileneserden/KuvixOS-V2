#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: cd <dizin>\n");
        return;
    }
    printk("Dizin degistirme (cd) yakinda eklenecek.\n");
}

REGISTER_COMMAND(cd, cmd_cd, "Calisma dizinini degistirir");